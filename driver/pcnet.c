#include "../include/pcnet.h"
#include "../include/io.h"
#include "../include/description_table.h"
#include "../include/pci.h"
#include "../include/timer.h"
#include "../include/memory.h"
#include "../include/printf.h"

uint8_t bus = 255, dev = 255, func = 255;

extern unsigned int PCI_ADDR_BASE;
extern idt_ptr_t idt_ptr;
static int io_base = 0;
static uint8_t sendBufferDescMemory[2048 + 15];
static uint8_t sendBuffers[8][2048 + 15];
static uint8_t currentSendBuffer;
static uint8_t recvBufferDescMemory[2048 + 15];
static uint8_t recvBuffers[8][2048 + 15];
static uint8_t currentRecvBuffer;
uint8_t mac0, mac1, mac2, mac3, mac4, mac5;
struct InitializationBlock initBlock;
static struct BufferDescriptor* sendBufferDesc;
static struct BufferDescriptor* recvBufferDesc;
int recv = 0;

static void set_handler(int IRQ, int addr) {
    register_interrupt_handler(0x20 + IRQ, (int)addr);
}

void into_32bitsRW() {
    io_out16(io_base + RAP16, BCR18);
    uint16_t tmp = io_in16(io_base + BDP16);
    tmp |= 0x80;
    io_out16(io_base + RAP16, BCR18);
    io_out16(io_base + BDP16, tmp);
    // 此时就处于32位读写模式了
}
void into_16bitsRW() {
    // 切换到16位读写模式 与切换到32位读写模式相反
    io_out32(io_base + RAP32, BCR18);
    uint32_t tmp = io_in32(io_base + BDP32);
    tmp &= ~0x80;
    io_out32(io_base + RAP32, BCR18);
    io_out32(io_base + BDP32, tmp);
}

void reset_card() {
    // PCNET卡复位（约等于切换到16位读写模式
    io_in16(io_base + RESET16);
    io_out16(io_base + RESET16, 0x00);
    // 执行完后需等待（sleep(1)）
}

void Activate() {
    // 激活PCNET IRQ中断
    io_out16(io_base + RAP16, CSR0);
    io_out16(io_base + RDP16, 0x41);

    io_out16(io_base + RAP16, CSR4);
    uint32_t temp = io_in16(io_base + RDP16);
    io_out16(io_base + RAP16, CSR4);
    io_out16(io_base + RDP16, temp | 0xc00);

    io_out16(io_base + RAP16, CSR0);
    io_out16(io_base + RDP16, 0x42);
}

