#pragma once

#define cmpxchg(P, O, N) __sync_val_compare_and_swap((P), (O), (N))
#define atomic_xadd(P, V) __sync_fetch_and_add((P), (V))

#define barrier() __asm__ volatile("": : :"memory")

#define spin_lock ticket_lock
#define spin_unlock ticket_unlock
#define spinlock ticketlock

#define SPINLOCK_INITIALIZER { 0, 0 };

#include "ctype.h"
#include "klog.h"

#define cpu_relax() do{ __asm__ volatile("pause\n": : :"memory"); }while(false);

typedef union ticketlock ticketlock;

union ticketlock {
    unsigned u;
    struct {
        uint16_t ticket;
        uint16_t users;
    } s;
};

static inline void ticket_lock(ticketlock *t) {
    uint16_t me = atomic_xadd(&t->s.users, 1);
    while (t->s.ticket != me) cpu_relax();
}

static inline void ticket_unlock(ticketlock *t) {
    barrier();
    t->s.ticket++;
}

static inline void ticket_init(ticketlock *t) {
    t->u = 0;
    t->s.ticket = 0;
}

static inline int ticket_trylock(ticketlock *t) {
    uint16_t me = t->s.users;
    uint16_t menew = me + 1;
    unsigned cmp = ((unsigned) me << 16) + me;
    unsigned cmpnew = ((unsigned) menew << 16) + me;

    if (cmpxchg(&t->u, cmp, cmpnew) == cmp) return 0;

    return 1;
}

static inline int ticket_lockable(ticketlock *t) {
    ticketlock u = *t;
    barrier();
    return (u.s.ticket == u.s.users);
}
