#include "page.h"
#include "frame.h"
#include "hhdm.h"
#include "io.h"
#include "isr.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "lock.h"
#include "pcb.h"
#include "smp.h"
#include "terminal.h"

spin_t page_lock;

page_directory_t  kernel_page_dir;
page_directory_t *current_directory = NULL;

uint64_t double_fault_page = 0;

static bool is_huge_page(page_table_entry_t *entry) {
    return (((uint64_t)entry->value) & PTE_HUGE) != 0;
}

__IRQHANDLER static void page_fault_handle(interrupt_frame_t *frame, uint64_t error_code) {
    close_interrupt;
    init_print_lock();
    disable_scheduler();
    terminal_close_flush();
    uint64_t faulting_address;
    __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_address));

    char *error_msg = !(error_code & 0x1) ? "NotPresent"
                      : error_code & 0x2  ? "WriteError"
                      : error_code & 0x4  ? "UserMode"
                      : error_code & 0x8  ? "ReservedBitsSet"
                      : error_code & 0x10 ? "DecodeAddress"
                                          : "Unknown";

    if (get_current_task() != NULL) {
        pcb_t current_proc = get_current_task()->parent_group;

        // 应用程序懒分配机制
        if (current_proc->task_level == TASK_APPLICATION_LEVEL) {
            //            logkf("Lazy alloc try(%p) %s Current process PID: %d:%s (%s) CPU%d\n", faulting_address,
            //                  error_msg, get_current_task()->tid, get_current_task()->name, current_proc->name,
            //                  get_current_task()->cpu_id);
            mm_virtual_page_t *virt_page = NULL;
            spin_lock(current_proc->virt_queue->lock);
            queue_foreach(current_proc->virt_queue, node) {
                mm_virtual_page_t *vpage = (mm_virtual_page_t *)node->data;
                if (faulting_address >= vpage->start &&
                    faulting_address < vpage->start + vpage->count * PAGE_SIZE) {
                    virt_page = vpage;
                    break;
                }
            }

            if (virt_page == NULL) {
                spin_unlock(current_proc->virt_queue->lock);
                logkf("Page fault virtual address 0x%x %p\n", faulting_address, frame->rip);
                logkf("Type: %s\n", error_msg);
                kill_proc(get_current_task()->parent_group, -1);
                terminal_open_flush();
                enable_scheduler();
                update_terminal();
                open_interrupt;
                cpu_hlt;
            } else {
                size_t   fault_index = (faulting_address - virt_page->start) / PAGE_SIZE;
                uint64_t page_addr   = virt_page->start + fault_index * PAGE_SIZE;

                // logkf("lazy alloc %p\n", page_addr);

                uint64_t phys = alloc_frames(1);
                page_map_to(get_current_directory(), page_addr, phys, virt_page->pte_flags);
                memset((void *)page_addr, 0, PAGE_SIZE);

                uint64_t range_start = virt_page->start;
                // uint64_t range_end = range_start + virt_page->count * PAGE_SIZE;

                mm_virtual_page_t *left  = NULL;
                mm_virtual_page_t *right = NULL;

                spin_unlock(current_proc->virt_queue->lock);

                // 左侧未分配部分
                if (fault_index > 0) {
                    left            = malloc(sizeof(mm_virtual_page_t));
                    left->start     = range_start;
                    left->count     = fault_index;
                    left->pte_flags = virt_page->pte_flags;
                    left->index     = queue_enqueue(current_proc->virt_queue, left);
                }

                // 右侧未分配部分
                if (fault_index + 1 < virt_page->count) {
                    right            = malloc(sizeof(mm_virtual_page_t));
                    right->start     = range_start + (fault_index + 1) * PAGE_SIZE;
                    right->count     = virt_page->count - fault_index - 1;
                    right->pte_flags = virt_page->pte_flags;
                    right->index     = queue_enqueue(current_proc->virt_queue, right);
                }

                // 移除原始 mm_virtual_page
                queue_remove_at(current_proc->virt_queue, virt_page->index);
                free(virt_page);
                terminal_open_flush();
                enable_scheduler();
                open_interrupt;
                update_terminal();
                return;
            }
        }
    }
    logkf("Page fault virtual address 0x%x %p\n", faulting_address, frame->rip);
    logkf("Type: %s\n", error_msg);

    printk("\n");
    printk(
        "\033[31m:3 Your CP_Kernel ran into a problem.\nERROR CODE >(PageFault%s:0x%p)<\033[0m\n",
        error_msg, faulting_address);
    if (get_current_task() != NULL) {
        printk("Current process PID: %d:%s (%s) at CPU%d\n", get_current_task()->tid,
               get_current_task()->name, get_current_task()->parent_group->name, cpu->id);
    }
    print_register(frame);
    terminal_flush();
    update_terminal();
    terminal_open_flush();
    cpu_hlt;
}

