#include "pcnet.h"
#include "arp.h"
#include "dhcp.h"
#include "etherframe.h"
#include "io.h"
#include "ipv4.h"
#include "isr.h"
#include "klog.h"
#include "kmalloc.h"
#include "krlibc.h"
#include "pci.h"
#include "tcp.h"
#include "timer.h"
#include "udp.h"

extern uint32_t gateway, submask, dns, ip, dhcp_ip;
extern uint8_t  mac0, mac1, mac2, mac3, mac4, mac5;

static uint32_t      pcnet_io_base;
static pci_device_t *device;

static uint8_t currentSendBuffer;
static uint8_t currentRecvBuffer;
static uint8_t sendBufferDescMemory[2048 + 15];
static uint8_t sendBuffers[8][2048 + 15];
static uint8_t recvBufferDescMemory[2048 + 15];
static uint8_t recvBuffers[8][2048 + 15];

struct InitializationBlock      initBlock;
static struct BufferDescriptor *sendBufferDesc;
static struct BufferDescriptor *recvBufferDesc;
static uint8_t *IP_Packet_Base[16] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                                      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

int recv = 0;

static void reset_card() {
    // PCNET卡复位（约等于切换到16位读写模式
    io_in16(pcnet_io_base + RESET16);
    io_out16(pcnet_io_base + RESET16, 0x00);
    clock_sleep(1);
}

static uint32_t Find_IP_Packet(uint16_t ident) {
    for (int i = 0; i != 16; i++) {
        if (IP_Packet_Base[i] != NULL) {
            struct IPV4Message *ipv4 =
                (struct IPV4Message *)(IP_Packet_Base[i] + sizeof(struct EthernetFrame_head));
            if (swap16(ipv4->ident) == ident) { return i; }
        }
    }
    return -1;
}

static void IP_Assembling(struct IPV4Message *ipv4, unsigned char *RawData) {
    uint32_t            i_p = Find_IP_Packet(swap16(ipv4->ident));
    struct IPV4Message *ipv4_p =
        (struct IPV4Message *)(IP_Packet_Base[i_p] + sizeof(struct EthernetFrame_head));
    uint32_t size_p     = swap16(ipv4_p->totalLength);
    ipv4_p->totalLength = swap16(swap16(ipv4->totalLength) + swap16(ipv4_p->totalLength) -
                                 sizeof(struct IPV4Message));
    IP_Packet_Base[i_p] =
        (uint8_t *)krealloc((void *)IP_Packet_Base[i_p], swap16(ipv4_p->totalLength));
    memcpy((void *)(IP_Packet_Base[i_p] + size_p),
           RawData + sizeof(struct EthernetFrame_head) + sizeof(struct IPV4Message),
           swap16(ipv4->totalLength) - sizeof(struct IPV4Message));
    return;
}

void Activate() {
    // 激活PCNET IRQ中断
    io_out16(pcnet_io_base + RAP16, CSR0);

    io_out16(pcnet_io_base + RDP16, 0x41);
    io_out16(pcnet_io_base + RAP16, CSR4);
    uint32_t temp = io_in16(pcnet_io_base + RDP16);
    io_out16(pcnet_io_base + RAP16, CSR4);
    io_out16(pcnet_io_base + RDP16, temp | 0xc00);

    io_out16(pcnet_io_base + RAP16, CSR0);
    io_out16(pcnet_io_base + RDP16, 0x42);
}

static void PCNET_IRQ(registers_t *reg) {
    io_out16(pcnet_io_base + RAP16, CSR0);

    uint16_t temp = io_in16(pcnet_io_base + RDP16);

    if ((temp & 0x0400) == 0x0400) Recv();

    io_out16(pcnet_io_base + RAP16, CSR0);
    io_out16(pcnet_io_base + RDP16, temp); // 通知PCNET网卡 中断处理完毕

    if ((temp & 0x0100) == 0x0100) {
        io_out8(0xa0, (0x60 + pci_get_drive_irq(device->bus, device->slot, device->func) - 0x8));
    }

    io_out8(0x20, 0x62);
}

