#pragma once

#include "isr.h"
#include "pcb.h"

pcb_t *get_current_proc(); //获取当前调度的进程
int get_all_task(); //获取所有运行中的进程

// 配合 io_sti() io_cli() 使用更佳
void enable_scheduler(); //启用进程调度器
void disable_scheduler(); //禁用进程调度器

//CP_Kernel默认调度措施
void default_scheduler(registers_t *reg,pcb_t* next);
void scheduler_process(registers_t *reg);