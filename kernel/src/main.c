#include "arch.h"
#include "bootarg.h"
#include "driver/tty.h"
#include "driver/device.h"
#include "fs/vfs.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "mem/slub.h"
#include "task/task.h"
#include "term/kprint.h"
#include "klog.h"
#include "kconfig.h"
#include "ksecure.h"

_Noreturn void kmain() {
    arch_init();
    init_frame();
    page_init();
    slub_init();

    /* 初始化安全模块 */
    ksecure_stack_init();
    ksecure_aslr_init();

    /* 初始化日志系统 */
    klog_init();

    /* 初始化配置系统 */
    kconfig_init();

    size_t boot_argc = boot_parse_cmdline(boot_get_cmdline());

    /* 从启动参数加载配置覆盖 */
    kconfig_load_bootargs();

    device_init();
    tty_init();

    printk("CoolPotOS %s\n", KERNEL_NAME);
    kinfo("kernel cmdline(%llu): %s", boot_argc, boot_get_cmdline());

    task_init();
    vfs_init();

    ksuccess("kernel load done!\n");
    while (true)
        arch_wait_for_interrupt();
}