void init_pcnet_card() {
    currentSendBuffer = 0;
    currentRecvBuffer = 0;

    mac0 = io_in8(pcnet_io_base + APROM0);
    mac1 = io_in8(pcnet_io_base + APROM1);
    mac2 = io_in8(pcnet_io_base + APROM2);
    mac3 = io_in8(pcnet_io_base + APROM3);
    mac4 = io_in8(pcnet_io_base + APROM4);
    mac5 = io_in8(pcnet_io_base + APROM5);

    reset_card();
    io_out16(pcnet_io_base + RAP16, BCR20);
    io_out16(pcnet_io_base + BDP16, 0x102);
    io_out16(pcnet_io_base + RAP16, CSR0);
    io_out16(pcnet_io_base + RDP16, 0x0004); // 暂时停止所有传输,用于初始化PCNET网卡

    initBlock.mode                    = 0;
    initBlock.reserved1numSendBuffers = (0 << 4) | 3; // 高4位是reserved1 低4位是numSendBuffers
    initBlock.reserved2numRecvBuffers = (0 << 4) | 3; // 高4位是reserved2 低4位是numRecvBuffers
    initBlock.mac0                    = mac0;
    initBlock.mac1                    = mac1;
    initBlock.mac2                    = mac2;
    initBlock.mac3                    = mac3;
    initBlock.mac4                    = mac4;
    initBlock.mac5                    = mac5;
    initBlock.reserved3               = 0;
    initBlock.logicalAddress          = 0;

    sendBufferDesc =
        (struct BufferDescriptor *)(((uint32_t)&sendBufferDescMemory[0] + 15) & 0xfffffff0);
    initBlock.sendBufferDescAddress = (uint32_t)sendBufferDesc;
    recvBufferDesc =
        (struct BufferDescriptor *)(((uint32_t)&recvBufferDescMemory[0] + 15) & 0xfffffff0);
    initBlock.recvBufferDescAddress = (uint32_t)recvBufferDesc;

    for (uint8_t i = 0; i < 8; i++) {
        sendBufferDesc[i].address = (((uint32_t)&sendBuffers[i] + 15) & 0xfffffff0);
        sendBufferDesc[i].flags   = 0xf7ff;
        sendBufferDesc[i].flags2  = 0;
        sendBufferDesc[i].avail   = 0;

        recvBufferDesc[i].address = (((uint32_t)&recvBuffers[i] + 15) & 0xfffffff0);
        recvBufferDesc[i].flags   = 0xf7ff | 0x80000000;
        recvBufferDesc[i].flags2  = 0;
        recvBufferDesc[i].avail   = 0;
        memclean((char *)recvBufferDesc[i].address, 2048);
    }

    io_out16(pcnet_io_base + RAP16, CSR1);
    io_out16(pcnet_io_base + RDP16, (uint16_t)&initBlock);
    io_out16(pcnet_io_base + RAP16, CSR2);
    io_out16(pcnet_io_base + RDP16, (uint32_t)&initBlock >> 16);

    Activate();

    initBlock.logicalAddress = 0xFFFFFFFF;
    ip                       = 0xFFFFFFFF;
    gateway                  = 0xFFFFFFFF;
    submask                  = 0xFFFFFFFF;
    dns                      = 0xFFFFFFFF;

    logkf("DHCP DISCOVERY %08x %08x %08x %08x %08x %08x\n", mac0, mac1, mac2, mac3, mac4, mac5);
    dhcp_discovery(&mac0);

    while (gateway == 0xFFFFFFFF && submask == 0xFFFFFFFF && dns == 0xFFFFFFFF &&
           ip == 0xFFFFFFFF) {
        initBlock.logicalAddress = ip;
    }
}

void Card_Recv_Handler(uint8_t *RawData) {
    struct EthernetFrame_head *header = (struct EthernetFrame_head *)(RawData);
    if (header->type == swap16(IP_PROTOCOL)) { // IP数据报
        struct IPV4Message *ipv4 =
            (struct IPV4Message *)(RawData + sizeof(struct EthernetFrame_head));
        if (ipv4->version == 4) {
            if ((swap16(ipv4->flagsAndOffset) >> IP_MF) & 1) {
                if (Find_IP_Packet(swap16(ipv4->ident)) == -1) {
                    for (int i = 0; i != 16; i++) {
                        if (IP_Packet_Base[i] == NULL) {
                            IP_Packet_Base[i] = (uint8_t *)kmalloc(
                                swap16(ipv4->totalLength) + sizeof(struct EthernetFrame_head));
                            memcpy((void *)IP_Packet_Base[i], RawData,
                                   swap16(ipv4->totalLength) + sizeof(struct EthernetFrame_head));
                            break;
                        }
                    }
                } else {
                    IP_Assembling(ipv4, RawData);
                }
            } else if (!((swap16(ipv4->flagsAndOffset) >> IP_MF) & 1)) {
                uint32_t i_p  = Find_IP_Packet(swap16(ipv4->ident));
                void    *base = RawData;
                if (i_p != -1) {
                    IP_Assembling(ipv4, RawData);
                    base = (void *)IP_Packet_Base[i_p];
                }
                // if (ipv4->protocol == ICMP_PROTOCOL) {  // ICMP
                // icmp_handler(base);
                /*} else*/ if (ipv4->protocol == UDP_PROTOCOL) { // UDP
                    udp_handler(base);
                } else if (ipv4->protocol == TCP_PROTOCOL) { // TCP
                    tcp_handler(base);
                }
                if (i_p != -1) {
                    kfree((void *)IP_Packet_Base[i_p]);
                    IP_Packet_Base[i_p] = NULL;
                }
            }
        }
    } else if (header->type == swap16(ARP_PROTOCOL)) { // ARP
        arp_handler(RawData);
    }
}

