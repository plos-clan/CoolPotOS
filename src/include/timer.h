#ifndef CRASHPOWEROS_TIMER_H
#define CRASHPOWEROS_TIMER_H

#define MAX_TIMER 500
#define TIMER_FLAGS_ALLOC 1 /* 已配置状态 */
#define TIMER_FLAGS_USING 2 /* 定时器运行中 */

#include <stdint.h>
#include "task.h"

struct TIMER {
    struct TIMER *next;
    unsigned int timeout, flags;
    struct FIFO8 *fifo;
    unsigned char data;
    struct task_struct *waiter;
};

struct TIMERCTL {
    unsigned int count, next;
    struct TIMER *t0;
    struct TIMER timers0[MAX_TIMER];
};

void init_timer(uint32_t timer);
void timer_free(struct TIMER *timer);
void timer_init(struct TIMER *timer, struct FIFO8 *fifo, unsigned char data);
void timer_settime(struct TIMER *timer, unsigned int timeout);
void clock_sleep(uint32_t timer);
void sleep(uint32_t timer);
struct TIMER *timer_alloc(void);
unsigned int time(void);
void init_pit(void);

#endif //CRASHPOWEROS_TIMER_H
