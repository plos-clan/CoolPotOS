#include "terminal.h"
#include "gop.h"
#include "klog.h"

static void setup_cpos_default(){
    TerminalPalette palette = {
            .background = 0x000000,
            .foreground = 0xffffff,
            .ansi_colors = {
                    [0] = 0x000000,
                    [1] = 0xe96161,
                    [2] = 0x00df00,
                    [3] = 0xdfd300,
                    [4] = 0x002fe2,
                    [5] = 0x5100ff,
                    [6] = 0x377d6e,
                    [7] = 0xc6c6c6,

                    [8] = 0x111111,
                    [9] = 0xdf1200,
                    [10] = 0x00df00,
                    [11] = 0xdfd300,
                    [12] = 0x0036ff,
                    [13] = 0x5100ff,
                    [14] = 0x377d6e,
                    [15] = 0xffffff
            }
    };
    terminal_set_custom_color_scheme(&palette);
}

void init_terminal() {
    TerminalDisplay display = {
            .width = framebuffer->width,
            .height = framebuffer->height,
            .address = framebuffer->address
    };
    float size = 10.0f * ((float) framebuffer->width / 1024);
    terminal_init(&display, size, malloc, free, NULL);
    setup_cpos_default();
}
