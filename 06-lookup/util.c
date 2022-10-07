#include "util.h"
#include "tree.h"

// return the interval in us
long get_interval(struct timeval tv_start,struct timeval tv_end){
    long start_us = tv_start.tv_sec * 1000000 + tv_start.tv_usec;
    long end_us   = tv_end.tv_sec   * 1000000 + tv_end.tv_usec;
    return end_us - start_us;
}

// Get the INDEXth bit of 32-bit n
int get_bit(uint32_t n, int index) {
    return (n >> (32 - index - 1)) & 1U;
}