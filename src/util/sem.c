#include "sem.h"
#include "timer.h"

bool sem_wait(sem_t *sem, uint32_t timeout) {
    uint64_t timerStart = nano_time();
    bool ret = false;

    while (true) {
        if (timeout > 0 && nano_time() > (timerStart + timeout))
            goto just_return; // not under any lock atm
        spin_lock(sem->lock);
        if (sem->cnt > 0) {
            sem->cnt--;
            ret = true;
            goto cleanup;
        }
        spin_unlock(sem->lock);
    }

cleanup:
    spin_unlock(sem->lock);
just_return:
    return ret;
}

void sem_post(sem_t *sem) {
    spin_lock(sem->lock);
    sem->cnt++;
    spin_unlock(sem->lock);
}

