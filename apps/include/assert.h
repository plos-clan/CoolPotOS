#ifndef _ASSERT__
#define _ASSERT__
#define assert(n) do { if(!n) { printf("Error: app assert.\n"); exit(-1); } }while(0);
#endif