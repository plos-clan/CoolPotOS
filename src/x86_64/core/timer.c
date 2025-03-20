#include "timer.h"
#include "alloc.h"

timer_t *alloc_timer() {
    timer_t *timer0  = malloc(sizeof(timer_t));
    timer0->old_time = nanoTime();
    return timer0;
}

uint64_t get_time(timer_t *timer1) {
    uint64_t time0 = nanoTime() - timer1->old_time;
    free(timer1);
    return time0;
}
