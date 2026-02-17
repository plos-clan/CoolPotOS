#pragma once

#define MAX_PARAMS 32 // 内核最大参数支持

#include "types.h"
#include "boot.h"

typedef struct {
    char key[20];
    char value[50];
} boot_param_t;

int boot_parse_cmdline(const char *cmdline);
const char *boot_get_cmdline_param(const char *key);
