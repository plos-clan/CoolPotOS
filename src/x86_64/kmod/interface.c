/**
 * 内核模块接口导出及部分兼容性实现
 * Kernel Module Interface Export and Compatibility Implementation
 *
 * 有关于接口的使用说明, 详见其预定义的头文件注释
 */
#include "cpustats.h"
#include "dlinker.h"
#include "frame.h"
#include "heap.h"
#include "hhdm.h"
#include "ipc.h"
#include "isr.h"
#include "klog.h"
#include "kprint.h"
#include "krlibc.h"
#include "memstats.h"
#include "page.h"
#include "pcb.h"
#include "pci.h"
#include "sprintf.h"
#include "vdisk.h"
#include "vfs.h"

/**
 * MemoryManager Subsystem Interface
 * 内存管理子系统接口导出
 */
EXPORT_SYMBOL(get_physical_memory_offset);
EXPORT_SYMBOL(get_current_directory);
EXPORT_SYMBOL(alloc_frames);
EXPORT_SYMBOL(free_frames);
EXPORT_SYMBOL(page_map_range_to);
EXPORT_SYMBOL(page_map_range);
EXPORT_SYMBOL(switch_process_page_directory);
EXPORT_SYMBOL(get_commit_memory);
EXPORT_SYMBOL(get_used_memory);
EXPORT_SYMBOL(get_available_memory);
EXPORT_SYMBOL(get_reserved_memory);
EXPORT_SYMBOL(get_all_memory);
EXPORT_SYMBOL(clone_page_directory);
EXPORT_SYMBOL(free_page_directory);
EXPORT_SYMBOL(phys_to_virt);
EXPORT_SYMBOL(virt_to_phys);
EXPORT_SYMBOL(page_virt_to_phys);
EXPORT_SYMBOL(driver_phys_to_virt);
EXPORT_SYMBOL(driver_virt_to_phys);

/**
 * FileSystem Subsystem Interface
 * 文件系统子系统接口导出
 */
EXPORT_SYMBOL(general_map);
EXPORT_SYMBOL(vfs_regist);
EXPORT_SYMBOL(vfs_read);
EXPORT_SYMBOL(vfs_write);
EXPORT_SYMBOL(vfs_node_alloc);
EXPORT_SYMBOL(vfs_child_append);
EXPORT_SYMBOL(vfs_open);
EXPORT_SYMBOL(vfs_close);
EXPORT_SYMBOL(vfs_free);
EXPORT_SYMBOL(get_rootdir);
EXPORT_SYMBOL(vfs_mount);
EXPORT_SYMBOL(vfs_unmount);
EXPORT_SYMBOL(vfs_get_fullpath);
EXPORT_SYMBOL(is_virtual_fs);

/**
 * Interrupt and Clock Subsystem Interface
 * 中断与时钟子系统接口导出
 */
EXPORT_SYMBOL(register_interrupt_handler);
EXPORT_SYMBOL(sched_clock);
EXPORT_SYMBOL(send_eoi);
EXPORT_SYMBOL(nano_time);

/**
 * Process and Scheduling Subsystem Interface
 * 进程与调度子系统接口导出
 */
EXPORT_SYMBOL(create_process_group);
EXPORT_SYMBOL(create_kernel_thread);
EXPORT_SYMBOL(create_user_thread);
EXPORT_SYMBOL(enable_scheduler);
EXPORT_SYMBOL(disable_scheduler);
EXPORT_SYMBOL(scheduler_yield);
EXPORT_SYMBOL(get_current_task);
EXPORT_SYMBOL(ipc_send);
EXPORT_SYMBOL(ipc_recv_wait);

/**
 * 驱动子系统接口导出
 */
EXPORT_SYMBOL(devfs_register);
EXPORT_SYMBOL(regist_vdisk);
EXPORT_SYMBOL(vdisk_read);
EXPORT_SYMBOL(vdisk_write);
EXPORT_SYMBOL(pci_find_vid_did);
EXPORT_SYMBOL(pci_find_class);

// kernel logger functions
EXPORT_SYMBOL(logkf);
EXPORT_SYMBOL_F(printk, cp_printf);

// kernel heap functions
EXPORT_SYMBOL(malloc);
EXPORT_SYMBOL(realloc);
EXPORT_SYMBOL(calloc);
EXPORT_SYMBOL(free);

// krlibc functions
EXPORT_SYMBOL(memcmp);
EXPORT_SYMBOL(memset);
EXPORT_SYMBOL(memcpy);
EXPORT_SYMBOL(strdup);
EXPORT_SYMBOL(strlen);
EXPORT_SYMBOL(strnlen);
EXPORT_SYMBOL(strcpy);
EXPORT_SYMBOL(strncpy);
EXPORT_SYMBOL(strcmp);
EXPORT_SYMBOL(strncmp);
EXPORT_SYMBOL(sprintf);
EXPORT_SYMBOL(snprintf);
EXPORT_SYMBOL(strrchr);
