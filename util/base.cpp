#include "../include/cpp.h"
#include <stdint.h>
#include <stddef.h>

extern "C" uint32_t kmalloc(size_t);
extern "C" void kfree(void*);

extern "C" void __cxa_pure_virtual()
{
    // Do nothing or print an error message.
}

void *operator
new(size_t
size)
{
return
(void*)  kmalloc(size);
}

void *operator
new[](
size_t size
)
{
return (void*)kmalloc(size);
}

void operator

delete(void *p, unsigned int size) {
    kfree(p);
}

void operator
delete[](
void *p,
unsigned int size
)
{
kfree(p);
}
void operator

delete(void *p) {
    kfree(p);
}

void operator
delete[](
void *p
)
{
kfree(p);
}
