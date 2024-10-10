#include "stdio.h"
#include "stdlib.h"
#include "../include/cpos.h"

void print_info(){

    struct sysinfo *info = malloc(sizeof(struct sysinfo));
    get_sysinfo(info);

    printf("               .:=*#&&@@@@@@&&#+=:.               \n");
    printf("            -+&@@@@@@@@@@@@@@@@@@@- \033[36m----------.\033[39m          -----------------\n");
    printf("         -*@@@@@@@@@@@@@@@@@@@@&=\033[36m.-&@@@@@@@@@@+\033[39m          OSName:     %s\n",info->osname);
    printf("       =&@@@@@@@@@@@@@@@@@@@@@=\033[36m.-&@@@@@@@@@@@@+\033[39m          Kernel:     %s\n",info->kenlname);
    printf("     -&@@@@@@@@@@@@@@@@@@@@@=\033[36m.-&@@@@@@@@@@@@@@+\033[39m          CPU Vendor: %s\n",info->cpu_vendor);
    printf("    *@@@@@@@@@@@@@@@@@@@@@=\033[36m.-&@@@@@@@@@@@@@@@@+\033[39m          CPU:        %s\n",info->cpu_name);
    printf("  .&@@@@@@@@@@@@@@@&*=-:: \033[36m:#@@@@@@@@@@@@@@@@@@=\033[39m          Memory:     %dMB\n",info->phy_mem_size);
    printf("  &@@@@@@@@@@@@@&=.       \033[36m#@@@@@@@@@@@@@@@@@#:.\033[39m=         Console:    CPOS_USER_SHELL vt100\n");
    printf(" *@@@@@@@@@@@@@-          \033[36m#@@@@@@@@@@@@@@@#:.\033[39m=@@+        PCI Device: %d\n",info->pci_device);
    printf(":@@@@@@@@@@@@&.           \033[36m#@@@@@@@@@@@@@#:\033[39m =@@@@@.       Resolution: %d x %d\n",info->frame_width,info->frame_height);
    printf("#@@@@@@@@@@@@.            \033[36m#@@@@@@@@@@@#:\033[39m =@@@@@@@+       Time:       %d/%d/%d %d:%d:%d\n",info->year,info->mon,
           info->day,info->hour,info->min,info->sec);
    printf("@@@@@@@@@@@@+             \033[36m*&&&&&&&&&#-\033[39m =@@@@@@@@@&\n");
    printf("@@@@@@@@@@@@-                        :@@@@@@@@@@@@\n");
    printf("@@@@@@@@@@@@+                        #@@@@@@@@@@@&\n");
    printf("*@@@@@@@@@@@@.                      :@@@@@@@@@@@@+\n");
    printf(":@@@@@@@@@@@@&.                    :@@@@@@@@@@@@@.\n");
    printf(" *@@@@@@@@@@@@@=                  =@@@@@@@@@@@@@+ \n");
    printf("  #@@@@@@@@@@@@@&+:            :+@@@@@@@@@@@@@@#  \n");
    printf("   #@@@@@@@@@@@@@@@&*+=----=+#&@@@@@@@@@@@@@@@*   \n");
    printf("    +@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@+    \n");
    printf("     :&@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#:     \n");
    printf("       -#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@#-       \n");
    printf("         :*@@@@@@@@@@@@@@@@@@@@@@@@@@@&+:         \n");
    printf("            :+#@@@@@@@@@@@@@@@@@@@@#+:            \n");
    printf("                :=+*#&@@@@@@&#*+-:                \n");

    free(info);
}