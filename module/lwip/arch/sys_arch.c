#include "sys_arch.h"

#include "../../../src/x86_64/include/scheduler.h"

uint64_t next = 1;

#include "lwipopts.h"
#include "sys_arch.h"

#include "lwip/arch.h"
#include "lwip/opt.h"

#include <lwip/arch.h>
#include <lwip/debug.h>
#include <lwip/opt.h>
#include <lwip/stats.h>
#include <lwip/sys.h>

#include "utils.h"

int errno = 0;

// lwip glue code for cavOS
// Copyright (C) 2024 Panagiotis

void sys_init(void) {
    // boo!
}

void sys_mutex_lock(spin_t *spinlock) {
    spin_lock(*spinlock);
}
void sys_mutex_unlock(spin_t *spinlock) {
    spin_unlock(*spinlock);
}

err_t sys_mutex_new(spin_t *spinlock) {
    *spinlock = SPIN_INIT;
    return ERR_OK;
}

err_t sys_sem_new(sys_sem_t *sem, uint8_t cnt) {
    sem->invalid = false;
    sys_mutex_new(&sem->lock);

    return ERR_OK;
}

void sys_sem_signal(sys_sem_t *sem) {
    spin_lock(sem->lock);
    sem->cnt++;
    spin_unlock(sem->lock);
}

uint32_t sys_arch_sem_wait(sys_sem_t *sem, uint32_t timeout) {
    bool ret = false;

    while (true) {
        spin_lock(sem->lock);
        if (sem->cnt > 0) {
            sem->cnt--;
            ret = true;
            goto cleanup;
        }
        spin_unlock(sem->lock);

        scheduler_yield();
    }

cleanup:
    spin_unlock(sem->lock);
just_return:
    return ret ? 0 : SYS_ARCH_TIMEOUT;
}

void sys_sem_free(sys_sem_t *sem) {
    sys_sem_new(sem, 0);
}

void sys_sem_set_invalid(sys_sem_t *sem) {
    sem->invalid = true;
}

int sys_sem_valid(sys_sem_t *sem) {
    return !sem->invalid;
}

uint32_t sys_now(void) {
    tm time;
    time_read(&time);
    return mktime(&time) * 1000;
}

sys_thread_t sys_thread_new(const char *pcName, void (*pxThread)(void *pvParameters), void *pvArg,
                            int iStackSize, int iPriority) {
    task_t *task =
        task_create(pcName, (void (*)(uint64_t))pxThread, (uint64_t)pvArg, KTHREAD_PRIORITY);
    return task->pid;
}

err_t sys_mbox_new(sys_mbox_t *mbox, int size) {
    if (!size) {
        size = TCPIP_MBOX_SIZE;
        // debugf("Tried to create a mailbox with a size of 0!\n");
        // panic();
    }
    memset(mbox, 0, sizeof(sys_mbox_t));
    mbox->invalid = false;
    mbox->size    = size;
    mbox->msges   = malloc(sizeof(mbox->msges[0]) * (size + 1));
    return ERR_OK;
}

void sys_mbox_free(sys_mbox_t *mbox) {
    spin_lock(mbox->lock);
    free(mbox->msges);
    spin_unlock(mbox->lock); // for set_invalid()!
}

void sys_mbox_set_invalid(sys_mbox_t *mbox) {
    spin_lock(mbox->lock);
    mbox->invalid = true;
    spin_unlock(mbox->lock);
}

int sys_mbox_valid(sys_mbox_t *mbox) {
    spin_lock(mbox->lock);
    int ret = !mbox->invalid;
    spin_unlock(mbox->lock);
    return ret;
}

void sys_mbox_post_unsafe(sys_mbox_t *q, void *msg) {
    // spin_lock(&q->lock);
    q->msges[q->ptrWrite] = msg;
    q->ptrWrite           = (q->ptrWrite + 1) % q->size;

    mboxBlock *browse = q->firstBlock;
    while (browse) {
        mboxBlock *next = browse->next;
        if (browse->write == false) { LinkedListRemove((void **)&q->firstBlock, browse); }
        browse = next;
    }

    spin_unlock(q->lock);
}

void sys_mbox_post(sys_mbox_t *q, void *msg) {
    while (true) {
        spin_lock(q->lock);
        if ((q->ptrWrite + 1) % q->size != q->ptrRead) break;
        spin_unlock(q->lock);

        scheduler_yield();
    }

    sys_mbox_post_unsafe(q, msg);
}

err_t sys_mbox_trypost(sys_mbox_t *q, void *msg) {
    spin_lock(q->lock);
    if ((q->ptrWrite + 1) % q->size == q->ptrRead) {
        spin_unlock(q->lock);
        return ERR_MEM;
    }

    sys_mbox_post_unsafe(q, msg);
    return ERR_OK;
}

err_t sys_mbox_trypost_fromisr(sys_mbox_t *q, void *msg) {
    return sys_mbox_trypost(q, msg); // xd
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *q, void **msg, u32_t timeout) {
    while (true) {
        spin_lock(q->lock);
        if (q->ptrRead != q->ptrWrite) break;
        mboxBlock *block = LinkedListAllocate((void **)&q->firstBlock, sizeof(mboxBlock));
        block->task      = current_task;
        block->write     = false;
        spin_unlock(q->lock);

        scheduler_yield();
    }

    // spin_lock(&q->lock);
    *msg       = q->msges[q->ptrRead];
    q->ptrRead = (q->ptrRead + 1) % q->size;
    spin_unlock(q->lock);

    return 0;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *q, void **msg) {
    spin_lock(q->lock);
    if (q->ptrRead == q->ptrWrite) {
        spin_unlock(q->lock);
        return SYS_MBOX_EMPTY;
    }

    // spin_lock(&q->lock);
    *msg       = q->msges[q->ptrRead];
    q->ptrRead = (q->ptrRead + 1) % q->size;
    spin_unlock(q->lock);

    return ERR_OK;
}

// mmm, trash
void *sio_open(u8_t devnum) {
    return 0;
}
u32_t sio_write(void *fd, const u8_t *data, u32_t len) {
    return 0;
}
void sio_send(u8_t c, void *fd) {}
u8_t sio_recv(void *fd) {
    return 0;
}
u32_t sio_read(void *fd, u8_t *data, u32_t len) {
    return 0;
}
u32_t sio_tryread(void *fd, u8_t *data, u32_t len) {
    return 0;
}
