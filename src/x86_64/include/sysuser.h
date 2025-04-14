#pragma once

#include "ctype.h"
#include "tty.h"

typedef struct user_control_block *ucb_t;

typedef enum {
    Ordinary, // 普通
    Super,    // 提升
    Kernel    // 内核态
} UserLevel;

struct user_control_block {
    char      name[50];         // 用户名
    int       uid;              // 用户ID
    UserLevel permission_level; // 权限等级
};

/**
 * 创建用户会话
 * @param name 会话名
 * @param permission_level 权限等级
 * @return == NULL ? 创建失败 : 用户会话
 */
ucb_t create_user(const char *name, UserLevel permission_level);
/**
 * 获取内核态用户会话
 * @return 用户会话
 */
ucb_t get_kernel_user();
void  user_setup();
