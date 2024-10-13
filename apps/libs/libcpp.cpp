#include "../include/iostream.hpp"

#include <stddef.h>

void *malloc(size_t size);
void free(void *ptr);
int printf(const char* fmt, ...);

auto operator new(size_t size) -> void * {
    return malloc(size);
}

auto operator new[](size_t size) -> void * {
    return malloc(size);
}

auto operator new(size_t size, void *ptr) -> void * {
    return ptr;
}

auto operator new[](size_t size, void *ptr) -> void * {
    return ptr;
}

auto operator delete(void *ptr, size_t size) -> void {
    //
}

auto operator delete[](void *ptr, size_t size) -> void {
    //
}

auto operator delete(void *ptr) -> void {
    free(ptr);
}

auto operator delete[](void *ptr) -> void {
    free(ptr);
}

namespace std{
    const iostream_cout& iostream_cout::operator<<(int value)const{
        printf("%d",value);
        return* this;
    }
    const iostream_cout& iostream_cout::operator << (char* str)const{
        printf("%s",str);
        return* this;
    }

    iostream_cout cout;
}