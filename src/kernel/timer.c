#include "../include/timer.h"
#include "../include/io.h"
#include "../include/graphics.h"
#include "../include/cmos.h"
#include "../include/acpi.h"

uint32_t tick = 0;
extern struct task_struct *current;
struct TIMERCTL timerctl;

unsigned int time(void) {
    unsigned int t;
    t = get_year() * get_mon() * get_day_of_month() * get_hour() * get_min() * get_sec() + timerctl.count + timerctl.next;
    t |= (int)timerctl.t0;
    t |= get_day_of_week();
    return t;
}

static void timer_handle(registers_t *regs) {
    io_cli();
    tick++;
    schedule();
    io_sti();
}

void sleep(uint32_t timer){
    clock_sleep(timer);
    //usleep(timer*1000000);
}

void clock_sleep(uint32_t timer){
    uint32_t sleep = tick + timer;
    while(1){
        printf("");
        if(tick >= sleep) break;
    }
}

void init_pit(void) {
    io_out8(0x43, 0x34);
    io_out8(0x40, 0x9c);
    io_out8(0x40, 0x2e);

    int i;
    struct TIMER *t;
    timerctl.count = 0;
    for (i = 0; i < MAX_TIMER; i++) {
        timerctl.timers0[i].flags = 0; /* 没有使用 */
    }
    t = timer_alloc(); /* 取得一个 */
    t->timeout = 0xffffffff;
    t->flags = TIMER_FLAGS_USING;
    t->next = 0;     /* 末尾 */
    timerctl.t0 = t; /* 因为现在只有哨兵，所以他就在最前面*/
    timerctl.next =
            0xffffffff; /* 因为只有哨兵，所以下一个超时时刻就是哨兵的时刻 */
    return;
}

void init_timer(uint32_t timer) {
    register_interrupt_handler(IRQ0, &timer_handle);
    uint32_t divisor = 1193180 / timer;

    outb(0x43, 0x36); // 频率

    uint8_t l = (uint8_t) (divisor & 0xFF);
    uint8_t h = (uint8_t) ((divisor >> 8) & 0xFF);

    outb(0x40, l);
    outb(0x40, h);
}

struct TIMER *timer_alloc(void) {
    int i;
    for (i = 0; i < MAX_TIMER; i++) {
        if (timerctl.timers0[i].flags == 0) {
            timerctl.timers0[i].flags = TIMER_FLAGS_ALLOC;
            timerctl.timers0[i].waiter = NULL;
            return &timerctl.timers0[i];
        }
    }
    return 0; /* 没找到 */
}

void timer_free(struct TIMER *timer) {
    timer->flags = 0; /* 未使用 */
    timer->waiter = NULL;
    return;
}

void timer_init(struct TIMER *timer, struct FIFO8 *fifo, unsigned char data) {
    timer->fifo = fifo;
    timer->data = data;
    return;
}

void timer_settime(struct TIMER *timer, unsigned int timeout) {
    int e;
    struct TIMER *t, *s;
    timer->timeout = timeout + timerctl.count;
    timer->flags = TIMER_FLAGS_USING;
    e = io_load_eflags();
    t = timerctl.t0;
    if (timer->timeout <= t->timeout) {
        /* 插入最前面的情况 */
        timerctl.t0 = timer;
        timer->next = t; /* 下面是设定t */
        timerctl.next = timer->timeout;
        io_store_eflags(e);
        return;
    }
    for (;;) {
        s = t;
        t = t->next;
        if (timer->timeout <= t->timeout) {
            /* 插入s和t之间的情况 */
            s->next = timer; /* s下一个是timer */
            timer->next = t; /* timer的下一个是t */
            io_store_eflags(e);
            return;
        }
    }
}
