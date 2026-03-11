/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * w32threadwrap.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef W32THREADWRAP_H
#define W32THREADWRAP_H

#include "sqfs/predef.h"

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>

typedef unsigned int sigset_t;
typedef HANDLE pthread_t;
typedef CRITICAL_SECTION pthread_mutex_t;
typedef CONDITION_VARIABLE pthread_cond_t;

static inline int pthread_create(pthread_t *thread, const void *attr,
				 LPTHREAD_START_ROUTINE proc, LPVOID arg)
{
	HANDLE hnd = CreateThread(NULL, 0, proc, arg, 0, 0);
	(void)attr;

	if (hnd == NULL)
		return -1;

	*thread = hnd;
	return 0;
}

static int pthread_join(pthread_t thread, void **retval)
{
	WaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);
	if (retval != NULL)
		*retval = NULL;
	return 0;
}

static inline int pthread_mutex_init(pthread_mutex_t *mutex, const void *attr)
{
	(void)attr;
	InitializeCriticalSection(mutex);
	return 0;
}

static inline int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	EnterCriticalSection(mutex);
	return 0;
}

static inline int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	LeaveCriticalSection(mutex);
	return 0;
}

static inline void pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	DeleteCriticalSection(mutex);
}

static inline int pthread_cond_init(pthread_cond_t *cond, const void *attr)
{
	(void)attr;
	InitializeConditionVariable(cond);
	return 0;
}

static inline int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mtx)
{
	return SleepConditionVariableCS(cond, mtx, INFINITE) != 0;
}

static inline int pthread_cond_broadcast(pthread_cond_t *cond)
{
	WakeAllConditionVariable(cond);
	return 0;
}

static inline void pthread_cond_destroy(pthread_cond_t *cond)
{
	(void)cond;
}

#define SIG_SETMASK (0)

static inline int sigfillset(sigset_t *set)
{
	*set = 0xFFFFFFFF;
	return 0;
}

static inline int pthread_sigmask(int how, const sigset_t *set,
				  const sigset_t *old)
{
	(void)how; (void)set; (void)old;
	return 0;
}

#endif /* W32THREADWRAP_H */
