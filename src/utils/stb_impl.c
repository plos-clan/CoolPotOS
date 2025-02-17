#define STB_SPRINTF_IMPLEMENTATION
#include "sprintf.h"
#include "lock.h"

ticketlock sprintf_lock;

int sprintf(char *buf, char const *fmt, ...){
    ticket_lock(&sprintf_lock);
    int result;
    va_list va;
    va_start(va, fmt);
    result = STB_SPRINTF_DECORATE(vsprintfcb)(0, 0, buf, fmt, va);
    va_end(va);
    ticket_unlock(&sprintf_lock);
    return result;
}
