#include "stdio.h"
#include "stdlib.h"
#include "../include/cpos.h"

void print_info(){

    struct sysinfo *info = get_sysinfo();

    printf("               .:=*#&&@@@@@@&&#+=:.               \n");
    printf("            -+&@@@@@@@@@@@@@@@@@@@- \03300C5CD;----------.\033C6C6C6;          -----------------\n");
    printf("         -*@@@@@@@@@@@@@@@@@@@@&=\03300C5CD;.-&@@@@@@@@@@+\033C6C6C6;          OSName:     %s\n",info->osname);
    printf("       =&@@@@@@@@@@@@@@@@@@@@@=\03300C5CD;.-&@@@@@@@@@@@@+\033C6C6C6;          Kernel:     %s\n",info->kenlname);
    printf("     -&@@@@@@@@@@@@@@@@@@@@@=\03300C5CD;.-&@@@@@@@@@@@@@@+\033C6C6C6;          CPU Vendor: %s\n",info->cpu_vendor);
    printf("    *@@@@@@@@@@@@@@@@@@@@@=\03300C5CD;.-&@@@@@@@@@@@@@@@@+\033C6C6C6;          CPU:        %s\n",info->cpu_name);
    printf("  .&@@@@@@@@@@@@@@@&*=-:: \03300C5CD;:#@@@@@@@@@@@@@@@@@@=\033C6C6C6;          Memory:     %dMB\n",info->phy_mem_size);
    printf("  &@@@@@@@@@@@@@&=.       \03300C5CD;#@@@@@@@@@@@@@@@@@#:.\033C6C6C6;=         Console:    CPOS_USER_SHELL\n");
    printf(" *@@@@@@@@@@@@@-          \03300C5CD;#@@@@@@@@@@@@@@@#:.\033C6C6C6;=@@+        PCI Device: %d\n",info->pci_device);
    printf(":@@@@@@@@@@@@&.           \03300C5CD;#@@@@@@@@@@@@@#:\033C6C6C6; =@@@@@.       Resolution: %d x %d\n",info->frame_width,info->frame_height);
    printf("#@@@@@@@@@@@@.            \03300C5CD;#@@@@@@@@@@@#:\033C6C6C6; =@@@@@@@+       Time:       %d/%d/%d %d:%d:%d\n",info->year,info->mon,
           info->day,info->hour,info->min,info->sec);
    printf("@@@@@@@@@@@@+             \03300C5CD;*&&&&&&&&&#-\033C6C6C6; =@@@@@@@@@&\n");
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

    free_info(info);
}