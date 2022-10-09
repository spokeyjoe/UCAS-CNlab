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

// Get the INDEXth 2-bit of 32-bit n
// which means 0 < INDEX < 16
int get_2bit(uint32_t n, int index) {
    return (n >> (32 - 2*index -2)) & 3U;
}

// Get the INDEXth 4-bit of 32-bit n 
int get_4bit(uint32_t n, int index) {
    return (n >> (32 - 4*index - 4)) & 15U;
}

// Get the first 6 bits of 32-bit n
int get_6bit(uint32_t n) {
    return n >> 26;
}

// Get the first 8 bits of 32-bit n
int get_8bit(uint32_t n) {
    return n >> 24;
}
