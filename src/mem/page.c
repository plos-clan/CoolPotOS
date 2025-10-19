#include "mem/page.h"
#include "mem/frame.h"
#include "lock.h"
#include "task/smp.h"
#include "task/task.h"

page_directory_t  kernel_page_dir;

page_directory_t *get_kernel_pagedir() {
    return &kernel_page_dir;
}

void page_map_range_to(page_directory_t *directory, uint64_t frame, uint64_t length,
                       uint64_t flags) {
    for (uint64_t i = 0; i < length; i += PAGE_SIZE) {
        uint64_t var = (uint64_t)phys_to_virt(frame + i);
        page_map_to(directory, var, frame + i, flags);
    }
}

void page_map_range(page_directory_t *directory, uint64_t addr, uint64_t frame, uint64_t length,
                    uint64_t flags) {
    for (uint64_t i = 0; i < length; i += PAGE_SIZE) {
        uint64_t var = addr + i;
        page_map_to(directory, var, frame + i, flags);
    }
}

void unmap_page_range(page_directory_t *directory, uint64_t vaddr, uint64_t size) {
    for (uint64_t va = vaddr; va < vaddr + size; va += PAGE_SIZE) {
        unmap_page(directory, va);
    }
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

page_directory_t *switch_context_directory(page_directory_t *directory){
    tcb_t thread = get_current_task();
    if(thread == NULL) return NULL;
    arch_close_interrupt();
    page_directory_t *ret = thread->process->directory;
    thread->process->directory = directory;
    switch_page_directory(directory);
    arch_open_interrupt();
    return ret;
}

page_directory_t *current_directory() {
    cpu_local_t *local = arch_current_cpu();
    if(local == NULL) return NULL;
    return local->directory;
}

void init_page(){
    arch_page_setup();
}
