#ifndef CRASHPOWEROS_TIMER_H
#define CRASHPOWEROS_TIMER_H

#include <stdint.h>

void init_timer(uint32_t timer);

void clock_sleep(uint32_t timer);

#endif //CRASHPOWEROS_TIMER_H