void page_table_clear(page_table_t *table) {
    for (int i = 0; i < 512; i++) {
        table->entries[i].value = 0;
    }
}

bool page_table_get_flags(page_directory_t *directory, uint64_t addr, uint64_t *out_flags) {
    uint64_t l4_index = (((addr >> 39)) & 0x1FF);
    uint64_t l3_index = (((addr >> 30)) & 0x1FF);
    uint64_t l2_index = (((addr >> 21)) & 0x1FF);
    uint64_t l1_index = (((addr >> 12)) & 0x1FF);

    page_table_t *pml4 = directory->table;

    if (!(pml4->entries[l4_index].value & PTE_PRESENT)) return false;
    if (pml4->entries[l4_index].value & PTE_HUGE) {
        *out_flags = pml4->entries[l4_index].value & 0xFFF;
        return true;
    }

    page_table_t *pdpt = (page_table_t *)(pml4->entries[l4_index].value & PAGE_MASK);
    if (!(pdpt->entries[l3_index].value & PTE_PRESENT)) return false;
    if (pdpt->entries[l3_index].value & PTE_HUGE) {
        *out_flags = pdpt->entries[l3_index].value & 0xFFF;
        return true;
    }

    page_table_t *pd = (page_table_t *)(pdpt->entries[l3_index].value & PAGE_MASK);
    if (!(pd->entries[l2_index].value & PTE_PRESENT)) return false;
    if (pd->entries[l2_index].value & PTE_HUGE) {
        *out_flags = pd->entries[l2_index].value & 0xFFF;
        return true;
    }

    page_table_t *pt = (page_table_t *)(pd->entries[l2_index].value & PAGE_MASK);
    if (!(pt->entries[l1_index].value & PTE_PRESENT)) return false;

    *out_flags = pt->entries[l1_index].value & 0xFFF;
    return true;
}

bool page_table_update_flags(page_directory_t *directory, uint64_t addr, uint64_t new_flags) {
    page_table_t *pml4     = directory->table;
    uint64_t      l4_index = (((addr >> 39)) & 0x1FF);
    uint64_t      l3_index = (((addr >> 30)) & 0x1FF);
    uint64_t      l2_index = (((addr >> 21)) & 0x1FF);
    uint64_t      l1_index = (((addr >> 12)) & 0x1FF);

    if (!(pml4->entries[l4_index].value & PTE_PRESENT)) return false;
    if (pml4->entries[l4_index].value & PTE_HUGE) {
        uint64_t phys                 = pml4->entries[l4_index].value & PAGE_MASK;
        pml4->entries[l4_index].value = phys | (new_flags & 0xFFF);
        return true;
    }

    page_table_t *pdpt = (page_table_t *)(pml4->entries[l4_index].value & PAGE_MASK);
    if (!(pdpt->entries[l3_index].value & PTE_PRESENT)) return false;
    if (pdpt->entries[l3_index].value & PTE_HUGE) {
        uint64_t phys                 = pdpt->entries[l3_index].value & PAGE_MASK;
        pdpt->entries[l3_index].value = phys | (new_flags & 0xFFF);
        return true;
    }

    page_table_t *pd = (page_table_t *)(pdpt->entries[l3_index].value & PAGE_MASK);
    if (!(pd->entries[l2_index].value & PTE_PRESENT)) return false;
    if (pd->entries[l2_index].value & PTE_HUGE) {
        uint64_t phys               = pd->entries[l2_index].value & PAGE_MASK;
        pd->entries[l2_index].value = phys | (new_flags & 0xFFF);
        return true;
    }

    page_table_t *pt = (page_table_t *)(pd->entries[l2_index].value & PAGE_MASK);
    if (!(pt->entries[l1_index].value & PTE_PRESENT)) return false;

    uint64_t phys               = pt->entries[l1_index].value & PAGE_MASK;
    pt->entries[l1_index].value = phys | (new_flags & 0xFFF);

    __asm__ volatile("invlpg (%0)" ::"r"(addr) : "memory");
    return true;
}

