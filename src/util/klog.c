#include <stdarg.h>
#include "klog.h"
#include "krlibc.h"
#include "tty.h"
#include "serial.h"
#include "video.h"

extern tty_t default_tty;

void logkf(char *formet,...){
    va_list ap;
    va_start(ap, formet);
    char *buf[1024] = {0};
    vsprintf(buf, formet, ap);
    logk(buf);
    va_end(ap);
}

void logk(const char *message){
    for (size_t i = 0; i < strlen(message); i++){
#ifndef OS_HARDWARE_MODE
        write_serial(message[i]);
#endif
    }
}

void printk(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    char *buf[1024] = {0};
    vsprintf(buf, format, ap);
    k_print(buf);
    va_end(ap);
}

void k_print(const char* message){
    default_tty.print(&default_tty,message);
}

void klogf(bool isok, char* fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    char *buf[1024] = {0};
    vsprintf(buf, fmt, ap);

    printk("[%s]: %s",isok ? "  \033[32mOK\033[39m  ":"\033[31mFAILED\033[39m",buf);
}