int pcnet_find_card() {
    //printk("pcnet_find:");
    PCI_GET_DEVICE(CARD_VENDOR_ID, CARD_DEVICE_ID, &bus, &dev, &func);
    if (bus == 255) {
        //printk("false\n");
        return 0;
    }
    //printk("true");
    return 1;
}
/*
static void init_Card_all() {
    currentSendBuffer = 0;
    currentRecvBuffer = 0;

    // 获取MAC地址并保存
    mac0 = io_in8(io_base + APROM0);
    mac1 = io_in8(io_base + APROM1);
    mac2 = io_in8(io_base + APROM2);
    mac3 = io_in8(io_base + APROM3);
    mac4 = io_in8(io_base + APROM4);
    mac5 = io_in8(io_base + APROM5);
    // printk("MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n", mac0, mac1, mac2,
    // mac3,
    //        mac4, mac5);
    // 这里约等于 into_16bitsRW();
    reset_card();
    clock_sleep(1);

    io_out16(io_base + RAP16, BCR20);
    io_out16(io_base + BDP16, 0x102);
    io_out16(io_base + RAP16, CSR0);
    io_out16(io_base + RDP16, 0x0004);  // 暂时停止所有传输（用于初始化PCNET网卡

    // initBlock传输初始化（CSR1=IB地址低16位，CSR2=IB地址高16位）
    // &
    // Send/Recv环形缓冲区的初始化
    initBlock.mode = 0;
    initBlock.reserved1numSendBuffers =
            (0 << 4) | 3;  // 高4位是reserved1 低4位是numSendBuffers
    initBlock.reserved2numRecvBuffers =
            (0 << 4) | 3;  // 高4位是reserved2 低4位是numRecvBuffers
    initBlock.mac0 = mac0;
    initBlock.mac1 = mac1;
    initBlock.mac2 = mac2;
    initBlock.mac3 = mac3;
    initBlock.mac4 = mac4;
    initBlock.mac5 = mac5;
    initBlock.reserved3 = 0;
    initBlock.logicalAddress = 0;

    sendBufferDesc =
            (struct BufferDescriptor*)(((uint32_t)&sendBufferDescMemory[0] + 15) &
                                       0xfffffff0);
    initBlock.sendBufferDescAddress = (uint32_t)sendBufferDesc;
    recvBufferDesc =
            (struct BufferDescriptor*)(((uint32_t)&recvBufferDescMemory[0] + 15) &
                                       0xfffffff0);
    initBlock.recvBufferDescAddress = (uint32_t)recvBufferDesc;

    for (uint8_t i = 0; i < 8; i++) {
        sendBufferDesc[i].address = (((uint32_t)&sendBuffers[i] + 15) & 0xfffffff0);
        sendBufferDesc[i].flags = 0xf7ff;
        sendBufferDesc[i].flags2 = 0;
        sendBufferDesc[i].avail = 0;

        recvBufferDesc[i].address = (((uint32_t)&recvBuffers[i] + 15) & 0xfffffff0);
        recvBufferDesc[i].flags = 0xf7ff | 0x80000000;
        recvBufferDesc[i].flags2 = 0;
        recvBufferDesc[i].avail = 0;
        memclean(recvBufferDesc[i].address, 2048);
    }
    // CSR1,CSR2赋值（initBlock地址
    io_out16(io_base + RAP16, CSR1);
    io_out16(io_base + RDP16, (uint16_t)&initBlock);
    io_out16(io_base + RAP16, CSR2);
    io_out16(io_base + RDP16, (uint32_t)&initBlock >> 16);

    Activate();

    initBlock.logicalAddress = 0xFFFFFFFF;
    ip = 0xFFFFFFFF;
    gateway = 0xFFFFFFFF;
    submask = 0xFFFFFFFF;
    dns = 0xFFFFFFFF;
    dhcp_discovery(&mac0);
    while (gateway == 0xFFFFFFFF && submask == 0xFFFFFFFF && dns == 0xFFFFFFFF &&
           ip == 0xFFFFFFFF) {
        initBlock.logicalAddress = ip;
    }

    // 初始化ARP表


    // DNSParseIP("baidu.com");

    // UDPProviderSend(0x761ff8d7, initBlock.logicalAddress, 52949, 38,
    //   "来自Powerint DOS 386的消息：我是周志昊！！！", strlen("来自Powerint DOS
    //   386的消息：我是周志昊！！！"));
}
*/
void init_pcnet_card() {
    printf("[\035kernel\036]: Loading pcnet driver.\n");
    // 允许PCNET网卡产生中断
    // 1.注册中断
    register_interrupt_handler(pci_get_drive_irq(bus, dev, func),PCNET_IRQ);
    // 2,写COMMAND和STATUS寄存器
    uint32_t conf = pci_read_command_status(bus, dev, func);
    conf &= 0xffff0000;  // 保留STATUS寄存器，清除COMMAND寄存器
    conf |= 0x7;         // 设置第0~2位（允许PCNET网卡产生中断
    pci_write_command_status(bus, dev, func, conf);
    io_base = pci_get_port_base(bus, dev, func);
    //init_Card_all();
}

void PCNET_IRQ(registers_t *reg) {

    io_out16(io_base + RAP16, CSR0);
    uint16_t temp = io_in16(io_base + RDP16);

   if ((temp & 0x0400) == 0x0400)
        Recv();

    io_out16(io_base + RAP16, CSR0);
    io_out16(io_base + RDP16, temp);  // 通知PCNET网卡 中断处理完毕

    if ((temp & 0x0100) == 0x0100)
        printf("PCNET INIT DONE\n");
    return;
}

void Recv() {
    recv = 1;
    //printk("\nPCNET RECV: ");
    for (; (recvBufferDesc[currentRecvBuffer].flags & 0x80000000) == 0;
           currentRecvBuffer = (currentRecvBuffer + 1) % 8) {
        if (!(recvBufferDesc[currentRecvBuffer].flags & 0x40000000) &&
            (recvBufferDesc[currentRecvBuffer].flags & 0x03000000) == 0x03000000) {
            uint32_t size = recvBufferDesc[currentRecvBuffer].flags & 0xfff;
            if (size > 128)
                size -= 4;

            uint8_t* buffer = (uint8_t*)(recvBufferDesc[currentRecvBuffer].address);
            for (int i = 0; i < (size > 128 ? 128 : size); i++) {
                //printk("%02x ", buffer[i]);
            }
            //printk("\n");
        }
        recv = 0;
        currentRecvBuffer = 0;
        //TODO  Card_Recv_Handler(recvBufferDesc[currentRecvBuffer].address);

        memclean(recvBufferDesc[currentRecvBuffer].address, 2048);
        recvBufferDesc[currentRecvBuffer].flags2 = 0;
        recvBufferDesc[currentRecvBuffer].flags = 0x8000f7ff;
    }
    currentRecvBuffer = 0;

}

