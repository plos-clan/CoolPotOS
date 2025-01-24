#include "tty.h"
#include "limine.h"
#include "gop.h"
#include "alloc.h"
#include "os_terminal.h"

tty_t defualt_tty;

static void tty_kernel_print(tty_t *tty,const char* msg){
    terminal_process(msg);
}

static void tty_kernel_putc(tty_t *tty,int c){
    terminal_process_char(c);
}

tty_t *alloc_default_tty(){
    tty_t *tty = malloc(sizeof(tty_t));
    tty->video_ram = framebuffer->address;
    tty->height = framebuffer->height;
    tty->width = framebuffer->width;
    tty->print = tty_kernel_print;
    tty->putchar = tty_kernel_putc;
    tty->keyboard_buffer = create_queue();
    return tty;
}

void free_tty(tty_t *tty){
    destroy_queue(tty->keyboard_buffer);
    free(tty);
}

tty_t *get_default_tty(){
    return &defualt_tty;
}

void init_tty(){
    defualt_tty.video_ram = framebuffer->address;
    defualt_tty.width = framebuffer->width;
    defualt_tty.height = framebuffer->height;
    defualt_tty.keyboard_buffer = create_queue();
    defualt_tty.print = tty_kernel_print;
    defualt_tty.putchar = tty_kernel_putc;
}
