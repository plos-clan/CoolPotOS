#pragma once

// E1000 Device IDs
#define E1000_DEVICE_ID_82540EM 0x100E
#define MAX_E1000_DEVICES       8

// E1000 Register Definitions
#define E1000_CTRL     0x0000 // Device Control
#define E1000_STATUS   0x0008 // Device Status
#define E1000_EERD     0x0014 // EEPROM Read
#define E1000_CTRL_EXT 0x0018 // Extended Device Control
#define E1000_ICR      0x00C0 // Interrupt Cause Read
#define E1000_IMS      0x00D0 // Interrupt Mask Set
#define E1000_IMC      0x00D8 // Interrupt Mask Clear
#define E1000_RCTL     0x0100 // RX Control
#define E1000_TCTL     0x0400 // TX Control
#define E1000_TIPG     0x0410 // TX Inter-packet Gap
#define E1000_RDBAL    0x2800 // RX Descriptor Base Address Low
#define E1000_RDBAH    0x2804 // RX Descriptor Base Address High
#define E1000_RDLEN    0x2808 // RX Descriptor Length
#define E1000_RDH      0x2810 // RX Descriptor Head
#define E1000_RDT      0x2818 // RX Descriptor Tail
#define E1000_RDTR     0x2820 // RX Delay Timer
#define E1000_TDBAL    0x3800 // TX Descriptor Base Address Low
#define E1000_TDBAH    0x3804 // TX Descriptor Base Address High
#define E1000_TDLEN    0x3808 // TX Descriptor Length
#define E1000_TDH      0x3810 // TX Descriptor Head
#define E1000_TDT      0x3818 // TX Descriptor Tail
#define E1000_TIDV     0x3820 // TX Interrupt Delay Value
#define E1000_RA       0x5400 // Receive Address (MAC)

// Control Register Bits
#define E1000_CTRL_RST     (1 << 26) // Reset
#define E1000_CTRL_ASDE    (1 << 5)  // Auto-Speed Detection Enable
#define E1000_CTRL_SLU     (1 << 6)  // Set Link Up
#define E1000_CTRL_FRCSPD  (1 << 11) // Force Speed
#define E1000_CTRL_FRCDPLX (1 << 12) // Force Duplex

// RCTL Register Bits
#define E1000_RCTL_EN         (1 << 1)   // Enable
#define E1000_RCTL_SBP        (1 << 2)   // Store Bad Packets
#define E1000_RCTL_UPE        (1 << 3)   // Unicast Promiscuous Enable
#define E1000_RCTL_MPE        (1 << 4)   // Multicast Promiscuous Enable
#define E1000_RCTL_LPE        (1 << 5)   // Long Packet Enable
#define E1000_RCTL_LBM_NONE   (0 << 6)   // No Loopback
#define E1000_RCTL_RDMTS_HALF (0 << 8)   // RX Desc Min Threshold Size
#define E1000_RCTL_MO_36      (36 << 12) // Multicast Offset
#define E1000_RCTL_BAM        (1 << 15)  // Broadcast Accept Mode
#define E1000_RCTL_SECRC      (1 << 26)  // Strip Ethernet CRC

// TCTL Register Bits
#define E1000_TCTL_EN         (1 << 1)  // Enable
#define E1000_TCTL_PSP        (1 << 3)  // Pad Short Packets
#define E1000_TCTL_CT_SHIFT   4         // Collision Threshold
#define E1000_TCTL_COLD_SHIFT 12        // Collision Distance
#define E1000_TCTL_SWXOFF     (1 << 22) // Software XOFF Transmission

// EERD Register Bits
#define E1000_EERD_START      (1 << 0) // Start Read
#define E1000_EERD_DONE       (1 << 1) // Read Done
#define E1000_EERD_ADDR_SHIFT 2        // Address Shift
#define E1000_EERD_DATA_SHIFT 16       // Data Shift

// RX Descriptor Status Bits
#define E1000_RXD_STAT_DD  (1 << 0) // Descriptor Done
#define E1000_RXD_STAT_EOP (1 << 1) // End of Packet
#define E1000_RXD_ERR_CE   (1 << 0) // CRC Error
#define E1000_RXD_ERR_SE   (1 << 1) // Symbol Error
#define E1000_RXD_ERR_SEQ  (1 << 2) // Sequence Error
#define E1000_RXD_ERR_CXE  (1 << 3) // Carrier Extension Error
#define E1000_RXD_ERR_RXE  (1 << 4) // RX Data Error

// TX Descriptor Command Bits
#define E1000_TXD_CMD_EOP  (1 << 0) // End of Packet
#define E1000_TXD_CMD_IFCS (1 << 1) // Insert FCS
#define E1000_TXD_CMD_RS   (1 << 3) // Report Status

// TX Descriptor Status Bits
#define E1000_TXD_STAT_DD (1 << 0) // Descriptor Done

// Constants
#define E1000_NUM_RX_DESC    32
#define E1000_NUM_TX_DESC    32
#define E1000_RX_BUFFER_SIZE 2048
#define E1000_TX_BUFFER_SIZE 2048
#define E1000_MTU            1500

#include "cp_kernel.h"

// RX Descriptor Structure
struct e1000_rx_desc {
    uint64_t buffer_addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed));

// TX Descriptor Structure
struct e1000_tx_desc {
    uint64_t buffer_addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed));

// E1000 Device Structure
typedef struct e1000_device {
    void    *mmio_base;
    uint8_t  mac[6];
    uint32_t mtu;

    // RX descriptors and buffers
    struct e1000_rx_desc *rx_descs;
    void                 *rx_buffers[E1000_NUM_RX_DESC];
    uint16_t              rx_tail;

    // TX descriptors and buffers
    struct e1000_tx_desc *tx_descs;
    void                 *tx_buffers[E1000_NUM_TX_DESC];
    uint16_t              tx_head;
    uint16_t              tx_tail;
} e1000_device_t;

bool    e1000_has_packets(e1000_device_t *dev);
errno_t e1000_send(void *dev_desc, void *data, uint32_t len);
errno_t e1000_receive(void *dev_desc, void *buffer, uint32_t buffer_size);
