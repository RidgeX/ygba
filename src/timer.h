// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <stdint.h>

#define TM_FREQ_1    0
#define TM_FREQ_64   1
#define TM_FREQ_256  2
#define TM_FREQ_1024 3
#define TM_CASCADE   (1 << 2)
#define TM_IRQ       (1 << 6)
#define TM_ENABLE    (1 << 7)
#define TM_FREQ_MASK 3

void timer_reset(int i);
void timer_update(uint32_t cycles);
