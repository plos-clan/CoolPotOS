#pragma once

#ifdef __cplusplus
static val null = nullptr;
#else
#    define null ((void *)0)
#endif

typedef __INT8_TYPE__  sbyte;
typedef __UINT8_TYPE__ byte;

typedef __INTPTR_TYPE__  isize;
typedef __UINTPTR_TYPE__ usize;

typedef __INT8_TYPE__   i8;
typedef __INT16_TYPE__  i16;
typedef __INT32_TYPE__  i32;
typedef __INT64_TYPE__  i64;
typedef __UINT8_TYPE__  u8;
typedef __UINT16_TYPE__ u16;
typedef __UINT32_TYPE__ u32;
typedef __UINT64_TYPE__ u64;

typedef int pid_t;
