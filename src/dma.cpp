// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#include "dma.h"

#include <stdint.h>
#include <cassert>

#include "backup.h"
#include "cpu.h"
#include "io.h"
#include "memory.h"

int dma_channel;
uint32_t dma_pc;

const uint32_t src_addr_mask[4] = {0x07ffffff, 0x0fffffff, 0x0fffffff, 0x0fffffff};
const uint32_t dst_addr_mask[4] = {0x07ffffff, 0x07ffffff, 0x07ffffff, 0x0fffffff};

static void dma_transfer(int ch, uint32_t dst_ctrl, uint32_t src_ctrl, uint32_t &dst_addr, uint32_t &src_addr, uint32_t size, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        bool bad_src_addr = (src_addr < 0x02000000);

        if (size == 4) {
            uint32_t value;
            if (!bad_src_addr) {
                value = memory_read_word(src_addr & ~3);
                ioreg.dma[ch].value.dw = value;
            }
            value = ioreg.dma[ch].value.dw;
            memory_write_word(dst_addr & ~3, value);
        } else {
            uint16_t value;
            if (!bad_src_addr) {
                value = memory_read_halfword(src_addr & ~1);
                ioreg.dma[ch].value.w.w0 = value;
                ioreg.dma[ch].value.w.w1 = value;
            }
            if (dst_addr & 2) {
                value = ioreg.dma[ch].value.w.w1;
            } else {
                value = ioreg.dma[ch].value.w.w0;
            }
            memory_write_halfword(dst_addr & ~1, value);
        }

        switch (dst_ctrl) {
            case DMA_INC:
            case DMA_RELOAD:
                dst_addr += size;
                break;
            case DMA_DEC:
                dst_addr -= size;
                break;
            case DMA_FIXED:
                break;
        }
        switch (src_ctrl) {
            case DMA_INC:
                src_addr += size;
                break;
            case DMA_DEC:
                src_addr -= size;
                break;
            case DMA_FIXED:
            case DMA_RELOAD:
                break;
        }
        dst_addr &= dst_addr_mask[ch];
        src_addr &= src_addr_mask[ch];
    }
}

void dma_reset(int ch) {
    uint32_t sad = ioreg.dma[ch].sad.dw;
    uint32_t dad = ioreg.dma[ch].dad.dw;
    uint32_t cnt = ioreg.dma[ch].cnt.dw;

    ioreg.dma[ch].src_addr = sad;
    ioreg.dma[ch].dst_addr = dad;
    ioreg.dma[ch].count = (uint16_t) cnt;
    if (ioreg.dma[ch].count == 0) ioreg.dma[ch].count = (ch == 3 ? 0x10000 : 0x4000);
}

void dma_update(uint32_t current_timing) {
    for (int ch = 0; ch < 4; ch++) {
        uint32_t dad = ioreg.dma[ch].dad.dw;
        uint32_t cnt = ioreg.dma[ch].cnt.dw;
        uint32_t start_timing = BITS(cnt, 28, 29);

        if (!(cnt & DMA_ENABLE)) continue;
        if (start_timing != current_timing) continue;

        uint32_t &dst_addr = ioreg.dma[ch].dst_addr;
        uint32_t &src_addr = ioreg.dma[ch].src_addr;
        uint16_t count = ioreg.dma[ch].count;

        uint32_t dst_ctrl = BITS(cnt, 21, 22);
        uint32_t src_ctrl = BITS(cnt, 23, 24);
        if (src_addr >= 0x08000000 && src_addr < 0x0e000000) src_ctrl = DMA_INC;
        bool word_size = (cnt & DMA_32);

        if (start_timing == DMA_AT_REFRESH) {
            if (ch == 0) {
                continue;
            } else if (ch == 1 || ch == 2) {
                if (!(dst_addr == 0x40000a0 || dst_addr == 0x40000a4)) continue;
                assert(cnt & DMA_REPEAT);
                if (dst_addr == 0x40000a0 && !ioreg.fifo_a_refill) continue;
                if (dst_addr == 0x40000a4 && !ioreg.fifo_b_refill) continue;
                dst_ctrl = DMA_FIXED;
                word_size = true;
                count = 4;
            } else if (ch == 3) {
                continue;  // FIXME Implement video capture DMA
            }
        }

        assert(!(cnt & DMA_DRQ));

        // EEPROM size autodetect
        if (has_eeprom && dst_addr >= (game_rom_size <= 0x1000000 ? 0x0d000000 : 0x0dffff00) && dst_addr < 0x0e000000) {
            if (count == 9 || count == 73) {
                eeprom_width = 6;
            } else if (count == 17 || count == 81) {
                eeprom_width = 14;
            }
        }

        dma_pc = get_pc();
        dma_channel = ch;
        dma_transfer(ch, dst_ctrl, src_ctrl, dst_addr, src_addr, word_size ? 4 : 2, count);
        dma_channel = -1;

        if (cnt & DMA_IRQ) {
            ioreg.irq.w |= 1 << (8 + ch);
        }

        if (cnt & DMA_REPEAT) {
            if (dst_ctrl == DMA_RELOAD) ioreg.dma[ch].dst_addr = dad;
            ioreg.dma[ch].count = (uint16_t) cnt;
            if (ioreg.dma[ch].count == 0) ioreg.dma[ch].count = (ch == 3 ? 0x10000 : 0x4000);
        } else {
            ioreg.dma[ch].cnt.dw &= ~DMA_ENABLE;
        }
    }
}
