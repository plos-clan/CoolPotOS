#include "driver/gop.h"
#include "io.h"
#include "krlibc.h"
#include "bootarg.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "mem/heap.h"

USED _Noreturn void kmain() {
    size_t boot_argc = boot_parse_cmdline(get_kernel_cmdline());
    init_frame();
    init_page();
    init_heap();
    arch_page_setup_l2();
    gop_clear(get_framebuffer_response()->framebuffers[0],0x333333);
    while (true) arch_wait_for_interrupt();
}