#include "terminal.h"
#include "atom_queue.h"
#include "gop.h"
#include "krlibc.h"
#include "lock.h"
#include "module.h"
#include "timer.h"

atom_queue *output_buffer;
bool        open_flush = false;
ticketlock  terminal_lock;

static void setup_cpos_default() {
    TerminalPalette palette = { //特别更新配色方案
            .background  = 0x1e1b29,  // 深靛紫 - 夜猫底
            .foreground  = 0xfafaff,  // 奶油白 - 轻柔文字

            .ansi_colors = {
                    [0]  = 0x1e1b29,  // black - 深底色
                    [1]  = 0xff6b81,  // red - 草莓粉（错误提示）
                    [2]  = 0xa7f0ba,  // green - 奶绿（成功）
                    [3]  = 0xffd47e,  // yellow - 柔焦奶油黄
                    [4]  = 0xa0d8ef,  // blue - 天空蓝
                    [5]  = 0xd7aefb,  // magenta - 薰衣紫
                    [6]  = 0x9bf6e1,  // cyan - 薄荷青
                    [7]  = 0xececec,  // white - 柔白灰

                    [8]  = 0x39324b,  // bright black - 低亮背景衬色
                    [9]  = 0xff5e6e,  // bright red - 闪亮莓粉
                    [10] = 0xc1ffd7,  // bright green - 萌草绿
                    [11] = 0xffeb99,  // bright yellow - 糖黄
                    [12] = 0xb8e0ff,  // bright blue - 果冻蓝
                    [13] = 0xf6b3f6,  // bright magenta - 樱花紫
                    [14] = 0xadf0f0,  // bright cyan - 奶薄荷蓝
                    [15] = 0xffffff   // bright white - 纯白
            }
    };

    terminal_set_custom_color_scheme(&palette);
}

void update_terminal() {
    ticket_lock(&terminal_lock);
    while (true) {
        char a = atom_pop(output_buffer);
        if (a == -1) break;
        terminal_process_char(a);
    }
    terminal_flush();
    ticket_unlock(&terminal_lock);
}

int terminal_flush_service(void *pVoid) {
    terminal_set_auto_flush(0);
    open_flush = true;
    infinite_loop {
        update_terminal();
        usleep(100);
    }
    return 0;
}

void terminal_putc(char c) {
    atom_push(output_buffer, c);
}

void terminal_puts(const char *msg) {
    for (size_t i = 0; i < strlen(msg); ++i) {
        terminal_putc(msg[i]);
    }
}

void init_terminal() {
    TerminalDisplay display = {.width   = framebuffer->width,
                               .height  = framebuffer->height,
                               .address = framebuffer->address};
    float           size    = 10.0f * ((float)framebuffer->width / 1024);

    //    cp_module_t *mod = get_module("sysfont");
    //    if(mod == NULL){
    //        logkf("Error: no default terminal font.\n");
    //        cpu_hlt;
    //    }
    terminal_init(&display, size, malloc, free, NULL);

    terminal_set_scroll_speed(3);
    setup_cpos_default();
    output_buffer = create_atom_queue(2048);
}
