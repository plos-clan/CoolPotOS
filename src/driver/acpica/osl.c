#include "lib/acpica/acpi.h"

#include "boot.h"
#include "driver/pci/pci.h"
#include "driver/tty.h"
#include "krlibc.h"
#include "lock.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "sem.h"
#include "task/scheduler.h"
#include "task/task.h"
#include "term/klog.h"
#include "timer.h"

#include <intctl.h>

#ifdef __x86_64__
#    include "io.h"
#endif

extern pcb_t                 kernel_process;
extern _Atomic volatile bool scheduler_status;
extern tty_t                *current_session;
extern irq_action_t          actions[ARCH_MAX_IRQ_NUM];

typedef struct {
    UINT16 object_size;
} acpica_cache_t;

typedef struct {
    ACPI_OSD_EXEC_CALLBACK func;
    void                  *ctx;
} acpica_exec_ctx_t;

static volatile UINT32 acpica_exec_count = 0;

static int acpica_exec_thread(void *arg) {
    acpica_exec_ctx_t *exec = arg;

    if (exec && exec->func) {
        exec->func(exec->ctx);
    }
    free(exec);
    __atomic_sub_fetch(&acpica_exec_count, 1, __ATOMIC_RELEASE);
    return 0;
}

static inline void acpi_busy_wait_ns(uint64_t ns) {
    uint64_t end = nano_time() + ns;
    while (nano_time() < end) {
        cpu_relax();
    }
}

ACPI_STATUS AcpiOsInitialize(void) {
    return AE_OK;
}

ACPI_STATUS AcpiOsTerminate(void) {
    return AE_OK;
}

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer(void) {
    return boot_get_acpi_rsdp();
}

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *InitVal, ACPI_STRING *NewVal) {
    (void)InitVal;
    if (NewVal) {
        *NewVal = NULL;
    }
    return AE_OK;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_TABLE_HEADER **NewTable) {
    (void)ExistingTable;
    if (NewTable) {
        *NewTable = NULL;
    }
    return AE_OK;
}

ACPI_STATUS AcpiOsPhysicalTableOverride(
    ACPI_TABLE_HEADER *ExistingTable, ACPI_PHYSICAL_ADDRESS *NewAddress, UINT32 *NewTableLength
) {
    (void)ExistingTable;
    if (NewAddress) {
        *NewAddress = 0;
    }
    if (NewTableLength) {
        *NewTableLength = 0;
    }
    return AE_OK;
}

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *OutHandle) {
    spin_t *lock;

    if (!OutHandle) {
        return AE_BAD_PARAMETER;
    }

    lock = (spin_t *)malloc(sizeof(*lock));
    if (!lock) {
        return AE_NO_MEMORY;
    }

    *lock      = SPIN_INIT;
    *OutHandle = (ACPI_SPINLOCK)lock;
    return AE_OK;
}

void AcpiOsDeleteLock(ACPI_SPINLOCK Handle) {
    if (Handle) {
        free(Handle);
    }
}

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle) {
    if (Handle) {
        spin_lock(*(spin_t *)Handle);
    }
    return 0;
}

void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags) {
    (void)Flags;
    if (Handle) {
        spin_unlock(*(spin_t *)Handle);
    }
}

ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_SEMAPHORE *OutHandle) {
    sem_t *sem;

    (void)MaxUnits;
    if (!OutHandle) {
        return AE_BAD_PARAMETER;
    }

    sem = (sem_t *)malloc(sizeof(*sem));
    if (!sem) {
        return AE_NO_MEMORY;
    }

    sem->lock    = SPIN_INIT;
    sem->cnt     = InitialUnits;
    sem->invalid = false;

    *OutHandle = (ACPI_SEMAPHORE)sem;
    return AE_OK;
}

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle) {
    if (!Handle) {
        return AE_BAD_PARAMETER;
    }
    free(Handle);
    return AE_OK;
}

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout) {
    sem_t *sem = (sem_t *)Handle;

    if (!sem || Units == 0) {
        return AE_BAD_PARAMETER;
    }

    if (Timeout == 0) {
        spin_lock(sem->lock);
        if (sem->cnt >= Units) {
            sem->cnt -= Units;
            spin_unlock(sem->lock);
            return AE_OK;
        }
        spin_unlock(sem->lock);
        return AE_TIME;
    }

    while (Units--) {
        if (Timeout == ACPI_WAIT_FOREVER) {
            (void)sem_wait(sem, 0);
            continue;
        }
        uint64_t ns = (uint64_t)Timeout * 1000000ULL;
        if (ns > UINT32_MAX) {
            ns = UINT32_MAX;
        }
        if (!sem_wait(sem, (uint32_t)ns)) {
            return AE_TIME;
        }
    }
    return AE_OK;
}

ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units) {
    sem_t *sem = (sem_t *)Handle;

    if (!sem || Units == 0) {
        return AE_BAD_PARAMETER;
    }

    while (Units--) {
        sem_post(sem);
    }
    return AE_OK;
}

