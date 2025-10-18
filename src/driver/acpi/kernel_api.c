#include <driver/uacpi/kernel_api.h>
#include <krlibc.h>
#include <mem/frame.h>
#include <mem/page.h>
// #include <drivers/bus/pci.h>
#include <driver/acpi.h>
#include <intctl.h>
#include <lock.h>
#include <mem/heap.h>
#include <sem.h>
#include <term/klog.h>
#include <timer.h>

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address) {
    *out_rsdp_address = boot_get_acpi_rsdp();
    return UACPI_STATUS_OK;
}

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
    void *vaddr = (void *)phys_to_virt(addr);
    page_map_range(get_kernel_pagedir(), (uint64_t)vaddr & ~(PAGE_SIZE - 1),
                   addr & ~(PAGE_SIZE - 1), len, KERNEL_PTE_FLAGS);
    return vaddr;
}

void uacpi_kernel_unmap(void *addr, uacpi_size len) {
    unmap_page_range(get_kernel_pagedir(), (uint64_t)addr & ~(PAGE_SIZE - 1), len);
}

void uacpi_kernel_log(uacpi_log_level, const uacpi_char *str) {
    printk(str);
}

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle *out_handle) {
    //    pci_device_t *device = pci_find_bdfs(address.bus, address.device,
    //                                         address.function, address.segment);
    //    if (device) {
    //        *out_handle = (void *)device;
    //        return UACPI_STATUS_OK;
    //    }

    return UACPI_STATUS_NOT_FOUND;
}

void uacpi_kernel_pci_device_close(uacpi_handle) {}

uacpi_status uacpi_kernel_pci_read8(uacpi_handle device, uacpi_size offset, uacpi_u8 *value) {
    //    pci_device_t *pci_device = device;
    //    *value = (uacpi_u8)pci_device->op->read(pci_device->bus, pci_device->slot,
    //                                            pci_device->func,
    //                                            pci_device->segment, offset);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_read16(uacpi_handle device, uacpi_size offset, uacpi_u16 *value) {
    //    pci_device_t *pci_device = device;
    //    *value = (uacpi_u16)pci_device->op->read(pci_device->bus, pci_device->slot,
    //                                             pci_device->func,
    //                                             pci_device->segment, offset);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_read32(uacpi_handle device, uacpi_size offset, uacpi_u32 *value) {
    //    pci_device_t *pci_device = device;
    //    *value = (uacpi_u32)pci_device->op->read(pci_device->bus, pci_device->slot,
    //                                             pci_device->func,
    //                                             pci_device->segment, offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle device, uacpi_size offset, uacpi_u8 value) {
    //    pci_device_t *pci_device  = device;
    //    uint32_t      v           = (uacpi_u32)pci_device->op->read(pci_device->bus, pci_device->slot,
    //                                                                pci_device->func, pci_device->segment, offset);
    //    v                        &= ~0xFF;
    //    v                        |= value;
    //    pci_device->op->write(pci_device->bus, pci_device->slot, pci_device->func, pci_device->segment,
    //                          offset, v);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_write16(uacpi_handle device, uacpi_size offset, uacpi_u16 value) {
    //    pci_device_t *pci_device  = device;
    //    uint32_t      v           = (uacpi_u32)pci_device->op->read(pci_device->bus, pci_device->slot,
    //                                                                pci_device->func, pci_device->segment, offset);
    //    v                        &= ~0xFFFF;
    //    v                        |= value;
    //    pci_device->op->write(pci_device->bus, pci_device->slot, pci_device->func, pci_device->segment,
    //                          offset, v);
    return UACPI_STATUS_OK;
}
uacpi_status uacpi_kernel_pci_write32(uacpi_handle device, uacpi_size offset, uacpi_u32 value) {
    //    pci_device_t *pci_device = device;
    //    pci_device->op->write(pci_device->bus, pci_device->slot, pci_device->func, pci_device->segment,
    //                          offset, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle) {
    *out_handle = (uacpi_handle)base;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) {}

#if defined(__x86_64__)

#    include "io.h"

uacpi_status uacpi_kernel_io_read8(uacpi_handle base, uacpi_size offset, uacpi_u8 *out_value) {
    *out_value = io_in8((uint64_t)base + offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle base, uacpi_size offset, uacpi_u16 *out_value) {
    *out_value = io_in16((uint64_t)base + offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle base, uacpi_size offset, uacpi_u32 *out_value) {
    *out_value = io_in32((uint64_t)base + offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle base, uacpi_size offset, uacpi_u8 in_value) {
    io_out8((uint64_t)base + offset, in_value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle base, uacpi_size offset, uacpi_u16 in_value) {
    io_out16((uint64_t)base + offset, in_value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle base, uacpi_size offset, uacpi_u32 in_value) {
    io_out32((uint64_t)base + offset, in_value);
    return UACPI_STATUS_OK;
}

#else

uacpi_status uacpi_kernel_io_read8(uacpi_handle, uacpi_size offset, uacpi_u8 *out_value) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle, uacpi_size offset, uacpi_u16 *out_value) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle, uacpi_size offset, uacpi_u32 *out_value) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle, uacpi_size offset, uacpi_u8 in_value) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle, uacpi_size offset, uacpi_u16 in_value) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle, uacpi_size offset, uacpi_u32 in_value) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

