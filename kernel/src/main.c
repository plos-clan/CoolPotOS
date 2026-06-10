#include "arch.h"
#include "driver/tty.h"
#include "mem/frame.h"
#include "mem/page.h"
#include "mem/slub.h"
#include "term/kprint.h"

_Noreturn void kmain() {
    arch_init();
    init_frame();
    page_init();
    slub_init();
    tty_init();

    logkf("kernel load done!\n");
    while (true)
        arch_wait_for_interrupt();
}
