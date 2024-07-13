#ifndef CRASHPOWEROS_BMP_H
#define CRASHPOWEROS_BMP_H

#include "common.h"

typedef struct {
    uint16_t magic;
    uint32_t fileSize;
    uint32_t reserved;
    uint32_t bmpDataOffset;
    //  bmp信息头开始
    uint32_t bmpInfoSize;
    uint32_t frameWidth;
    uint32_t frameHeight;
    uint16_t reservedValue; //  必须为0x0001
    uint16_t bitsPerPixel;
    uint32_t compressionMode;
    uint32_t frameSize;
    uint32_t horizontalResolution;
    uint32_t verticalResolution;
    uint32_t usedColorCount;
    uint32_t importantColorCount;
} __attribute__((packed)) Bmp;

void display(Bmp *bmp,uint32_t x, uint32_t y, bool isTransparent);

#endif