void Recv() {
    recv = 1;
    for (; (recvBufferDesc[currentRecvBuffer].flags & 0x80000000) == 0;
         currentRecvBuffer = (currentRecvBuffer + 1) % 8) {
        if (!(recvBufferDesc[currentRecvBuffer].flags & 0x40000000) &&
            (recvBufferDesc[currentRecvBuffer].flags & 0x03000000) == 0x03000000) {
            uint32_t size = recvBufferDesc[currentRecvBuffer].flags & 0xfff;
            if (size > 128) size -= 4;

            uint8_t *buffer = (uint8_t *)(recvBufferDesc[currentRecvBuffer].address);
            //            for (int i = 0; i < (size > 128 ? 128 : size); i++) {
            //                printk("%02x ", buffer[i]);
            //            }
            //            printk("\n");
        }
        recv              = 0;
        currentRecvBuffer = 0;
        Card_Recv_Handler((uint8_t *)recvBufferDesc[currentRecvBuffer].address);

        memclean((char *)recvBufferDesc[currentRecvBuffer].address, 2048);
        recvBufferDesc[currentRecvBuffer].flags2 = 0;
        recvBufferDesc[currentRecvBuffer].flags  = 0x8000f7ff;
    }
    currentRecvBuffer = 0;
}

void PcnetSend(uint8_t *buffer, int size) {
    while (recv)
        ;
    int sendDesc      = currentSendBuffer;
    currentSendBuffer = (currentSendBuffer + 1) % 8;
    memclean((char *)sendBufferDesc[currentSendBuffer].address, 2048);
    if (size > MTU + sizeof(struct EthernetFrame_head) + sizeof(struct EthernetFrame_tail))
        size = MTU + sizeof(struct EthernetFrame_head) + sizeof(struct EthernetFrame_tail);

    for (uint8_t *src = buffer + size - 1,
                 *dst = (uint8_t *)(sendBufferDesc[sendDesc].address + size - 1);
         src >= buffer; src--, dst--)
        *dst = *src;

    //printk("SENDING: ");
    for (int i = 0; i < (size > 128 ? 128 : size); i++) {
        //printk("%02x ", buffer[i]);
    }
    //printk("\n");
    sendBufferDesc[sendDesc].avail  = 0;
    sendBufferDesc[sendDesc].flags  = 0x8300f000 | ((uint16_t)((-size) & 0xfff));
    sendBufferDesc[sendDesc].flags2 = 0;

    io_out16(pcnet_io_base + RAP16, CSR0);
    io_out16(pcnet_io_base + RDP16, 0x48);

    currentSendBuffer = 0;
}

void pcnet_setup() {
    device = pci_find_vid_did(0x1022, 0x2000);
    if (device == NULL) {
        device = pci_find_vid_did(0x1022, 0x2001);
        if (device == NULL) {
            klogf(false, "Cannot find pcnet.\n");
            return;
        }
    }
    klogf(true, "Loading pcnet driver...\n");

    register_interrupt_handler(pci_get_drive_irq(device->bus, device->slot, device->func) + 0x20,
                               PCNET_IRQ);

    //启用IO端口和总线主控
    uint32_t conf  = pci_read_command_status(device->bus, device->slot, device->func);
    conf          &= 0xffff0000;
    conf          |= 0x5;
    pci_write_command_status(device->bus, device->slot, device->func, conf);

    pcnet_io_base = pci_get_port_base(device->bus, device->slot, device->func);
    init_pcnet_card();
}
