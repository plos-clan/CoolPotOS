#ifndef CRASHPOWEROS_PROC_H
#define CRASHPOWEROS_PROC_H

#include "common.h"

struct stackframe {
    uint32_t 	gs;
    uint32_t	fs;
    uint32_t	es;
    uint32_t	ds;
    uint32_t	edi;
    uint32_t	esi;
    uint32_t	ebp;
    uint32_t	kernel_esp;
    uint32_t	ebx;
    uint32_t	edx;
    uint32_t	ecx;
    uint32_t	eax;
    uint32_t	retaddr;
    uint32_t	eip;
    uint32_t	cs;
    uint32_t	eflags;
    uint32_t	esp;
    uint32_t	ss;
};

void proc_install();

#endif
