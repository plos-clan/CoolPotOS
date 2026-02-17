#include "exec/elf.h"
#include "krlibc.h"
#include "limine.h"
#include "mem/heap.h"
#include "ptrace.h"
#include "task/task.h"
#include "term/klog.h"

typedef struct {
    const char *name;
    uint64_t addr;
} ksym_t;

LIMINE_REQUEST struct limine_kernel_file_request kfile_request = {
    .id = LIMINE_KERNEL_FILE_REQUEST,
};
extern char _kernel_start[];
ksym_t *kallsyms                  = NULL;
size_t kallsyms_num               = 0;
void *eh_frame_start              = NULL;
size_t eh_frame_size              = 0;
static uint64_t kernel_text_start = 0;
static uint64_t kernel_text_end   = 0;

const char *kallsyms_lookup(uint64_t addr, uint64_t *sym_addr);

static bool addr_in_kernel_text(uint64_t addr) {
    if (kernel_text_start && kernel_text_end) {
        return addr >= kernel_text_start && addr < kernel_text_end;
    }
    return addr >= 0xffffffff80000000UL && addr < 0xfffffffffffff000UL;
}

static void get_stack_bounds(uint64_t rsp, uint64_t *stack_low, uint64_t *stack_high) {
    tcb_t task = get_current_task();
    if (task != NULL) {
        uint64_t k_low  = (uint64_t)task;
        uint64_t k_high = task->context.kernel_stack;
        if (k_high == 0) {
            k_high = k_low + STACK_SIZE;
        }
        if (rsp >= k_low && rsp < k_high) {
            *stack_low  = k_low;
            *stack_high = k_high;
            return;
        }

        if (task->syscall_stack != 0) {
            uint64_t s_high = task->syscall_stack;
            uint64_t s_low  = s_high - MAX_STACK_SIZE;
            if (rsp >= s_low && rsp < s_high) {
                *stack_low  = s_low;
                *stack_high = s_high;
                return;
            }
        }

        if (task->signal_stack != 0) {
            uint64_t s_high = task->signal_stack;
            uint64_t s_low  = s_high - STACK_SIZE;
            if (rsp >= s_low && rsp < s_high) {
                *stack_low  = s_low;
                *stack_high = s_high;
                return;
            }
        }
    }

    *stack_low  = rsp;
    *stack_high = rsp + MAX_STACK_SIZE;
}

static int
backtrace_from_rbp(uint64_t rbp, uint64_t stack_low, uint64_t stack_high, int max_frames) {
    int count         = 0;
    uint64_t sym_addr = 0;

    while (count < max_frames) {
        if (rbp < stack_low || rbp + 16 > stack_high) {
            break;
        }

        uint64_t next_rbp = *(uint64_t *)rbp;
        uint64_t ret_addr = *((uint64_t *)rbp + 1);

        if (!addr_in_kernel_text(ret_addr)) {
            break;
        }

        const char *name = kallsyms_lookup(ret_addr, &sym_addr);
        if (name) {
            printk("  [<0x%lx>] %s+0x%lx\n", ret_addr, name, ret_addr - sym_addr);
        } else {
            printk("  [<0x%lx>] ???\n", ret_addr);
        }
        count++;

        if (next_rbp <= rbp) {
            break;
        }
        rbp = next_rbp;
    }

    return count;
}

static int ksym_cmp(const void *a, const void *b) {
    const ksym_t *sym_a = a;
    const ksym_t *sym_b = b;

    if (sym_a->addr < sym_b->addr)
        return -1;
    if (sym_a->addr > sym_b->addr)
        return 1;
    return 0;
}

void sort_kallsyms(void) {
    if (kallsyms && kallsyms_num > 0) {
        qsort(kallsyms, kallsyms_num, sizeof(ksym_t), ksym_cmp);
    }
}

