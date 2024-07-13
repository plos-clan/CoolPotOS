#include "../include/bmp.h"
#include "../include/graphics.h"

void display(Bmp *bmp,uint32_t x, uint32_t y, bool isTransparent) {
    if (bmp->magic != 0x4d42 || bmp->bitsPerPixel != 24)
        return;
    uint8_t *data = (uint8_t *)bmp + bmp->bmpDataOffset;
    for (uint32_t yOffset = 0; yOffset < bmp->frameHeight; ++yOffset) {
        for (uint32_t xOffset = 0; xOffset < bmp->frameWidth; ++xOffset) {
            uint32_t offset = (yOffset * bmp->frameWidth + xOffset) * 3;
            uint8_t b = data[offset + 0];
            uint8_t g = data[offset + 1];
            uint8_t r = data[offset + 2];
            uint32_t color = (r << 16) | (g << 8) | b;
            if (isTransparent && color == 0)
                continue;
            drawPixel(x + xOffset, y + bmp->frameHeight - 1 - yOffset, color);
        }
    }
    return;
}


