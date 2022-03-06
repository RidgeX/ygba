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

#define INT_VBLANK     (1 << 0)
#define INT_HBLANK     (1 << 1)
#define INT_VCOUNT     (1 << 2)
#define INT_TIMER0     (1 << 3)
#define INT_TIMER1     (1 << 4)
#define INT_TIMER2     (1 << 5)
#define INT_TIMER3     (1 << 6)
#define INT_COM        (1 << 7)
#define INT_DMA0       (1 << 8)
#define INT_DMA1       (1 << 9)
#define INT_DMA2       (1 << 10)
#define INT_DMA3       (1 << 11)
#define INT_BUTTON     (1 << 12)
#define INT_CART       (1 << 13)

#define TM_FREQ_1      0
#define TM_FREQ_64     1
#define TM_FREQ_256    2
#define TM_FREQ_1024   3
#define TM_CASCADE     (1 << 2)
#define TM_IRQ         (1 << 6)
#define TM_ENABLE      (1 << 7)
#define TM_FREQ_MASK   3

extern bool halted;
extern int active_dma;

void gba_check_keypad_interrupt();
void gba_timer_reset(int i);
uint32_t gba_open_bus();
void gba_dma_reset(int ch);
void gba_dma_update(uint32_t current_timing);
