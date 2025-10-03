#include "timer.h"
#include "heap.h"
#include "krlibc.h"

timer_t *alloc_timer() {
    timer_t *timer0 = malloc(sizeof(timer_t));
    if (timer0 == NULL) return NULL;
    timer0->old_time = nano_time();
    return timer0;
}

uint64_t get_time(timer_t *timer1) {
    if (timer1 == NULL) return 0;
    uint64_t time0 = nano_time() - timer1->old_time;
    free(timer1);
    return time0;
}
