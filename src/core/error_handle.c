#include "description_table.h"
#include "kprint.h"
#include "krlibc.h"

void double_fault() {
    kerror("Double fault");
    cpu_hlt;
}

void divede_error() {
    kerror("Divide by zero error");
    cpu_hlt;
}

void segment_not_present() {
    kerror("Segment not present");
    cpu_hlt;
}

void invalid_opcode() {
    kerror("Invalid opcode");
    cpu_hlt;
}

void general_protection_fault() {
    kerror("General protection fault");
    cpu_hlt;
}

void error_setup() {
    register_interrupt_handler(8, (void *) double_fault, 1, 0x8E);
    register_interrupt_handler(0, (void *) divede_error, 0, 0x8E);
    register_interrupt_handler(11, (void *) segment_not_present, 0, 0x8E);
    register_interrupt_handler(6, (void *) invalid_opcode, 0, 0x8E);
    register_interrupt_handler(13, (void *) general_protection_fault, 0, 0x8E);
}
