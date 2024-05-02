#include "../include/pcat.h"
#include "../include/graphics.h"
#include "../include/keyboard.h"
#include "../include/date.h"
#include "../include/shell.h"
#include "../include/common.h"

struct task_struct *father_pcat;
struct pcat_process *this_process;
uint32_t pid_pcat;
extern KEY_STATUS *key_status;
extern uint16_t *terminal_buffer;
extern uint16_t cursor_x, cursor_y; // 光标位置
extern int date_pid;

static void pcat_movcur(){
    cursor_x = this_process->buf_x;
    cursor_y = this_process->buf_y;
    move_cursor();
}

static void pcat_char(char c){
    uint16_t *location;
    uint8_t attribute = vga_entry_color(VGA_COLOR_DARK_GREY,VGA_COLOR_BLACK);
    if(c == '\n'){
        this_process->buf_y++;
        this_process->buf_x = 0;
        pcat_movcur();
        return;
    } else if (c == 0x09) {
        location = terminal_buffer + (this_process->buf_y * 80 + this_process->buf_x);
        *location = ' ' | attribute;
        this_process->buf_x = (this_process->buf_x + 8) & ~(8 - 1);
        pcat_movcur();
        return;
    } else if (c == '\b') {
        if(this_process->buf_x){
            this_process->buf_x--;
            location = terminal_buffer + (this_process->buf_y * 80 + this_process->buf_x);
            *location = ' ' | attribute;
        } else{
            if(this_process->buf_y){
                this_process->buf_y--;
            }
        }
        char* cc = (char*) this_process->buffer_screen;
        int i = (this_process->chars)--;
        cc[i] = '\0';
        pcat_movcur();
        return;
    }

    vga_putentryat(c, attribute,this_process->buf_x,this_process->buf_y);
    this_process->buf_x++;
    if (this_process->buf_x >= 80) {
        this_process->buf_x = 0;
        this_process->buf_y++;
    }
    pcat_movcur();
    char* cc = (char*) this_process->buffer_screen;
    int i = (this_process->chars)++;
    cc[i] = c;
}

static void draw_string(const char *data){
    size_t size = strlen(data);
    for (size_t i = 0; i < size; i++)
        pcat_char(data[i]);
}

static void draw_menu(){
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        vga_putentryat(' ',vga_entry_color(VGA_COLOR_BLACK,VGA_COLOR_LIGHT_GREY),x,(VGA_HEIGHT - 1));
    }
}

static int input_handler(){
    int index = 0;
    char c;
    while (1) {
        c = getc();

        if(c == 27){
            this_process->keys = 1;
            return 0;
        }

        if (c == '\b') {
            if (index > 0) {
                index--;
                draw_string("\b \b");
            }
        } else {
            index++;
            pcat_char(c);
        }
    }
}

static int check_exit(){
    return this_process->keys;
}

void pcat_launch(struct File *file){
    vga_clear(); task_kill(date_pid); vga_clear();
    this_process = (struct pcat_process*) kmalloc(sizeof(struct pcat_process));
    this_process->buf_x = this_process->buf_y = 0;
    this_process->line = 1;
    this_process->chars = 0;
    this_process->buffer_screen = (uint16_t) kmalloc(VGA_WIDTH * (VGA_HEIGHT - 1));
    this_process->file = file;
    this_process->keys = 0;

    int pid = kernel_thread(input_handler,NULL,"CPOS-pcat");

    while (1){
        if(check_exit()){
            break;
        }
        draw_menu();
        input_handler();
    }
    task_kill(pid);
    vga_clear();
    date_pid = kernel_thread(setup_date, NULL, "CPOS-Date");
}