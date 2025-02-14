#include "tty.h"
#include "limine.h"
#include "gop.h"
#include "alloc.h"
#include "terminal.h"
#include "mpmc_queue.h"
#include "kprint.h"

tty_t defualt_tty;
mpmc_queue_t *queue;

extern bool open_flush; //terminal.c

static void tty_kernel_print(tty_t *tty,const char* msg){
    if(open_flush)
        terminal_puts(msg);
    else{
        if(queue == NULL){

        }
        terminal_process(msg);
    }
}

static void tty_kernel_putc(tty_t *tty,int c){
    if(open_flush)
        terminal_putc((char)c);
    else terminal_process_char((char)c);
}

tty_t *alloc_default_tty(){
    tty_t *tty = malloc(sizeof(tty_t));
    tty->video_ram = framebuffer->address;
    tty->height = framebuffer->height;
    tty->width = framebuffer->width;
    tty->print = tty_kernel_print;
    tty->putchar = tty_kernel_putc;
    tty->keyboard_buffer = create_atom_queue(1024);
    return tty;
}

void free_tty(tty_t *tty){
    free_queue(tty->keyboard_buffer);
    free(tty);
}

tty_t *get_default_tty(){
    return &defualt_tty;
}

void init_tty(){
    defualt_tty.video_ram = framebuffer->address;
    defualt_tty.width = framebuffer->width;
    defualt_tty.height = framebuffer->height;
    defualt_tty.keyboard_buffer = create_atom_queue(1024);
    defualt_tty.print = tty_kernel_print;
    defualt_tty.putchar = tty_kernel_putc;
    queue = malloc(sizeof(mpmc_queue_t));
    if(init(queue,1024, sizeof(uint32_t),FAIL) != SUCCESS){
        kerror("Cannot enable mppmc buffer\n");
        free(queue);
        queue = NULL;
    }
}
