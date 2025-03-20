#include "keyboard.h"
#include "klog.h"
#include "os_terminal.h"
#include "scheduler.h"
#include "shell.h"
#include "speaker.h"
#include "timer.h"

static int     sel      = 0;
static bool    is_reset = false;
static uint8_t status   = 0b11111111;
static int     index    = 0;
static char    l[5][15] = {
    [0] = "\033[47m \033[0m\0",    [1] = " \033[47m \033[0m\0",    [2] = "  \033[47m \033[0m\0",
    [3] = "   \033[47m \033[0m\0", [4] = "    \033[47m \033[0m\0",
};

static void key() {
    if (is_reset) {
        if (index == 0 || index == 5) beep();
        if (index > 4 * 2) {
            index    = 0;
            is_reset = false;
        } else {
            index++;
            if (sel == 0) status &= ~0x80;
            if (sel == 1) status &= ~0x40;
            if (sel == 2) status &= ~0x10;
            if (sel == 3) status = 0;
        }
        clock_sleep(sel == 3 ? 100 : 50);
        return;
    }
    int code = kernel_getch();
    if (code == 0xffffffff) { // ↑
        sel--;
    } else if (code == 0xfffffffe) { // ↓
        sel++;
    } else if (code == 0x0000000a) { // enter
        is_reset = true;
        if (sel == 4) {
            sel = -1;
            return;
        }
    }
    if (sel > 4)
        sel = 0;
    else if (sel < 0)
        sel = 4;
}

static void update_flush() {
    get_current_proc()->tty->clear(get_current_proc()->tty);
    printk("System restart. \n");
    printk("↑ ↓ Select the item you want to reset.\n");
    printk("Press enter to confirm.\n");
    printk("Menu>>>\n");
    printk("        Audio Device  %s %s\n", sel == 0 ? "<" : " ",
           is_reset        ? (sel == 0        ? (index == 0 || index == 5 ? l[0]
                                                 : index == 1 || index == 6
                                                     ? l[1]
                                                     : (index == 2 || index == 7
                                                            ? l[2]
                                                            : (index == 3 || index == 8 ? l[3] : l[4])))
                              : status & 0x80 ? "\033[31merror\033[0m"
                                              : "")
           : status & 0x80 ? "\033[31merror\033[0m"
                           : "");
    printk("        Camera System %s %s\n", sel == 1 ? "<" : " ",
           is_reset        ? (sel == 1        ? (index == 0 || index == 5 ? l[0]
                                                 : index == 1 || index == 6
                                                     ? l[1]
                                                     : (index == 2 || index == 7
                                                            ? l[2]
                                                            : (index == 3 || index == 8 ? l[3] : l[4])))
                              : status & 0x40 ? "\033[31merror\033[0m"
                                              : "")
           : status & 0x40 ? "\033[31merror\033[0m"
                           : "");
    printk("        Ventilation   %s %s\n", sel == 2 ? "<" : " ",
           is_reset        ? (sel == 2        ? (index == 0 || index == 5 ? l[0]
                                                 : index == 1 || index == 6
                                                     ? l[1]
                                                     : (index == 2 || index == 7
                                                            ? l[2]
                                                            : (index == 3 || index == 8 ? l[3] : l[4])))
                              : status & 0x10 ? "\033[31merror\033[0m"
                                              : "")
           : status & 0x10 ? "\033[31merror\033[0m"
                           : "");
    printk("\n");
    printk("        Reboot All    %s %s\n", sel == 3 ? "<" : " ",
           is_reset ? (sel == 3 ? (index == 0 || index == 5 ? l[0]
                                   : index == 1 || index == 6
                                       ? l[1]
                                       : (index == 2 || index == 7
                                              ? l[2]
                                              : (index == 3 || index == 8 ? l[3] : l[4])))
                                : "")
                    : "");
    printk("        Exit          %s\n", sel == 4 ? "<" : "");
}

void f3system_setup() {
    terminal_destroy();
    terminal_setup(true);
    update_flush();
    infinite_loop {
        key();
        if (sel == -1) break;
        update_flush();
    }
    disable_scheduler();
    get_current_proc()->tty->clear(get_current_proc()->tty);
    terminal_destroy();
    terminal_setup(false);
    create_kernel_thread((void *)setup_shell, NULL, "Shell");
    klogf(true, "Enable kernel shell service.\n");
    enable_scheduler();
}