#endif

void *uacpi_kernel_alloc(uacpi_size size) {
    return malloc(size);
}
void uacpi_kernel_free(void *mem) {
    free(mem);
}

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
    return nano_time();
}

static void delay(uint64_t ns) {
    uint64_t start = nano_time();
    while (nano_time() - start < ns) {
        arch_pause();
    }
}

void uacpi_kernel_stall(uacpi_u8 usec) {
    delay(usec * 1000);
}

void uacpi_kernel_sleep(uacpi_u64 msec) {
    delay(msec * 1000000);
}

uacpi_handle uacpi_kernel_create_mutex(void) {
    spin_t *lock = malloc(sizeof(spin_t));
    *lock        = SPIN_INIT;
    return (uacpi_handle)lock;
}

void uacpi_kernel_free_mutex(uacpi_handle lock) {
    free(lock);
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle lock, uacpi_u16 timeout) {
    (void)timeout;
    if (lock) spin_lock(*(spin_t*)lock);
    return UACPI_STATUS_OK;
}

void uacpi_kernel_release_mutex(uacpi_handle lock) {
    if (lock) spin_unlock(*(spin_t*)lock);
}

uacpi_handle uacpi_kernel_create_event(void) {
    sem_t *sem = malloc(sizeof(sem_t));
    memset(sem, 0, sizeof(sem_t));
    return sem;
}

void uacpi_kernel_free_event(uacpi_handle sem) {
    free(sem);
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle sem, uacpi_u16 timeout) {
    sem_wait(sem, (uint64_t)timeout * 1000000);
    return UACPI_TRUE;
}

void uacpi_kernel_signal_event(uacpi_handle sem) {
    sem_post(sem);
}

void uacpi_kernel_reset_event(uacpi_handle sem) {
    ((sem_t *)sem)->lock    = SPIN_INIT;
    ((sem_t *)sem)->cnt     = 0;
    ((sem_t *)sem)->invalid = false;
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *request) {
    return UACPI_STATUS_OK;
}

uacpi_thread_id uacpi_kernel_get_thread_id(void) {
    //TODO return current_task;
    return NULL;
}

#if defined(__x86_64__)

typedef struct uacpi_irq_handler_arg {
    uacpi_interrupt_handler irq_handler;
    uacpi_handle            ctx;
} uacpi_irq_handler_arg_t;

void uacpi_irq_handler(uint64_t irq_num, void *data, struct pt_regs *regs) {
    uacpi_irq_handler_arg_t *arg = data;
    arg->irq_handler(arg->ctx);
}

uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32               irq,
                                                    uacpi_interrupt_handler irq_handler,
                                                    uacpi_handle            ctx,
                                                    uacpi_handle           *out_irq_handle) {
    uacpi_irq_handler_arg_t *arg = malloc(sizeof(uacpi_irq_handler_arg_t));
    arg->irq_handler             = irq_handler;
    arg->ctx                     = ctx;
    //TODO irq_regist_irq(irq + 32, uacpi_irq_handler, irq, arg, &apic_controller, "uacpi_irq_handler");
    return UACPI_STATUS_OK;
}

#else

uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32               irq,
                                                    uacpi_interrupt_handler irq_handler,
                                                    uacpi_handle            ctx,
                                                    uacpi_handle           *out_irq_handle) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

#endif

uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler,
                                                      uacpi_handle irq_handle) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_handle uacpi_kernel_create_spinlock(void) {
    spin_t *lock = malloc(sizeof(spin_t));
    *lock        = SPIN_INIT;
    return (uacpi_handle)lock;
}

void uacpi_kernel_free_spinlock(uacpi_handle lock) {
    free(lock);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle lock) {
    if (lock) spin_lock(*(spin_t*)lock);
    return 0;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle lock, uacpi_cpu_flags flags) {
    (void)flags;
    if (lock) spin_unlock(*(spin_t*)lock);
}

uacpi_status uacpi_kernel_schedule_work(uacpi_work_type type, uacpi_work_handler handler,
                                        uacpi_handle ctx) {
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status uacpi_kernel_wait_for_work_completion(void) {
    return UACPI_STATUS_UNIMPLEMENTED;
}
