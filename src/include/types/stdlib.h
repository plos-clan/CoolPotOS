#ifndef _STDLIB_H
#define _STDLIB_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void  free(void *ptr);

#define RAND_MAX 32767
int  rand(void);
void srand(unsigned int seed);

int       abs(int j);
long      labs(long j);
long long llabs(long long j);

int       atoi(const char *nptr);
long      atol(const char *nptr);
long long atoll(const char *nptr);
double    atof(const char *nptr);

void  abort(void) __attribute__((__noreturn__));
int   atexit(void (*func)(void));
void  exit(int status) __attribute__((__noreturn__));
char *getenv(const char *name);
int   system(const char *command);

long               strtol(const char *nptr, char **endptr, int base);
unsigned long      strtoul(const char *nptr, char **endptr, int base);
long long          strtoll(const char *nptr, char **endptr, int base);
unsigned long long strtoull(const char *nptr, char **endptr, int base);
double             strtod(const char *nptr, char **endptr);

typedef struct {
    int quot;
    int rem;
} div_t;

typedef struct {
    long quot;
    long rem;
} ldiv_t;

typedef struct {
    long long quot;
    long long rem;
} lldiv_t;

div_t   div(int numer, int denom);
ldiv_t  ldiv(long numer, long denom);
lldiv_t lldiv(long long numer, long long denom);

int    mblen(const char *s, size_t n);
int    mbtowc(wchar_t *pwc, const char *s, size_t n);
int    wctomb(char *s, wchar_t wc);
size_t mbstowcs(wchar_t *pwcs, const char *s, size_t n);
size_t wcstombs(char *s, const wchar_t *pwcs, size_t n);

void  qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));

#ifdef __cplusplus
}
#endif

#endif /* _STDLIB_H */
