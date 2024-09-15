#include "../include/ttfprint.h"
#include "../include/math.h"
#include "../include/syscall.h"
#include "../include/utf.h"
#include <stdio.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../include/stb_truetype.h"

char* ttf_buffer;
stbtt_fontinfo *font;
uint32_t volatile* framebuffer;

static uint32_t LCD_AlphaBlend(uint32_t foreground_color, uint32_t background_color,
                        uint8_t alpha) {
    uint8_t *fg = (uint8_t *)&foreground_color;
    uint8_t *bg = (uint8_t *)&background_color;

    uint32_t rb = (((uint32_t)(*fg & 0xFF) * alpha) +
                   ((uint32_t)(*bg & 0xFF) * (256 - alpha))) >>
                                                             8;
    uint32_t g = (((uint32_t)(*(fg + 1) & 0xFF) * alpha) +
                  ((uint32_t)(*(bg + 1) & 0xFF) * (256 - alpha))) >>
                                                                  8;
    uint32_t a = (((uint32_t)(*(fg + 2) & 0xFF) * alpha) +
                  ((uint32_t)(*(bg + 2) & 0xFF) * (256 - alpha))) >>
                                                                  8;

    return (rb & 0xFF) | ((g & 0xFF) << 8) | ((a & 0xFF) << 16);
}

static void SDraw_Box(uint32_t *vram, int x, int y, int x1, int y1, uint32_t color,
                      int xsize) {
    int i, j;
    for (i = x; i < x1; i++) {
        for (j = y; j < y1; j++) {
            vram[j * xsize + i] = color;
        }
    }
    return;
}

uint8_t *ttf_bitmap(int *buf,uint32_t *width,uint32_t *heigh,float pixels){
    int bitmap_w = 512; /* 位图的宽 */
    int bitmap_h = 128; /* 位图的高 */
    uint8_t *bitmap = calloc(bitmap_w * bitmap_h, sizeof(uint8_t));

    /* "STB"的 unicode 编码 */
    int *word = buf;

    float scale = stbtt_ScaleForPixelHeight(
            font, pixels);

    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(font, &ascent, &descent, &lineGap);

    ascent = roundf(ascent * scale);
    descent = roundf(descent * scale);

    int x = 0; /*位图的x*/
    int max = 0;
    uint32_t h = (int)((float)(ascent - descent + lineGap * scale));
    for (int i = 0; word[i] != 0; ++i) {
        int advanceWidth = 0;
        int leftSideBearing = 0;
        stbtt_GetCodepointHMetrics(font, word[i], &advanceWidth, &leftSideBearing);

        int c_x1, c_y1, c_x2, c_y2;
        stbtt_GetCodepointBitmapBox(font, word[i], scale, scale, &c_x1, &c_y1,
                                    &c_x2, &c_y2);

        int y = ascent + c_y1;
        int byteOffset = x + roundf(leftSideBearing * scale) + (y * bitmap_w);
        stbtt_MakeCodepointBitmap(font, bitmap + byteOffset, c_x2 - c_x1,
                                  c_y2 - c_y1, bitmap_w, scale, scale, word[i]);

        x += roundf(advanceWidth * scale);

        int kern;
        kern = stbtt_GetCodepointKernAdvance(font, word[i], word[i + 1]);
        x += roundf(kern * scale);
    }
    *width = x;
    *heigh = max + h;
    return bitmap;
}

static void put_bitmap(uint8_t *bitmap, uint32_t *vram, uint32_t x, uint32_t y,
                uint32_t width, uint32_t heigh, uint32_t bitmap_xsize,
                uint32_t xsize, uint32_t fc, uint32_t bc) {
    for (int i = 0; i < width; i++) {
        for (int j = 0; j < heigh; j++) {
            uint32_t color = LCD_AlphaBlend(fc, bc, bitmap[j * bitmap_xsize + i]);
            if(color != 0x0) vram[(y + j) * xsize + (x + i)] = color;
        }
    }
}

void print_ttf(char* buf,uint32_t fc,uint32_t bc,uint32_t x,uint32_t y,float pixels){

    Rune *r = (Rune *) malloc((utflen(buf) + 1) * sizeof(Rune));
    r[utflen(buf)] = 0;
    int i = 0;
    while (*buf != '\0') {
        buf += chartorune(&(r[i++]), buf);
    }

    uint32_t width, heigh;
    uint8_t *bitmap = ttf_bitmap(r, &width, &heigh,pixels);

    //SDraw_Box(framebuffer, x, y, x + width, y + heigh, bc, 1280);
    put_bitmap(bitmap,framebuffer,x,y,width,heigh,512, 1280, fc, bc);
    free(bitmap);
    free(r);
}

bool ttf_install(char* filename){
    font = (stbtt_fontinfo*) malloc(sizeof (stbtt_fontinfo));
    ttf_buffer = malloc(syscall_vfs_filesize(filename));
    framebuffer = syscall_framebuffer();
    if(ttf_buffer != NULL){
        syscall_vfs_readfile(filename,ttf_buffer);
        stbtt_InitFont(font,ttf_buffer,stbtt_GetFontOffsetForIndex(ttf_buffer, 0));
        return true;
    } else{
        free(font);
        free(ttf_buffer);
        return false;
    }
}

