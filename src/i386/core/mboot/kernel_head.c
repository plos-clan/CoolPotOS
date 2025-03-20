#include "ctypes.h"
#include "klog.h"
#include "multiboot.h"
#include "video.h"

extern void kernel_main(multiboot_t *multiboot, uint32_t kernel_stack);
extern void init_mmap(multiboot_t *multiboot);

/*static int cmd_parse(char *cmd_str, char **argv, char token) {
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
*/
#define MAX_ARGS 50
static int cmd_parse(char *cmd_str, char **argv, char token) {
    // 初始化 argv 数组
    for (int i = 0; i < MAX_ARGS; i++) {
        argv[i] = NULL;
    }

    char *next     = cmd_str;
    int   argc     = 0;
    int   in_quote = 0; // 用于跟踪是否在引号内
    char  quote_char;
    while (*next) {
        // 跳过分隔符
        while (*next == token && !in_quote) {
            next++;
        }

        // 检查是否到达字符串末尾
        if (*next == 0) { break; }

        // 检查是否进入引号
        if (*next == '"' || *next == '\'') {
            in_quote   = 1;
            quote_char = *next;
            next++;
            continue;
        }

        // 保存参数起始位置
        argv[argc] = next;

        // 移动到参数末尾
        while (*next && (*next != token || in_quote)) {
            if (*next == '"' || *next == '\'') {
                if (*next == quote_char) {
                    in_quote = 0; // 退出引号
                }
            }
            next++;
        }

        // 终止参数
        if (*next) { *next++ = 0; }

        // 增加参数计数
        argc++;

        // 检查参数数量是否超过限制
        if (argc >= MAX_ARGS) { return -1; }
    }

    return argc; // 返回参数数量
}
/*
 * 该mboot目录下的源文件与 multiboot 引导协议耦合度较高, 其主要功能对协议依赖性较大
 * 故单独拆分出来, 方便移植
 * kernel_head 由 boot.asm/_start 调用
 */
void kernel_head(multiboot_t *multiboot, uint32_t kernel_stack) {
    init_mmap(multiboot);

    kernel_main(multiboot, kernel_stack);
}