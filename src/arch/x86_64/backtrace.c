#include "description_table.h"
#include "exec/elf.h"
#include "mem/heap.h"
#include "krlibc.h"
#include "limine.h"
#include "ptrace.h"
#include "term/klog.h"

typedef struct {
    const char *name;
    uint64_t    addr;
} ksym_t;

LIMINE_REQUEST struct limine_kernel_file_request kfile_request = {
    .id = LIMINE_KERNEL_FILE_REQUEST,
};
extern char   _kernel_start[];
static ksym_t static_ksyms[8192];
ksym_t       *kallsyms     = NULL;
size_t        kallsyms_num = 0;

static int ksym_cmp(const void *a, const void *b) {
    const ksym_t *sym_a = (const ksym_t *)a;
    const ksym_t *sym_b = (const ksym_t *)b;

    if (sym_a->addr < sym_b->addr) return -1;
    if (sym_a->addr > sym_b->addr) return 1;
    return 0;
}

void sort_kallsyms(void) {
    if (kallsyms && kallsyms_num > 0) {
        qsort(kallsyms, kallsyms_num, sizeof(ksym_t), ksym_cmp);
    }
}

void kallsyms_init_from_elf() {
    struct limine_kernel_file_response *response = kfile_request.response;
    Elf64_Ehdr                         *ehdr     = (Elf64_Ehdr *)response->kernel_file->address;
    if (ehdr->e_ident[0] != 0x7f || memcmp(ehdr->e_ident + 1, "ELF", 3) != 0) { return; }
    Elf64_Sym *symtab = NULL;
    char      *strtab = NULL;

    Elf64_Shdr *shdrs    = (Elf64_Shdr *)((char *)ehdr + ehdr->e_shoff);
    char       *shstrtab = (char *)ehdr + shdrs[ehdr->e_shstrndx].sh_offset;

    size_t symtabsz = 0;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdrs[i].sh_type == SHT_SYMTAB) {
            symtab   = (Elf64_Sym *)((char *)ehdr + shdrs[i].sh_offset);
            symtabsz = shdrs[i].sh_size;
            strtab   = (char *)ehdr + shdrs[shdrs[i].sh_link].sh_offset;
            break;
        }
    }

    size_t num_symbols = symtabsz / sizeof(Elf64_Sym);

    kallsyms = calloc(num_symbols, sizeof(ksym_t));

    for (size_t i = 0; i < num_symbols; i++) {
        Elf64_Sym *sym      = &symtab[i];
        char      *sym_name = &strtab[sym->st_name];
        uint64_t   bind     = ELF64_ST_BIND(sym->st_info);
        uint64_t   type     = ELF64_ST_TYPE(sym->st_info);
        if (sym->st_shndx == SHN_UNDEF) continue;
        if (type == STT_FUNC) {
            kallsyms[kallsyms_num].addr = (uint64_t)(sym->st_value);
            kallsyms[kallsyms_num].name = sym_name;
            kallsyms_num++;
        }
    }
    sort_kallsyms();
}

const char *kallsyms_lookup(uint64_t addr, uint64_t *sym_addr) {
    if (!kallsyms) return NULL;

    for (size_t i = 0; i < kallsyms_num; i++) {
        if (addr >= kallsyms[i].addr) {
            if (i + 1 < kallsyms_num) {
                if (addr < kallsyms[i + 1].addr) {
                    if (sym_addr) *sym_addr = kallsyms[i].addr;
                    return kallsyms[i].name;
                }
            } else {
                if (sym_addr) *sym_addr = kallsyms[i].addr;
                return kallsyms[i].name;
            }
        }
    }
    return NULL;
}

void print_kernel_backtrace(struct interrupt_frame *frame) {
    printk("Call Trace (stack scanning):\n");

    uint64_t    sym_addr = 0;
    const char *name     = kallsyms_lookup(frame->rip, &sym_addr);
    if (name) {
        printk("  [<0x%lx>] %s+0x%lx (RIP)\n", frame->rip, name, frame->rip - sym_addr);
    } else {
        printk("  [<0x%lx>] ??? (RIP)\n", frame->rip);
    }

    // 从 RSP 开始向上扫描栈（限制范围防止越界）
    uint64_t      *stack      = (uint64_t *)frame->rsp;
    const uint64_t stack_top  = frame->rsp + 0x2000; // 扫描最多 8KB
    int            count      = 0;
    const int      max_frames = 20;

    for (uint64_t *p = stack; p < (uint64_t *)stack_top && count < max_frames; p++) {
        uint64_t candidate = *p;

        // 合法内核地址范围（根据你的链接脚本）
        if (candidate >= 0xffffffff80000000UL && candidate < 0xfffffffffffff000UL) {
            // 检查是否对齐（指令地址通常是 16 字节对齐）
            if ((candidate & 0xF) == 0) {
                const char *name = kallsyms_lookup(candidate, &sym_addr);
                if (name) {
                    printk("  [<0x%lx>] %s+0x%lx\n", candidate, name, candidate - sym_addr);
                } else {
                    printk("  [<0x%lx>] ???\n", candidate);
                }
                count++;
            }
        }
    }

    if (count == 0) { printk("  (no valid return addresses found on stack)\n"); }
}
