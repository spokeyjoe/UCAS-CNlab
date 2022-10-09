#ifndef __UTIL_H__
#define __UTIL_H__

#include <sys/time.h>
#include <stdbool.h>
#include <stdint.h>

long get_interval(struct timeval tv_start,struct timeval tv_end);
int get_bit(uint32_t n, int index);
int get_2bit(uint32_t n, int index);
int get_4bit(uint32_t n, int index);
int get_6bit(uint32_t n);
int get_8bit(uint32_t n);


#endif