void kallsyms_init_from_elf() {
    struct limine_kernel_file_response *response = kfile_request.response;
    Elf64_Ehdr *ehdr                             = (Elf64_Ehdr *)response->kernel_file->address;
    if (ehdr->e_ident[0] != 0x7f || memcmp(ehdr->e_ident + 1, "ELF", 3) != 0) {
        return;
    }
    Elf64_Sym *symtab = NULL;
    char *strtab      = NULL;

    Elf64_Shdr *shdrs = (Elf64_Shdr *)((char *)ehdr + ehdr->e_shoff);
    char *shstrtab    = (char *)ehdr + shdrs[ehdr->e_shstrndx].sh_offset;

    size_t symtabsz = 0;

    bool has_sym = false;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        switch (shdrs[i].sh_type) {
        case SHT_SYMTAB:
            if (has_sym)
                break;
            symtab   = (Elf64_Sym *)((char *)ehdr + shdrs[i].sh_offset);
            symtabsz = shdrs[i].sh_size;
            strtab   = (char *)ehdr + shdrs[shdrs[i].sh_link].sh_offset;
            has_sym  = true;
            break;
        case SHT_PROGBITS:
            if (shdrs[i].sh_name >= shdrs[ehdr->e_shstrndx].sh_size) {
                break;
            }
            const char *sec_name = shstrtab + shdrs[i].sh_name;
            if (strcmp(sec_name, ".eh_frame") == 0) {
                eh_frame_start = (void *)((char *)ehdr + shdrs[i].sh_offset);
                eh_frame_size  = shdrs[i].sh_size;
                break;
            }
            if (strcmp(sec_name, ".text") == 0) {
                kernel_text_start = shdrs[i].sh_addr;
                kernel_text_end   = shdrs[i].sh_addr + shdrs[i].sh_size;
                break;
            }
        default:
            break;
        }
    }

    const size_t num_symbols = symtabsz / sizeof(Elf64_Sym);

    kallsyms = calloc(num_symbols, sizeof(ksym_t));

    for (size_t i = 0; i < num_symbols; i++) {
        Elf64_Sym *sym = &symtab[i];
        char *sym_name = &strtab[sym->st_name];
        uint64_t type  = ELF64_ST_TYPE(sym->st_info);
        if (sym->st_shndx == SHN_UNDEF)
            continue;
        if (type == STT_FUNC) {
            kallsyms[kallsyms_num].addr = (uint64_t)(sym->st_value);
            kallsyms[kallsyms_num].name = sym_name;
            kallsyms_num++;
        }
    }
    sort_kallsyms();
    kinfo("eh_frame: %p sz=%llu symnum=%llu", eh_frame_start, eh_frame_start, kallsyms_num);
}

const char *kallsyms_lookup(uint64_t addr, uint64_t *sym_addr) {
    if (!kallsyms)
        return NULL;

    for (size_t i = 0; i < kallsyms_num; i++) {
        if (addr >= kallsyms[i].addr) {
            if (i + 1 < kallsyms_num) {
                if (addr < kallsyms[i + 1].addr) {
                    if (sym_addr)
                        *sym_addr = kallsyms[i].addr;
                    return kallsyms[i].name;
                }
            } else {
                if (sym_addr)
                    *sym_addr = kallsyms[i].addr;
                return kallsyms[i].name;
            }
        }
    }
    return NULL;
}

void print_kernel_backtrace(struct interrupt_frame *frame, uint64_t saved_rbp) {
    printk("Call Trace:\n");

    uint64_t sym_addr = 0;
    const char *name  = kallsyms_lookup(frame->rip, &sym_addr);
    if (name) {
        printk("  [<0x%lx>] %s+0x%lx (RIP)\n", frame->rip, name, frame->rip - sym_addr);
    } else {
        printk("  [<0x%lx>] ??? (RIP)\n", frame->rip);
    }

    const int max_frames = 20;
    int count            = 0;

    uint64_t stack_low  = 0;
    uint64_t stack_high = 0;
    get_stack_bounds(frame->rsp, &stack_low, &stack_high);

    if (saved_rbp >= stack_low && saved_rbp + 16 <= stack_high) {
        uint64_t rbp = 0;
        uint64_t ret = *((uint64_t *)saved_rbp + 1);
        if (ret == frame->rip) {
            rbp = *(uint64_t *)saved_rbp; // handler 栈帧，取中断前的 RBP
        } else {
            rbp = saved_rbp; // saved_rbp 本身就是中断前的 RBP
        }
        if (rbp >= stack_low && rbp + 16 <= stack_high) {
            count = backtrace_from_rbp(rbp, stack_low, stack_high, max_frames);
        }
    }

    if (count == 0) {
        uint64_t start_rsp = frame->rsp;
        if (start_rsp < stack_low || start_rsp >= stack_high) {
            start_rsp = stack_low;
        }

        for (uint64_t *p = (uint64_t *)start_rsp;
             (uint64_t)p + sizeof(uint64_t) <= stack_high && count < max_frames; p++) {
            const uint64_t candidate = *p;

            if (!addr_in_kernel_text(candidate)) {
                continue;
            }

            const char *name = kallsyms_lookup(candidate, &sym_addr);
            if (name) {
                printk("  [<0x%lx>] %s+0x%lx\n", candidate, name, candidate - sym_addr);
            } else {
                printk("  [<0x%lx>] ???\n", candidate);
            }
            count++;
        }
    }

    if (count == 0) {
        printk("  (no valid return addresses found)\n");
    }
}
