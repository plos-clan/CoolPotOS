#include "smp_rv64.h"
#include "task/smp.h"
#include "krlibc.h"

_Noreturn void arch_ap_cpu_entry(){
    while (true) arch_wait_for_interrupt();
}

cpu_local_t *arch_current_cpu() {
    return NULL;
    //TODO
}