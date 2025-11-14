# lib/zstd/common/threading.h

## `#ifndef THREADING_H_938743 #define THREADING_H_938743 #include "debug.h" #if defined (__cplusplus) extern "C" {`


Copyright (c) 2016 Tino Reichardt
All rights reserved.

You can contact the author at:
- zstdmt source repository: https://github.com/mcmilk/zstdmt

This source code is licensed under both the BSD-style license (found in the
LICENSE file in the root directory of this source tree) and the GPLv2 (found
in the COPYING file in the root directory of this source tree).
You may select, at your option, one of the above-listed licenses.


---

## `#ifdef WINVER # undef WINVER #endif #define WINVER 0x0600 #ifdef _WIN32_WINNT # undef _WIN32_WINNT #endif #define _WIN32_WINNT 0x0600 #ifndef WIN32_LEAN_AND_MEAN # define WIN32_LEAN_AND_MEAN #endif #undef ERROR /* reported already defined on VS 2015 (Rich Geldreich) */ #include <windows.h> #undef ERROR #define ERROR(name) ZSTD_ERROR(name) /* mutex */ #define ZSTD_pthread_mutex_t CRITICAL_SECTION #define ZSTD_pthread_mutex_init(a, b) ((void)(b), InitializeCriticalSection((a)), 0) #define ZSTD_pthread_mutex_destroy(a) DeleteCriticalSection((a)) #define ZSTD_pthread_mutex_lock(a) EnterCriticalSection((a)) #define ZSTD_pthread_mutex_unlock(a) LeaveCriticalSection((a)) /* condition variable */ #define ZSTD_pthread_cond_t CONDITION_VARIABLE #define ZSTD_pthread_cond_init(a, b) ((void)(b), InitializeConditionVariable((a)), 0) #define ZSTD_pthread_cond_destroy(a) ((void)(a)) #define ZSTD_pthread_cond_wait(a, b) SleepConditionVariableCS((a), (b), INFINITE) #define ZSTD_pthread_cond_signal(a) WakeConditionVariable((a)) #define ZSTD_pthread_cond_broadcast(a) WakeAllConditionVariable((a)) /* ZSTD_pthread_create() and ZSTD_pthread_join() */ typedef struct {`


Windows minimalist Pthread Wrapper, based on :
http://www.cse.wustl.edu/~schmidt/win32-cv-1.html


---

## `#elif defined(ZSTD_MULTITHREAD) /* posix assumed ;`


add here more wrappers as required


---

