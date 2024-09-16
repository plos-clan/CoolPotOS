#ifndef _SET_JMP_H
#define _SET_JMP_H
#define _NSETJMP    10

typedef long jmp_buf[_NSETJMP];

int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);
#endif