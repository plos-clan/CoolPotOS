#ifndef CRASHPOWEROS_DMA_H
#define CRASHPOWEROS_DMA_H

#define LOW_BYTE(x) (x & 0x00FF)
#define HI_BYTE(x) ((x & 0xFF00) >> 8)

void _dma_xfer(unsigned char DMA_channel, unsigned char page,
               unsigned int offset, unsigned int length, unsigned char mode);
void dma_xfer(unsigned char channel, unsigned long address, unsigned int length,
              unsigned char read);

#endif
