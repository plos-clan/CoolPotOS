/*
 * PlantOS - KernelLogger
 * Copyright by copi143
 */

#include "../include/klog.h"
#include "../include/printf.h"
#include "../include/task.h"

const char *_log_basename_(const char *path) {
    static char name[128];
    int         i = 0;
    while (path[i])
        i++;
    for (i--; i >= 0; i--) {
        if (path[i] == '/' || path[i] == '\\') break;
    }
    return path + i + 1;
}

void klogf(bool isok,char* fmt,...){
    int len;
    va_list ap;
    va_start(ap, fmt);
    char *buf[1024] = {0};
    len = vsprintf(buf, fmt, ap);

    if(get_current() != NULL){
        printf("[%s]: %s",isok ? "  \033[32mOK\033[39m  ":"\033[31mFAILED\033[39m",buf);
    } else{
        if(isok){
            printf("[  \03300ee76;OK\033c6c6c6;  ]: %s",buf);
        } else{
            printf("[\033ee6363;FAILED\033c6c6c6;]: %s",buf);
        }
    }
}
