// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <stdint.h>

uint32_t next_power_of_2(uint32_t x);

uint32_t bits_clz(uint32_t x);
uint32_t bits_ctz(uint32_t x);
uint32_t bits_popcount(uint32_t x);

unsigned char *knuth_morris_pratt_matcher(unsigned char *text, int n, unsigned char *pattern, int m);
unsigned char *boyer_moore_matcher(unsigned char *text, int n, unsigned char *pattern, int m);
