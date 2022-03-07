// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <stdint.h>

#define DMA_INC        0
#define DMA_DEC        1
#define DMA_FIXED      2
#define DMA_RELOAD     3
#define DMA_REPEAT     (1 << 25)
#define DMA_32         (1 << 26)
#define DMA_DRQ        (1 << 27)
#define DMA_NOW        0
#define DMA_AT_VBLANK  1
#define DMA_AT_HBLANK  2
#define DMA_AT_REFRESH 3
#define DMA_IRQ        (1 << 30)
#define DMA_ENABLE     (1 << 31)

extern int dma_active;
extern uint32_t dma_pc;

void dma_reset(int ch);
void dma_update(uint32_t current_timing);