page_table_t *page_table_create(page_table_entry_t *entry) {
    if (entry->value == (uint64_t)NULL) {
        uint64_t frame      = alloc_frames(1);
        entry->value        = frame | PTE_PRESENT | PTE_WRITEABLE | PTE_USER;
        page_table_t *table = (page_table_t *)phys_to_virt(entry->value & 0x000fffffffff000);
        page_table_clear(table);
        return table;
    }
    page_table_t *table = (page_table_t *)phys_to_virt(entry->value & 0x000fffffffff000);
    return table;
}

page_directory_t *get_kernel_pagedir() {
    return &kernel_page_dir;
}

page_directory_t *get_current_directory() {
    return cpu->ready ? cpu->directory : current_directory;
}

static page_table_t *copy_page_table_recursive(page_table_t *source_table, int level, bool all_copy,
                                               bool kernel_space) {
    if (source_table == NULL) return NULL;
    if (level == 0) {
        if (kernel_space) {
            // If we are copying the kernel space, we can just return the same table
            return source_table;
        }

        uint64_t      frame          = alloc_frames(1);
        page_table_t *new_page_table = (page_table_t *)phys_to_virt(frame);
        memcpy(new_page_table, source_table->entries, PAGE_SIZE);
        return new_page_table;
    }

    uint64_t      phy_frame = alloc_frames(1);
    page_table_t *new_table = phys_to_virt(phy_frame);
    for (uint64_t i = 0; i < (all_copy ? 512 : (level == 4 ? 256 : 512)); i++) {
        if (source_table->entries[i].value & PTE_HUGE) {
            new_table->entries[i].value = source_table->entries[i].value;
            continue;
        }

        page_table_t *source_page_table_next =
            phys_to_virt(source_table->entries[i].value & 0x000fffffffff000);
        page_table_t *new_page_table = copy_page_table_recursive(
            source_page_table_next, level - 1, all_copy, level != 4 ? kernel_space : i >= 256);
        new_table->entries[i].value = (uint64_t)virt_to_phys((uint64_t)new_page_table) |
                                      (source_table->entries[i].value & 0xFFFF000000000FFF);
    }
    return new_table;
}

static void free_page_table_recursive(page_table_t *table, int level) {
    if (table == NULL) return;
    if (level == 0) {
        free_frame((uint64_t)virt_to_phys((uint64_t)table));
        return;
    }

    for (int i = 0; i < (level == 4 ? 256 : 512); i++) {
        page_table_t *page_table_next = phys_to_virt(table->entries[i].value & 0x000fffffffff000);
        free_page_table_recursive(page_table_next, level - 1);
    }
    free_frame((uint64_t)virt_to_phys((uint64_t)table));
}

page_directory_t *clone_page_directory(page_directory_t *dir, bool all_copy) {
    spin_lock(page_lock);
    page_directory_t *new_directory = malloc(sizeof(page_directory_t));
    if (new_directory == NULL) return NULL;
    new_directory->table = copy_page_table_recursive(dir->table, 4, all_copy, false);
    if (!all_copy)
        memcpy((uint64_t *)new_directory->table + 256, (uint64_t *)dir->table + 256, PAGE_SIZE / 2);
    spin_unlock(page_lock);
    return new_directory;
}

void free_page_directory(page_directory_t *dir) {
    spin_lock(page_lock);
    free_page_table_recursive(dir->table, 4);
    free(dir);
    spin_unlock(page_lock);
}

void unmap_page(page_directory_t *directory, uint64_t vaddr) {
    uint64_t l4_index = (((vaddr >> 39)) & 0x1FF);
    uint64_t l3_index = (((vaddr >> 30)) & 0x1FF);
    uint64_t l2_index = (((vaddr >> 21)) & 0x1FF);
    uint64_t l1_index = (((vaddr >> 12)) & 0x1FF);

    page_table_t *l4_table = directory->table;
    page_table_t *l3_table =
        phys_to_virt((&(l4_table->entries[l4_index]))->value & 0x000fffffffff000);
    page_table_t *l2_table =
        phys_to_virt((&(l3_table->entries[l3_index]))->value & 0x000fffffffff000);
    page_table_t *l1_table =
        phys_to_virt((&(l2_table->entries[l2_index]))->value & 0x000fffffffff000);

    free_frame(l1_table->entries[l1_index].value & 0x000fffffffff000);
    l1_table->entries[l1_index].value = 0;
    flush_tlb(vaddr);
}

