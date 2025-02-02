#include "terminal.h"
#include "gop.h"
#include "klog.h"
#include "atom_queue.h"
#include "krlibc.h"
#include "timer.h"

atom_queue *output_buffer;
bool open_flush = false;

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

void update_terminal(){
    while (true){
        char a = atom_pop(output_buffer);
        if(a == -1) break;
        terminal_process_char(a);
    }
    terminal_flush();
}

int terminal_flush_service(void *pVoid) {
    terminal_set_auto_flush(0);
    open_flush = true;
    while (1){
        update_terminal();
        usleep(100);
    }
    return 0;
}

void terminal_putc(char c){
    atom_push(output_buffer,c);
}

void terminal_puts(const char* msg){
    for (int i = 0; i < strlen(msg); ++i) {
        terminal_putc(msg[i]);
    }
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
    output_buffer = create_atom_queue(2048);
}
