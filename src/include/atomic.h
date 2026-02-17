#pragma once

typedef struct {
    volatile int counter;
} atomic_t;

static inline void atomic_set(atomic_t *v, int i) {
    v->counter = i;
}

static inline int atomic_read(atomic_t *v) {
    return v->counter;
}

static inline void atomic_inc(atomic_t *v) {
    __sync_add_and_fetch(&v->counter, 1);
}

static inline int atomic_dec_and_test(atomic_t *v) {
    return __sync_sub_and_fetch(&v->counter, 1) == 0;
}

static inline void atomic_add(int i, atomic_t *v) {
    __sync_add_and_fetch(&v->counter, i);
}

static inline void atomic_sub(int i, atomic_t *v) {
    __sync_sub_and_fetch(&v->counter, i);
}
