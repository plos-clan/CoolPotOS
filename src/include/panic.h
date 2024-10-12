#ifndef CRASHPOWEROS_PANIC_H
#define CRASHPOWEROS_PANIC_H

enum PANIC_TYPE{
    ILLEGAL_KERNEL_STATUS, //内核状态异常
    OUT_OF_MEMORY, //内存溢出
    KERNEL_PAGE_FAULT, //内核页错误
};

void panic_pane(char* msg,enum PANIC_TYPE type);// 发生蓝屏
void init_eh();

#endif
