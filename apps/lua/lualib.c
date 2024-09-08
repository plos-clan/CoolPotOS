#define MAKE_LIB
static inline char *getenv(char *s) { return "?.lua"; }
#include "lua/m.c"