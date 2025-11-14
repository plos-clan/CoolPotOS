# lib/zstd/common/threading.c

## `#include "threading.h" /* create fake symbol to avoid empty translation unit warning */ int g_ZSTD_threading_useless_symbol;`


This file will hold wrapper for systems, which do not support pthreads


---

## `/* === Dependencies === */ # include "errno.h" # include <process.h> /* === Implementation === */ static unsigned __stdcall worker(void *arg) {`


Windows minimalist Pthread Wrapper, based on :
http://www.cse.wustl.edu/~schmidt/win32-cv-1.html


---

