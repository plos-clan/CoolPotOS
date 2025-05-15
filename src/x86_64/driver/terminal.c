#include "terminal.h"
#include "atom_queue.h"
#include "gop.h"
#include "keyboard.h"
#include "klog.h"
#include "krlibc.h"
#include "lock.h"
#include "timer.h"

atom_queue *output_buffer;
bool        open_flush = false;
spin_t      terminal_lock;

// temporary alternative to handle unsupported keys
atom_queue *temp_keyboard_buffer;

void update_terminal() {
    spin_lock(terminal_lock);
    bool need_flush = false;
    loop {
        int ch = atom_pop(output_buffer);
        if (ch == -1) break;
        need_flush = true;
        if (ch == '\n') { terminal_process_byte('\r'); }
        terminal_process_byte(ch);
    }
    if (open_flush && need_flush) { terminal_flush(); }
    spin_unlock(terminal_lock);
}

void terminal_open_flush() {
    open_flush = true;
}

void terminal_close_flush() {
    open_flush = false;
}

int terminal_flush_service(void *pVoid) {
    //    terminal_set_auto_flush(false);
    open_flush = true;
    loop update_terminal();
    return 0;
}

void terminal_putc(char ch) {
    atom_push(output_buffer, ch);
}

void terminal_puts(const char *msg) {
    while (*msg != '\0') {
        atom_push(output_buffer, *msg);
        msg++;
    }
}

void terminal_pty_writer(const uint8_t *data) {
    while (*data != '\0') {
        atom_push(temp_keyboard_buffer, *data);
        data++;
    }
}

void init_terminal() {
    TerminalDisplay display = {.width            = framebuffer->width,
                               .height           = framebuffer->height,
                               .buffer           = framebuffer->address,
                               .blue_mask_shift  = framebuffer->blue_mask_shift,
                               .blue_mask_size   = framebuffer->blue_mask_size,
                               .green_mask_shift = framebuffer->green_mask_shift,
                               .green_mask_size  = framebuffer->green_mask_size,
                               .red_mask_shift   = framebuffer->red_mask_shift,
                               .red_mask_size    = framebuffer->red_mask_size,
                               .pitch            = framebuffer->pitch};
    float           size    = 16.0f * ((float)framebuffer->width / 1920);

    //    cp_module_t *mod = get_module("sysfont");
    //    if(mod == NULL){
    //        logkf("Error: no default terminal font.\n");
    //        cpu_hlt;
    //    }

    terminal_init(&display, size, malloc, free);
    // terminal_set_crnl_mapping(true);
    terminal_set_scroll_speed(3);
    terminal_set_pty_writer(terminal_pty_writer);
    terminal_set_auto_flush(false);

    TerminalPalette palette = {
        .background = 0x0d0d1a,
        .foreground = 0xeaeaea,

        .ansi_colors = {0x0d0d1a, 0xe84a5f, 0x50fa7b, 0xfacc60, 0x61aeee, 0xc074ec, 0x40e0d0,
                        0xbebec2, 0x2f2f38, 0xff6f91, 0x8affc1, 0xffe99b, 0x9ddfff, 0xd69fff,
                        0xb2ffff, 0xffffff}
    };
    terminal_set_custom_color_scheme(&palette);

    output_buffer        = create_atom_queue(2048);
    temp_keyboard_buffer = create_atom_queue(256);
}
