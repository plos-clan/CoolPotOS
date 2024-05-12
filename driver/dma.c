#include "../include/dma.h"
#include "../include/io.h"

unsigned char MaskReg[8] = {0x0A, 0x0A, 0x0A, 0x0A, 0xD4, 0xD4, 0xD4, 0xD4};
unsigned char ModeReg[8] = {0x0B, 0x0B, 0x0B, 0x0B, 0xD6, 0xD6, 0xD6, 0xD6};
unsigned char ClearReg[8] = {0x0C, 0x0C, 0x0C, 0x0C, 0xD8, 0xD8, 0xD8, 0xD8};

unsigned char PagePort[8] = {0x87, 0x83, 0x81, 0x82, 0x8F, 0x8B, 0x89, 0x8A};
unsigned char AddrPort[8] = {0x00, 0x02, 0x04, 0x06, 0xC0, 0xC4, 0xC8, 0xCC};
unsigned char CountPort[8] = {0x01, 0x03, 0x05, 0x07, 0xC2, 0xC6, 0xCA, 0xCE};

void dma_xfer(unsigned char channel, unsigned long address, unsigned int length,
              unsigned char read) {
    unsigned char page = 0, mode = 0;
    unsigned int offset = 0;

    if (read)
        mode = 0x48 + channel;
    else
        mode = 0x44 + channel;

    page = address >> 16;
    offset = address & 0xFFFF;
    length--;

    _dma_xfer(channel, page, offset, length, mode);
}

void _dma_xfer(unsigned char DMA_channel, unsigned char page,
               unsigned int offset, unsigned int length, unsigned char mode) {
    /* 我们不想别的事情来打扰 */
    asm("cli");

    /* 设置DMA通道，以便我们可以正确传输数据，这很简单，只要我们用I/O操作告诉DMA控制器就行了
     */
    /* 我们将使用这个通道（DMA_channel）*/
    io_out8(MaskReg[DMA_channel], 0x04 | DMA_channel);

    /* 我们先得解除DMA对这个通道的屏蔽，不然用不了 */
    io_out8(ClearReg[DMA_channel], 0x00);

    /* 向DMA发送指定的模式 */
    io_out8(ModeReg[DMA_channel], mode);

    /* 发送偏移量地址，先发送高八位，再发送低八位（因为一次性最多只能发送一个byte）
     */
    io_out8(AddrPort[DMA_channel], LOW_BYTE(offset));
    io_out8(AddrPort[DMA_channel], HI_BYTE(offset));

    /* 发送数据所在的物理页 */
    io_out8(PagePort[DMA_channel], page);

    /* 发送数据的长度 跟之前一样，先发送低八位，再发送高八位*/
    io_out8(CountPort[DMA_channel], LOW_BYTE(length));
    io_out8(CountPort[DMA_channel], HI_BYTE(length));

    /* 现在我们该做的东西已经全部做完了，所以启用DMA_channel */
    io_out8(MaskReg[DMA_channel], DMA_channel);

    /* 重新让CPU能够接收到中断 */
    asm("sti");
}
