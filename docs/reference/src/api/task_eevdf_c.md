# task/eevdf.c

## `//#pragma GCC push_options //#pragma GCC optimize("O0") #include "task/eevdf.h" #include "krlibc.h" #include "mem/heap.h" #include "task/scheduler.h" #include "task/smp.h" #include "term/klog.h" #include "timer.h" unsigned int sysctl_sched_base_slice = 700000ULL;`


CP_Kernel 精简版 EEVDF 最小虚拟截止时间优先调度

---