void *AcpiOsAllocate(ACPI_SIZE Size) {
    return calloc(1, Size);
}

void AcpiOsFree(void *Memory) {
    free(Memory);
}

void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS Where, ACPI_SIZE Length) {
    void *vaddr = phys_to_virt(Where);
    page_map_range(
        get_kernel_pagedir(), (uint64_t)vaddr & ~(PAGE_SIZE - 1), Where & ~(PAGE_SIZE - 1), Length,
        KERNEL_PTE_FLAGS
    );
    return vaddr;
}

void AcpiOsUnmapMemory(void *LogicalAddress, ACPI_SIZE Size) {
    unmap_page_range(get_kernel_pagedir(), (uint64_t)LogicalAddress & ~(PAGE_SIZE - 1), Size);
}

ACPI_STATUS AcpiOsGetPhysicalAddress(void *LogicalAddress, ACPI_PHYSICAL_ADDRESS *PhysicalAddress) {
    if (!PhysicalAddress) {
        return AE_BAD_PARAMETER;
    }
    *PhysicalAddress = (ACPI_PHYSICAL_ADDRESS)virt_to_phys(LogicalAddress);
    return AE_OK;
}

ACPI_STATUS
AcpiOsCreateCache(char *CacheName, UINT16 ObjectSize, UINT16 MaxDepth, ACPI_CACHE_T **ReturnCache) {
    acpica_cache_t *cache;

    (void)CacheName;
    (void)MaxDepth;
    if (!ReturnCache || ObjectSize == 0) {
        return AE_BAD_PARAMETER;
    }

    cache = (acpica_cache_t *)malloc(sizeof(*cache));
    if (!cache) {
        return AE_NO_MEMORY;
    }

    cache->object_size = ObjectSize;
    *ReturnCache       = (ACPI_CACHE_T *)cache;
    return AE_OK;
}

ACPI_STATUS AcpiOsDeleteCache(ACPI_CACHE_T *Cache) {
    if (Cache) {
        free(Cache);
    }
    return AE_OK;
}

ACPI_STATUS AcpiOsPurgeCache(ACPI_CACHE_T *Cache) {
    (void)Cache;
    return AE_OK;
}

void *AcpiOsAcquireObject(ACPI_CACHE_T *Cache) {
    acpica_cache_t *cache = (acpica_cache_t *)Cache;

    if (!cache) {
        return NULL;
    }
    return calloc(1, cache->object_size);
}

ACPI_STATUS AcpiOsReleaseObject(ACPI_CACHE_T *Cache, void *Object) {
    (void)Cache;
    free(Object);
    return AE_OK;
}

typedef struct acpica_irq_handler_arg {
    ACPI_OSD_HANDLER irq_handler;
    void            *ctx;
} acpica_irq_handler_arg_t;

void acpica_irq_handler(uint64_t irq_num, void *data, struct pt_regs *regs) {
    acpica_irq_handler_arg_t *arg = data;
    if (arg && arg->irq_handler) {
        arg->irq_handler(arg->ctx);
    }
}

