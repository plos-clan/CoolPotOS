#include "bootarg.h"
#include "krlibc.h"
#include "limine.h"

static boot_param_t params[MAX_PARAMS];
static int          param_count = 0;

LIMINE_REQUEST struct limine_executable_cmdline_request cmdline_request = {
    .id = LIMINE_EXECUTABLE_CMDLINE_REQUEST,
};

char *get_kernel_cmdline() {
    return cmdline_request.response->cmdline;
}

int boot_parse_cmdline(const char *cmdline) {
    param_count = 0;

    const char *p = cmdline;
    while (*p && param_count < MAX_PARAMS) {
        while (*p == ' ')
            p++;
        if (*p == '\0') break;
        const char *key_start = p;
        while (*p && *p != '=' && *p != ' ')
            p++;
        size_t key_len = p - key_start;
        if (key_len >= sizeof(params[param_count].key))
            key_len = sizeof(params[param_count].key) - 1;
        memcpy(params[param_count].key, key_start, key_len);
        params[param_count].key[key_len] = '\0';
        params[param_count].value[0]     = '\0';
        if (*p == '=') {
            p++;
            const char *val_start = p;
            while (*p && *p != ' ')
                p++;
            size_t val_len = p - val_start;
            if (val_len >= sizeof(params[param_count].value))
                val_len = sizeof(params[param_count].value) - 1;
            memcpy(params[param_count].value, val_start, val_len);
            params[param_count].value[val_len] = '\0';
        }
        param_count++;
    }

    return param_count;
}

const char *boot_get_cmdline_param(const char *key) {
    for (int i = 0; i < param_count; i++) {
        if (strcmp(params[i].key, key) == 0) { return params[i].value; }
    }
    return NULL;
}
