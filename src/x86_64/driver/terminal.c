#include "terminal.h"
#include "atom_queue.h"
#include "gop.h"
#include "krlibc.h"
#include "lock.h"
#include "timer.h"

atom_queue *output_buffer;
bool        open_flush = false;
ticketlock  terminal_lock;

static void setup_cpos_default() {
    TerminalPalette palette = {
            .background  = 0x0d0d1a,
            .foreground  = 0xeaeaea,

            .ansi_colors = {
                    0x0d0d1a, 0xe84a5f, 0x50fa7b, 0xfacc60,
                    0x61aeee, 0xc074ec, 0x40e0d0, 0xbebec2,
                    0x2f2f38, 0xff6f91, 0x8affc1, 0xffe99b,
                    0x9ddfff, 0xd69fff, 0xb2ffff, 0xffffff
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
        nsleep(100);
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
