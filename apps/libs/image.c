#include "../include/image.h"
#include "../include/cpos.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_THREAD_LOCALS
#include "../include/stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../include/stb_image_resize.h"

void convert_ABGR_to_ARGB(uint32_t* bitmap, size_t num_pixels) {
    for (size_t i = 0; i < num_pixels; ++i) {
        uint32_t pixel = bitmap[i];
        uint8_t alpha = (pixel >> 24) & 0xFF;
        uint8_t red = (pixel >> 16) & 0xFF;
        uint8_t green = (pixel >> 8) & 0xFF;
        uint8_t blue = pixel & 0xFF;

        bitmap[i] = (alpha << 24) | (blue << 16) | (green << 8) | red;
    }
}

void draw_image_xys(char* filename,uint32_t x,uint32_t y,uint32_t width,uint32_t height){
    int w, h, bpp;
    stbi_uc* b = stbi_load(filename, &w, &h, &bpp, 4);
    if(!b) {
        printf("[ERROR]: ");
        printf("Fatal Error:%s\n",stbi_failure_reason());
        return 0;
    }
    uint8_t* b1 = malloc(width * height * 4);
    stbir_resize_uint8(b, w, h, 0, b1, width, height, 0, 4);
    convert_ABGR_to_ARGB(b1, width * height);
    draw_bitmap(x, y, 1280, 768, b1);
}

void draw_image_xy(char* filename,uint32_t x,uint32_t y){
    int w, h, bpp;
    stbi_uc* b = stbi_load(filename, &w, &h, &bpp, 4);
    if(!b) {
        printf("[ERROR]: ");
        printf("Fatal Error:%s\n",stbi_failure_reason());
        return 0;
    }
    uint8_t* b1;
    if (w > 1280 && h > 768) {
        b1 = malloc(1280 * 768 * 4);
        stbir_resize_uint8(b, w, h, 0, b1, 1280, 768, 0, 4);
        convert_ABGR_to_ARGB(b1, 1280 * 768);
        draw_bitmap(x, y, 1280, 768, b1);
    } else if (w > 1280) {
        b1 = malloc(1280 * h * 4);

        stbir_resize_uint8(b, w, h, 0, b1, 1280, h, 0, 4);
        convert_ABGR_to_ARGB(b1, 1280 * h);
        draw_bitmap(x, y, 1280, h, b1);
    } else if (h > 768) {
        b1 = malloc(w * 768 * 4);
        stbir_resize_uint8(b, w, h, 0, b1, w, 768, 0, 4);
        convert_ABGR_to_ARGB(b1, w * 768);
        draw_bitmap(x, y, w, 768, b1);
    } else {
        convert_ABGR_to_ARGB(b, w * h);
        draw_bitmap(x, y, w, h, b);
    }
}

void draw_image(char* filename){
    int w, h, bpp;
    stbi_uc* b = stbi_load(filename, &w, &h, &bpp, 4);
    if(!b) {
        printf("Fatal Error:%s\n",stbi_failure_reason());
        return 0;
    }
    uint8_t* b1;
    if (w > 1280 && h > 768) {
        b1 = malloc(1280 * 768 * 4);
        stbir_resize_uint8(b, w, h, 0, b1, 1280, 768, 0, 4);
        convert_ABGR_to_ARGB(b1, 1280 * 768);
        draw_bitmap(0, 0, 1280, 768, b1);
    } else if (w > 1280) {
        b1 = malloc(1280 * h * 4);
        stbir_resize_uint8(b, w, h, 0, b1, 1280, h, 0, 4);
        convert_ABGR_to_ARGB(b1, 1280 * h);
        draw_bitmap(0, 0, 1280, h, b1);
    } else if (h > 768) {
        b1 = malloc(w * 768 * 4);
        stbir_resize_uint8(b, w, h, 0, b1, w, 768, 0, 4);
        convert_ABGR_to_ARGB(b1, w * 768);
        draw_bitmap(0, 0, w, 768, b1);
    } else {
        convert_ABGR_to_ARGB(b, w * h);
        draw_bitmap(0, 0, w, h, b);
    }
}