ACPI_STATUS AcpiOsInstallInterruptHandler(
    UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine, void *Context
) {
    acpica_irq_handler_arg_t *arg;

    if (!ServiceRoutine) {
        return AE_BAD_PARAMETER;
    }

    arg = malloc(sizeof(*arg));
    if (!arg) {
        return AE_NO_MEMORY;
    }
    arg->irq_handler = ServiceRoutine;
    arg->ctx         = Context;
#if defined(__x86_64__) || defined(__amd64__)
    extern intctl_t apic_controller;
    irq_regist_irq(
        InterruptNumber + IRQ_BASE_VECTOR, acpica_irq_handler, InterruptNumber, arg,
        &apic_controller, "acpica_irq_handler", 0, IO_APIC
    );
#endif
    return AE_OK;
}

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine) {
    uint64_t      vector = (uint64_t)InterruptNumber + IRQ_BASE_VECTOR;
    irq_action_t *action;

    if (!ServiceRoutine) {
        return AE_BAD_PARAMETER;
    }
    if (vector >= ARCH_MAX_IRQ_NUM) {
        return AE_BAD_PARAMETER;
    }

    action = &actions[vector];
    if (!action->handler) {
        return AE_NOT_EXIST;
    }

    if (action->irq_controller && action->irq_controller->_mask) {
        action->irq_controller->_mask(vector, action->flags);
    }

    if (action->data) {
        free(action->data);
    }
    if (action->name) {
        free(action->name);
    }

    action->handler        = NULL;
    action->data           = NULL;
    action->irq_controller = NULL;
    action->name           = NULL;
    action->flags          = 0;
    action->type           = 0;
    return AE_OK;
}

ACPI_THREAD_ID AcpiOsGetThreadId(void) {
    tcb_t task = get_current_task();
    if (!task || task->tid == 0) {
        return 1;
    }
    return (ACPI_THREAD_ID)task->tid;
}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context) {
    acpica_exec_ctx_t *ctx;

    (void)Type;
    if (!Function) {
        return AE_BAD_PARAMETER;
    }

    if (!kernel_process || !scheduler_status) {
        Function(Context);
        return AE_OK;
    }

    ctx = malloc(sizeof(*ctx));
    if (!ctx) {
        Function(Context);
        return AE_NO_MEMORY;
    }

    ctx->func = Function;
    ctx->ctx  = Context;
    __atomic_add_fetch(&acpica_exec_count, 1, __ATOMIC_RELAXED);
    create_kernel_thread("acpica_exec", acpica_exec_thread, ctx, NULL, NICE_TO_PRIO(0));
    return AE_OK;
}

void AcpiOsWaitEventsComplete(void) {
    while (__atomic_load_n(&acpica_exec_count, __ATOMIC_ACQUIRE) != 0) {
        if (scheduler_status) {
            scheduler_yield();
        } else {
            cpu_relax();
        }
    }
}

void AcpiOsSleep(UINT64 Milliseconds) {
    if (scheduler_status && get_current_task()) {
        scheduler_nano_sleep(Milliseconds * 1000000ULL);
    } else {
        acpi_busy_wait_ns(Milliseconds * 1000000ULL);
    }
}

void AcpiOsStall(UINT32 Microseconds) {
    acpi_busy_wait_ns((uint64_t)Microseconds * 1000ULL);
}

ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32 *Value, UINT32 Width) {
#ifdef __x86_64__
    if (!Value) {
        return AE_BAD_PARAMETER;
    }
    switch (Width) {
    case 8:
        *Value = io_in8((uint16_t)Address);
        return AE_OK;
    case 16:
        *Value = io_in16((uint16_t)Address);
        return AE_OK;
    case 32:
        *Value = io_in32((uint16_t)Address);
        return AE_OK;
    default:
        return AE_BAD_PARAMETER;
    }
#else
    (void)Address;
    (void)Value;
    (void)Width;
    return AE_NOT_IMPLEMENTED;
#endif
}

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width) {
#ifdef __x86_64__
    switch (Width) {
    case 8:
        io_out8((uint16_t)Address, (uint8_t)Value);
        return AE_OK;
    case 16:
        io_out16((uint16_t)Address, (uint16_t)Value);
        return AE_OK;
    case 32:
        io_out32((uint16_t)Address, (uint32_t)Value);
        return AE_OK;
    default:
        return AE_BAD_PARAMETER;
    }
#else
    (void)Address;
    (void)Value;
    (void)Width;
    return AE_NOT_IMPLEMENTED;
#endif
}

ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 *Value, UINT32 Width) {
    volatile void *ptr = phys_to_virt(Address);

    if (!ptr || !Value) {
        return AE_BAD_PARAMETER;
    }

    switch (Width) {
    case 8:
        *Value = *(volatile uint8_t *)ptr;
        return AE_OK;
    case 16:
        *Value = *(volatile uint16_t *)ptr;
        return AE_OK;
    case 32:
        *Value = *(volatile uint32_t *)ptr;
        return AE_OK;
    case 64:
        *Value = *(volatile uint64_t *)ptr;
        return AE_OK;
    default:
        return AE_BAD_PARAMETER;
    }
}

ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 Value, UINT32 Width) {
    volatile void *ptr = phys_to_virt(Address);

    if (!ptr) {
        return AE_BAD_PARAMETER;
    }

    switch (Width) {
    case 8:
        *(volatile uint8_t *)ptr = (uint8_t)Value;
        return AE_OK;
    case 16:
        *(volatile uint16_t *)ptr = (uint16_t)Value;
        return AE_OK;
    case 32:
        *(volatile uint32_t *)ptr = (uint32_t)Value;
        return AE_OK;
    case 64:
        *(volatile uint64_t *)ptr = (uint64_t)Value;
        return AE_OK;
    default:
        return AE_BAD_PARAMETER;
    }
}