void unmap_page_range(page_directory_t *directory, uint64_t vaddr, uint64_t size) {
    for (uint64_t va = vaddr; va < vaddr + size; va += PAGE_SIZE) {
        unmap_page(directory, va);
    }
}

void page_map_to(page_directory_t *directory, uint64_t addr, uint64_t frame, uint64_t flags) {
    uint64_t l4_index = (((addr >> 39)) & 0x1FF);
    uint64_t l3_index = (((addr >> 30)) & 0x1FF);
    uint64_t l2_index = (((addr >> 21)) & 0x1FF);
    uint64_t l1_index = (((addr >> 12)) & 0x1FF);

    page_table_t *l4_table = directory->table;
    page_table_t *l3_table = page_table_create(&(l4_table->entries[l4_index]));
    page_table_t *l2_table = page_table_create(&(l3_table->entries[l3_index]));
    page_table_t *l1_table = page_table_create(&(l2_table->entries[l2_index]));

    l1_table->entries[l1_index].value = (frame & 0x000fffffffff000) | flags;

    flush_tlb(addr);
}

void switch_process_page_directory(page_directory_t *dir) {
    bool is_sti = are_interrupts_enabled();
    close_interrupt;
    pcb_t pcb     = get_current_task()->parent_group;
    pcb->page_dir = dir;
    switch_page_directory(dir);
    if (is_sti) open_interrupt;
}

void switch_page_directory(page_directory_t *dir) {
    if (cpu->ready) {
        cpu->directory = dir;
    } else {
        current_directory = dir;
    }
    page_table_t *physical_table = virt_to_phys((uint64_t)dir->table);
    __asm__ volatile("mov %0, %%cr3" : : "r"(physical_table));
}

uint64_t page_alloc_random(page_directory_t *directory, uint64_t length, uint64_t flags) {
    if (length == 0) return -1;
    size_t   p    = length / PAGE_SIZE;
    uint64_t addr = alloc_frames(p == 0 ? 1 : p);
    for (uint64_t i = 0; i < length; i += 0x1000) {
        uint64_t var = (uint64_t)addr + i;
        page_map_to(directory, var, var, flags);
    }
    return addr;
}

void page_map_range_to_random(page_directory_t *directory, uint64_t addr, uint64_t length,
                              uint64_t flags) {
    for (uint64_t i = 0; i < length; i += 0x1000) {
        uint64_t var = (uint64_t)addr + i;
        page_map_to(directory, var, alloc_frames(1), flags);
    }
}

void page_map_range_to(page_directory_t *directory, uint64_t frame, uint64_t length,
                       uint64_t flags) {
    for (uint64_t i = 0; i < length; i += 0x1000) {
        uint64_t var = (uint64_t)phys_to_virt(frame + i);
        page_map_to(directory, var, frame + i, flags);
    }
}

void page_map_range(page_directory_t *directory, uint64_t addr, uint64_t frame, uint64_t length,
                    uint64_t flags) {
    for (uint64_t i = 0; i < length; i += 0x1000) {
        uint64_t var = (uint64_t)addr + i;
        page_map_to(directory, var, frame + i, flags);
    }
}

void switch_cp_kernel_page_directory() {
    page_directory_t *new_directory = clone_page_directory(&kernel_page_dir, true);
    kernel_page_dir.table           = new_directory->table;
    free(new_directory);
    switch_page_directory(&kernel_page_dir);
    double_fault_page = get_cr3();
    current_directory = &kernel_page_dir;
}

void page_setup() {
    page_table_t *kernel_page_table = (page_table_t *)phys_to_virt(get_cr3());
    kernel_page_dir                 = (page_directory_t){.table = kernel_page_table};
    double_fault_page               = get_cr3();
    register_interrupt_handler(14, (void *)page_fault_handle, 0, 0x8E);
    current_directory = &kernel_page_dir;
}
