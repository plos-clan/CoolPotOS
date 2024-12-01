#include "../include/tty.h"

void putchar_TextMode(struct tty *res, int c) {
    if (res->x == res->xsize) { res->gotoxy(res, 0, res->y + 1); }
    if (c == '\n') {
        if (res->y == res->ysize - 1) {
            res->screen_ne(res);
            return;
        }
        res->MoveCursor(res, 0, res->y + 1);
        return;
    } else if (c == '\0') {
        return;
    } else if (c == '\b') {
        if (res->x == 0) {
            res->MoveCursor(res, res->xsize - 1, res->y - 1);
            *(uint8_t *)(res->vram + res->y * res->xsize * 2 + res->x * 2)         = ' ';
            *(uint8_t *)(res->vram + res->y * res->xsize * 2 + res->x * 2 - 2 + 1) = res->color;
            return;
        }
        *(uint8_t *)(res->vram + res->y * res->xsize * 2 + res->x * 2 - 2)     = ' ';
        *(uint8_t *)(res->vram + res->y * res->xsize * 2 + res->x * 2 - 2 + 1) = res->color;
        res->MoveCursor(res, res->x - 1, res->y);
        return;
    } else if (c == '\t') {
        // 制表符
        res->print(res, "    ");
        return;
    } else if (c == '\r') {
        return;
    }
    *(uint8_t *)(res->vram + res->y * res->xsize * 2 + res->x * 2)     = c;
    *(uint8_t *)(res->vram + res->y * res->xsize * 2 + res->x * 2 + 1) = res->color;
    res->MoveCursor(res, res->x + 1, res->y);
}

void screen_ne_TextMode(struct tty *res) {
    for (int i = 0; i < res->xsize * 2; i += 2) {
        for (int j = 0; j < res->ysize; j++) {
            *(uint8_t *)(res->vram + j * res->xsize * 2 + i) =
                    *(uint8_t *)(res->vram + (j + 1) * res->xsize * 2 + i);
            *(uint8_t *)(res->vram + j * res->xsize * 2 + i + 1) =
                    *(uint8_t *)(res->vram + (j + 1) * res->xsize * 2 + i + 1);
        }
    }
    for (int i = 0; i < res->xsize * 2; i += 2) {
        *(uint8_t *)(res->vram + (res->ysize - 1) * res->xsize * 2 + i)     = ' ';
        *(uint8_t *)(res->vram + (res->ysize - 1) * res->xsize * 2 + i + 1) = res->color;
    }
    res->gotoxy(res, 0, res->ysize - 1);
    res->Raw_y++;
}

void clear_TextMode(struct tty *res) {
    for (int i = 0; i < res->xsize * 2; i += 2) {
        for (int j = 0; j < res->ysize; j++) {
            *(uint8_t *)(res->vram + j * res->xsize * 2 + i)     = ' ';
            *(uint8_t *)(res->vram + j * res->xsize * 2 + i + 1) = res->color;
        }
    }
    res->gotoxy(res, 0, 0);
    res->Raw_y = 0;
}

void move_cur_TextMode(struct tty *res, int x, int y){
    //res->x = x;
   // res->y = y;
   // int i = y * res->xsize + x;
}