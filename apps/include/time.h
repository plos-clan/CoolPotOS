#ifndef __TIME__H__
#define __TIME__H__
#include <ctype.h>
typedef long clock_t;
struct tm {
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year;
  int tm_wday;
  int tm_yday;
  int tm_isdst;
};
typedef unsigned int time_t;
time_t time(time_t timer);
clock_t clock();
void clock_gettime(int* sec1, int* usec1);
time_t mktime(struct tm* tm);
size_t strftime(char * s, size_t max, const char * fmt, const struct tm * t);
struct tm* localtime(time_t *t1);
#define CLOCKS_PER_SEC 1000
#endif