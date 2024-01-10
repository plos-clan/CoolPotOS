#include "../include/task.h"
#include "../include/memory.h"
#include "../include/fatfs.h"
#include "../include/kheap.h"
#include "../include/gdt.h"
#include "../include/console.h"
#include "../include/shell.h"
#include "../include/common.h"
#include "../include/io.h"

void task_setup(void) {

    shell_setup();
}