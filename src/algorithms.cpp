// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#include "algorithms.h"

#include <stdint.h>

uint32_t next_power_of_2(uint32_t x) {
    return (x == 1 ? 1 : 1 << (32 - bits_clz(x - 1)));
}

uint32_t bits_clz(uint32_t x) {
#ifdef __GNUC__
    return __builtin_clz(x);
#else
    uint32_t t = ((x & 0xffff0000) == 0) << 4;
    x >>= 16 - t;
    uint32_t r = t;
    t = ((x & 0xff00) == 0) << 3;
    x >>= 8 - t;
    r += t;
    t = ((x & 0xf0) == 0) << 2;
    x >>= 4 - t;
    r += t;
    t = ((x & 0xc) == 0) << 1;
    x >>= 2 - t;
    r += t;
    return r + ((2 - x) & -((x & 2) == 0));
#endif
}