ACPI_STATUS
AcpiOsReadPciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 *Value, UINT32 Width) {
    uint32_t data;

    if (!PciId || !Value) {
        return AE_BAD_PARAMETER;
    }

    if (Width != 8 && Width != 16 && Width != 32 && Width != 64) {
        return AE_BAD_PARAMETER;
    }

    data = pci_read(PciId->Bus, PciId->Device, PciId->Function, PciId->Segment, Reg & ~0x3U);
    switch (Width) {
    case 8:
        *Value = (data >> ((Reg & 3U) * 8U)) & 0xFFU;
        break;
    case 16:
        *Value = (data >> ((Reg & 2U) * 8U)) & 0xFFFFU;
        break;
    case 32:
        *Value = data;
        break;
    case 64: {
        UINT64 hi;
        hi =
            pci_read(PciId->Bus, PciId->Device, PciId->Function, PciId->Segment, (Reg & ~0x3U) + 4);
        *Value = (hi << 32) | data;
        break;
    }
    }
    return AE_OK;
}

ACPI_STATUS
AcpiOsWritePciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 Value, UINT32 Width) {
    uint32_t data;
    uint32_t shift;

    if (!PciId) {
        return AE_BAD_PARAMETER;
    }

    if (Width != 8 && Width != 16 && Width != 32 && Width != 64) {
        return AE_BAD_PARAMETER;
    }

    data = pci_read(PciId->Bus, PciId->Device, PciId->Function, PciId->Segment, Reg & ~0x3U);
    switch (Width) {
    case 8:
        shift = (Reg & 3U) * 8U;
        data  = (data & ~(0xFFU << shift)) | ((uint32_t)(Value & 0xFFU) << shift);
        break;
    case 16:
        shift = (Reg & 2U) * 8U;
        data  = (data & ~(0xFFFFU << shift)) | ((uint32_t)(Value & 0xFFFFU) << shift);
        break;
    case 32:
        data = (uint32_t)Value;
        break;
    case 64:
        pci_write(
            PciId->Bus, PciId->Device, PciId->Function, PciId->Segment, (Reg & ~0x3U) + 4,
            (uint32_t)(Value >> 32)
        );
        data = (uint32_t)Value;
        break;
    }

    pci_write(PciId->Bus, PciId->Device, PciId->Function, PciId->Segment, Reg & ~0x3U, data);
    return AE_OK;
}

BOOLEAN AcpiOsReadable(void *Pointer, ACPI_SIZE Length) {
    (void)Pointer;
    (void)Length;
    return TRUE;
}

BOOLEAN AcpiOsWritable(void *Pointer, ACPI_SIZE Length) {
    (void)Pointer;
    (void)Length;
    return TRUE;
}

UINT64 AcpiOsGetTimer(void) {
    return nano_time() / 100ULL;
}

ACPI_STATUS AcpiOsSignal(UINT32 Function, void *Info) {
    (void)Info;
    if (Function == ACPI_SIGNAL_FATAL) {
        logkf("acpica: fatal signal\n");
    }
    return AE_OK;
}

ACPI_STATUS AcpiOsEnterSleep(UINT8 SleepState, UINT32 RegaValue, UINT32 RegbValue) {
    (void)SleepState;
    (void)RegaValue;
    (void)RegbValue;
    arch_close_interrupt();
    return AE_OK;
}

void AcpiOsPrintf(const char *Format, ...) {
    char    buffer[512];
    va_list args;

    va_start(args, Format);
    (void)vsnprintf(buffer, sizeof(buffer), Format, args);
    va_end(args);

    printk(buffer);
}

void AcpiOsVprintf(const char *Format, va_list Args) {
    char buffer[512];

    (void)vsnprintf(buffer, sizeof(buffer), Format, Args);
    printk(buffer);
}

