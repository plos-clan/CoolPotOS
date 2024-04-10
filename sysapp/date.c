#include "../include/date.h"
#include "../include/cmos.h"
#include "../include/vga.h"
#include "../include/timer.h"

extern uint16_t *terminal_buffer;
extern uint8_t terminal_color;

int setup_date(){
    char* date_info;
    int i;
    while (1){
        clock_sleep(5);
        date_info = get_date_time(); //11
        i = 0;
        for(size_t x = VGA_WIDTH - 19; x < VGA_WIDTH ; x++){
            const size_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(date_info[i],terminal_color);
            i++;
        }
    }
    return 0;
}
