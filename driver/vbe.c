#include "../include/graphics.h"
#include "../include/io.h"

int isVBEDisplayMode(uint16_t vbe_mode_info) {
    if (vbe_mode_info & (1 << 12)) {
        return 1;
    } else {
        return 0;
    }
}

void initVBE(multiboot_t *mboot) {

    if(isVBEDisplayMode(mboot->vbe_mode_info)){
        printf("[\035kernel\036]: Graphics mode: \037VBE\036\n");
    } else printf("[\035kernel\036]: Graphics mode: \037VGA\036\n");
}
