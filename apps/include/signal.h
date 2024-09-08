#ifndef CRASHPOWEROS_SIGNAL_H
#define CRASHPOWEROS_SIGNAL_H

void (*signal(int sig, void (*func)(int)))(int);

#endif
