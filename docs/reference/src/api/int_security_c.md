# int/security.c

## `#include "security.h" #include "krlibc.h" #include "metadata.h" #include "task/scheduler.h" #include "term/klog.h" #include "timer.h" #if defined(__x86_64__) || defined(__amd64__) # include "fsgsbase.h" #endif static struct pthread pthread_self;`


定义调试模式下的一些保护机制


---