void AcpiOsRedirectOutput(void *Destination) {
    (void)Destination;
}

ACPI_STATUS AcpiOsGetLine(char *Buffer, UINT32 BufferLength, UINT32 *BytesRead) {
    UINT32 count = 0;

    if (!Buffer || !BytesRead || BufferLength == 0) {
        return AE_BAD_PARAMETER;
    }
    if (!current_session) {
        return AE_NOT_IMPLEMENTED;
    }

    while (count + 1 < BufferLength) {
        int ch = kernel_getch();
        if (ch < 0) {
            continue;
        }
        Buffer[count++] = (char)ch;
        if (ch == '\n' || ch == '\r') {
            break;
        }
    }
    Buffer[count] = '\0';
    *BytesRead    = count;
    return AE_OK;
}

ACPI_STATUS AcpiOsInitializeDebugger(void) {
    if (!current_session) {
        return AE_NOT_IMPLEMENTED;
    }
    return AE_OK;
}

void AcpiOsTerminateDebugger(void) {
}

ACPI_STATUS AcpiOsWaitCommandReady(void) {
    return AE_OK;
}

ACPI_STATUS AcpiOsNotifyCommandComplete(void) {
    return AE_OK;
}

void AcpiOsTracePoint(ACPI_TRACE_EVENT_TYPE Type, BOOLEAN Begin, UINT8 *Aml, char *Pathname) {
    (void)Type;
    (void)Begin;
    (void)Aml;
    (void)Pathname;
}

ACPI_STATUS AcpiOsGetTableByName(
    char *Signature, UINT32 Instance, ACPI_TABLE_HEADER **Table, ACPI_PHYSICAL_ADDRESS *Address
) {
    ACPI_STATUS st;

    if (!Signature || !Table) {
        return AE_BAD_PARAMETER;
    }
    st = AcpiGetTable(Signature, Instance, Table);
    if (ACPI_SUCCESS(st) && Address) {
        *Address = (ACPI_PHYSICAL_ADDRESS)virt_to_phys(*Table);
    }
    return st;
}

ACPI_STATUS AcpiOsGetTableByIndex(
    UINT32 Index, ACPI_TABLE_HEADER **Table, UINT32 *Instance, ACPI_PHYSICAL_ADDRESS *Address
) {
    ACPI_STATUS        st;
    ACPI_TABLE_HEADER *hdr;
    UINT32             inst = 1;

    if (!Table) {
        return AE_BAD_PARAMETER;
    }

    st = AcpiGetTableByIndex(Index, &hdr);
    if (ACPI_FAILURE(st)) {
        return st;
    }

    if (Instance) {
        for (UINT32 i = 0; i < Index; i++) {
            ACPI_TABLE_HEADER *tmp;
            if (ACPI_SUCCESS(AcpiGetTableByIndex(i, &tmp))) {
                if (!memcmp(tmp->Signature, hdr->Signature, 4)) {
                    inst++;
                }
                AcpiPutTable(tmp);
            }
        }
        *Instance = inst;
    }

    if (Address) {
        *Address = (ACPI_PHYSICAL_ADDRESS)virt_to_phys(hdr);
    }
    *Table = hdr;
    return AE_OK;
}

ACPI_STATUS AcpiOsGetTableByAddress(ACPI_PHYSICAL_ADDRESS Address, ACPI_TABLE_HEADER **Table) {
    UINT32 index = 0;

    if (!Table) {
        return AE_BAD_PARAMETER;
    }
    *Table = NULL;

    while (true) {
        ACPI_TABLE_HEADER *hdr;
        ACPI_STATUS        st = AcpiGetTableByIndex(index, &hdr);
        if (ACPI_FAILURE(st)) {
            break;
        }

        if ((ACPI_PHYSICAL_ADDRESS)virt_to_phys(hdr) == Address) {
            *Table = hdr;
            return AE_OK;
        }

        AcpiPutTable(hdr);
        index++;
    }

    return AE_NOT_FOUND;
}

void *AcpiOsOpenDirectory(char *Pathname, char *WildcardSpec, char RequestedFileType) {
    (void)Pathname;
    (void)WildcardSpec;
    (void)RequestedFileType;
    return NULL;
}

char *AcpiOsGetNextFilename(void *DirHandle) {
    (void)DirHandle;
    return NULL;
}

void AcpiOsCloseDirectory(void *DirHandle) {
    (void)DirHandle;
}
