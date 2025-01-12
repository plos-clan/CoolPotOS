#include "terminal.h"
#include "gop.h"
#include "klog.h"

void init_terminal() {
    TerminalDisplay display = {
            .width = framebuffer->width,
            .height = framebuffer->height,
            .address = framebuffer->address
    };

    float size = 10.0f * ((float) framebuffer->width / 1024);

    terminal_init(&display, size, malloc, free, NULL);
}
