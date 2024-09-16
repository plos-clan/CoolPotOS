#ifndef CRASHPOWEROS_CPOS_H
#define CRASHPOWEROS_CPOS_H

#include "ctype.h"

struct sysinfo{
    char* osname;
    char* kenlname;
    char* cpu_vendor;
    char* cpu_name;
    unsigned int phy_mem_size;
    unsigned int pci_device;
    unsigned int frame_width;
    unsigned int frame_height;
    uint32_t year;
    uint32_t mon;
    uint32_t day;
    uint32_t hour;
    uint32_t min;
    uint32_t sec;
};

#include "syscall.h"
#include "stdlib.h"

static inline struct sysinfo* get_sysinfo(){
    return syscall_sysinfo();
}

static inline void free_info(struct sysinfo *info){
    free(info->kenlname);
    free(info->osname);
    free(info->cpu_name);
    free(info->cpu_vendor);
    free(info);
}

static inline int exec_elf(const char* filename,const char* args,int is_async){
    return syscall_exec(filename,args,is_async);
}

static inline void draw_bitmap(int x,int y,int width,int height,char* bitmap){
    syscall_draw_bitmap(x,y,width,height,bitmap);
}

static inline void screen_clear(){
    syscall_g_clean();
}

#endif
