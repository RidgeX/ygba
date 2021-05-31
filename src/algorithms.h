#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#include <stdbool.h>
#include <stdint.h>

bool is_power_of_2(uint32_t x);
uint32_t next_power_of_2(uint32_t x);

uint32_t bits_clz(uint32_t x);
uint32_t bits_ctz(uint32_t x);
uint32_t bits_popcount(uint32_t x);

unsigned char *knuth_morris_pratt_matcher(unsigned char *text, int n, unsigned char *pattern, int m);
unsigned char *boyer_moore_matcher(unsigned char *text, int n, unsigned char *pattern, int m);

#endif  // ALGORITHMS_H
