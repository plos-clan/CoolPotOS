#include "../include/redpane.h"
#include "../include/console.h"
#include "../include/common.h"
#include "../include/io.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t *buffer= (uint16_t*) 0xB8000;

void show_OK_blue_pane(char *type,char *message){
    terminal_clear();
    asm("cli");

    uint8_t attributeByte = (0b0000100 << 4) | (15 & 0x0F);
    uint16_t blank = 0x20 | (attributeByte << 8); // 0x20 -> 空格这个字，attributeByte << 8 -> 属性位


    for (int i = 0; i < 80 * 25; i++) buffer[i] = blank; // all line 25

    for (int i = 0; i < 3; i++)
        println("\n");
    redpane_println("           :)");
    redpane_println("           Your PC is dead, but it's not because there's a problem with it.");
    redpane_print("           Message: ");
    redpane_print(message);
    redpane_print("\n");
    redpane_print("           ExitType: [");
    redpane_print(type);
    redpane_print("]");
    redpane_print("\n");
    redpane_println("\n");
    redpane_print("           CrashPowerDOS: ");
    redpane_print(CPOS_VERSION "\n");
    redpane_println("           Copyright 2023-2024 by XIAOYI12.");

    for(;;)io_hlt();
}

void show_VGA_red_pane(char *type,char *message,uint32_t hex,registers_t *page){
    terminal_clear();
    asm("cli");

    uint8_t attributeByte = (0b0000100 << 4) | (15 & 0x0F);
    uint16_t blank = 0x20 | (attributeByte << 8); // 0x20 -> 空格这个字，attributeByte << 8 -> 属性位


    for (int i = 0; i < 80 * 25; i++) buffer[i] = blank; // all line 25

    for (int i = 0; i < 3; i++)
        println("\n");
    redpane_println("              :(");
    redpane_println("              Your PC ran into a problem that it couldn't handle.");
      redpane_print("              Message: ");
    redpane_print(message);
    redpane_print("\n");
      redpane_print("              ErrorCode: [");
    redpane_print(type);
    redpane_print("] : ");
    redpane_print_hex(hex);
    redpane_print("\n");
    redpane_println("\n");
      redpane_print("              CrashPowerDOS: ");
    redpane_print(CPOS_VERSION "\n");
    redpane_println("              Copyright 2023-2024 by XIAOYI12.");

    if(page == NULL){

    }


    for(;;)io_hlt();
}