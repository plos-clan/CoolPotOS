# mem/buddy.c

## `#include "mem/buddy.h" #include "boot.h" #include "krlibc.h" #include "mem/bitmap.h" #include "mem/frame.h" #include "mem/page.h" #include "task/smp.h" #include "term/klog.h" const char *zone_names[__MAX_NR_ZONES] = {`


NeoAetherOS per-cpu cache buddy alloc.
Copyright @2025-2026 by lihanrui2913.


---

