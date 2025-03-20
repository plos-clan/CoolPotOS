#pragma once

extern struct limine_framebuffer *framebuffer;

void init_gop();

void gop_clear();

uint64_t get_screen_width();
uint64_t get_screen_height();
