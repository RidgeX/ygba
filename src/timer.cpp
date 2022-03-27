// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#include "timer.h"

#include <stdint.h>
#include <cstdlib>

#include "cpu.h"
#include "dma.h"
#include "io.h"

void timer_reset(int i) {
    ioreg.timer[i].counter.w = ioreg.timer[i].reload.w;
    ioreg.timer[i].elapsed = 0;
}

void timer_update(uint32_t cycles) {
    bool overflow = false;

    for (int i = 0; i < 4; i++) {
        uint16_t &counter = ioreg.timer[i].counter.w;
        uint16_t reload = ioreg.timer[i].reload.w;
        uint16_t control = ioreg.timer[i].control.w;
        uint32_t &elapsed = ioreg.timer[i].elapsed;

        if (!(control & TM_ENABLE)) {
            overflow = false;
            continue;
        }

        int increment = 0;
        if (control & TM_CASCADE) {
            increment = (overflow ? 1 : 0);
        } else {
            elapsed += cycles;
            uint32_t freq;
            switch (control & TM_FREQ_MASK) {
                case TM_FREQ_1: freq = 1; break;
                case TM_FREQ_64: freq = 64; break;
                case TM_FREQ_256: freq = 256; break;
                case TM_FREQ_1024: freq = 1024; break;
                default: std::abort();
            }
            if (elapsed >= freq) {
                increment = elapsed / freq;
                elapsed = elapsed % freq;
            }
        }

        overflow = false;
        for (int n = 0; n < increment; n++) {
            counter++;
            if (counter == 0) {
                counter = reload;
                overflow = true;
            }
        }

        if (overflow) {
            bool fifo_a_tick = BIT(ioreg.soundcnt_h.w, 10) == i;
            bool fifo_b_tick = BIT(ioreg.soundcnt_h.w, 14) == i;
            if (fifo_a_tick) {
                ioreg.fifo_a_ticks = (ioreg.fifo_a_ticks + 1) % 16;
                if (ioreg.fifo_a_ticks == 0) ioreg.fifo_a_refill = true;
            }
            if (fifo_b_tick) {
                ioreg.fifo_b_ticks = (ioreg.fifo_b_ticks + 1) % 16;
                if (ioreg.fifo_b_ticks == 0) ioreg.fifo_b_refill = true;
            }
            if (ioreg.fifo_a_refill || ioreg.fifo_b_refill) {
                dma_update(DMA_AT_REFRESH);
                ioreg.fifo_a_refill = false;
                ioreg.fifo_b_refill = false;
            }
            if (control & TM_IRQ) {
                ioreg.irq.w |= 1 << (3 + i);
            }
        }
    }
}
