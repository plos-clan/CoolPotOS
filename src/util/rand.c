#include "../include/common.h"

static unsigned long next1 = 1;

unsigned int rand(void) {
    next1 = next1 * 1103515245 + 12345;
    return ((unsigned int)(next1));
}

void srand(unsigned long seed) {
    next1 = seed;
}