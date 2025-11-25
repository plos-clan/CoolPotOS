#include "rv64_irq.h"
#include "ptrace.h"
#include "term/klog.h"
#include "task/scheduler.h"
#include "krlibc.h"
#include "io.h"

bool is_debug = false;

void page_fault_(struct pt_regs *regs,enum page_fault_type type){
    char *type_msg;
    uint64_t faulting_address = csr_read(stval);
    switch (type) { case LOAD_PAGE: type_msg = "load page error"; break;
    case STORE_AMO_PAGE: type_msg = "store/amo error"; break;
    case INS_PAGE:type_msg = "instruction error"; break;
    }

    kerror("page fault %s %p at %p",type_msg,faulting_address,regs->epc);
    if (is_debug) return;
    arch_close_interrupt();
    while (true) arch_wait_for_interrupt();
}