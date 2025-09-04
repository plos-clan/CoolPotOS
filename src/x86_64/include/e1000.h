#pragma once

#include "ctype.h"

typedef struct __packed {
    //
    // Device Control and Status Registers
    //
    volatile uint32_t CTRL;         // 0x0000 - Device Control Register
    volatile uint32_t STATUS;       // 0x0008 - Device Status Register
    volatile uint32_t reserved1[2]; // 0x0004 is non-existent, 0x000C is reserved
    volatile uint32_t CTRL_EXT;     // 0x0018 - Extended Device Control
    volatile uint32_t reserved2[7];

    //
    // Interrupt Registers
    //
    volatile uint32_t ICR; // 0x00C0 - Interrupt Cause Read Register
    volatile uint32_t ITR; // 0x00C4 - Interrupt Throttling Register
    volatile uint32_t ICS; // 0x00C8 - Interrupt Cause Set Register
    volatile uint32_t IMS; // 0x00CC - Interrupt Mask Set Register
    volatile uint32_t reserved3[1];
    volatile uint32_t IMC; // 0x00D8 - Interrupt Mask Clear Register
    volatile uint32_t reserved4[1];

    //
    // Transmit Registers
    //
    volatile uint32_t TCTL; // 0x0400 - Transmit Control Register
    volatile uint32_t reserved5[1];
    volatile uint32_t TDBAL; // 0x3800 - Transmit Descriptor Base Address Low
    volatile uint32_t TDBAH; // 0x3804 - Transmit Descriptor Base Address High
    volatile uint32_t TDLEN; // 0x3808 - Transmit Descriptor Length
    volatile uint32_t TDH;   // 0x3810 - Transmit Descriptor Head
    volatile uint32_t TDT;   // 0x3818 - Transmit Descriptor Tail
    volatile uint32_t reserved6[4];

    //
    // Receive Registers
    //
    volatile uint32_t RCTL; // 0x0100 - Receive Control Register
    volatile uint32_t reserved7[3];
    volatile uint32_t RDBAL; // 0x2800 - Receive Descriptor Base Address Low
    volatile uint32_t RDBAH; // 0x2804 - Receive Descriptor Base Address High
    volatile uint32_t RDLEN; // 0x2808 - Receive Descriptor Length
    volatile uint32_t RDH;   // 0x2810 - Receive Descriptor Head
    volatile uint32_t RDT;   // 0x2818 - Receive Descriptor Tail
    volatile uint32_t reserved8[4];

    //
    // MAC Address Registers
    //
    volatile uint32_t RAL; // 0x5400 - Receive Address Low
    volatile uint32_t RAH; // 0x5404 - Receive Address High
    volatile uint32_t reserved9[24];
} e1000_regs_t;

void e1000_setup();
