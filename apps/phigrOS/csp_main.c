#include "../include/stdio.h"
#include "../include/image.h"
#include "../include/cpos.h"
#include "../include/ttfprint.h"

uint32_t *screen;

void draw_recv(int x,int y,int height,int width,uint32_t color,float a){
    struct { uint8_t r,g,b,a; } * const rgb = (void*)color;
    rgb->r = rgb->r * a;
    rgb->g = rgb->g * a;
    rgb->b = rgb->b * a;

    for (int i = x; i < x + width; ++i) {
        for (int j = y; j < y + height; ++j) {
            screen[(y + j) * 1280 + (x + i)] = rgb;
        }
    }
}

int main(int argc,char **argv){
    screen = syscall_framebuffer();
    printf("Loading csp resource...\n");
    ttf_install("phifont.ttf");
    screen_clear();
    draw_image_xys("result.jpg",0,0,1280,768);
   // print_ttf("CoolPotOS Subsystem for PhigrOS.",0xffffff,0x000000,200,600,20.0);

    return 0;
}