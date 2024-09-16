#define MAKE_LUAC
static inline char *getenv(char *s) { return "?.lua"; }
#include "lua/m.c"