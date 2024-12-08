#include "multiboot.h"
#include "ctypes.h"
#include "video.h"
#include "klog.h"

extern void kernel_main(multiboot_t *multiboot, uint32_t kernel_stack);
extern void init_mmap(multiboot_t *multiboot);

static int cmd_parse(char *cmd_str, char **argv, char token) {
    int arg_idx = 0;
    while (arg_idx < 50) {
        argv[arg_idx] = NULL;
        arg_idx++;
    }
    char *next = cmd_str;
    int argc = 0;

    while (*next) {
        while (*next == token) *next++;
        if (*next == 0) break;
        argv[argc] = next;
        while (*next && *next != token) *next++;
        if (*next) *next++ = 0;
        if (argc > 50) return -1;
        argc++;
    }

    return argc;
}


/*
 * 该mboot目录下的源文件与 multiboot 引导协议耦合度较高, 其主要功能对协议依赖性较大
 * 故单独拆分出来, 方便移植
 * kernel_head 由 boot.asm/_start 调用
 */
void kernel_head(multiboot_t *multiboot, uint32_t kernel_stack){
    init_mmap(multiboot);

    kernel_main(multiboot,kernel_stack);
}