// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#include "io.h"

#include <stdint.h>

#include <fmt/core.h>

#include "audio.h"
#include "cpu.h"
#include "dma.h"
#include "memory.h"
#include "system.h"
#include "timer.h"
#include "video.h"

//#define LOG_BAD_MEMORY_ACCESS

io_registers ioreg;

bool halted;
bool sound_powered;

static void check_keypad_interrupt() {
    if (BIT(ioreg.keycnt.w, 14)) {
        uint16_t held = ~ioreg.keyinput.w & 0x3ff;
        uint16_t mask = ioreg.keycnt.w & 0x3ff;
        if (BIT(ioreg.keycnt.w, 15)) {
            if ((held & mask) == mask) {  // All keys held
                ioreg.irq.w |= INT_BUTTON;
            }
        } else {
            if (held & mask) {  // Any key held
                ioreg.irq.w |= INT_BUTTON;
            }
        }
    }
}

static void set_sound_powered(bool flag) {
    sound_powered = flag;
    if (!sound_powered) {
        ioreg.sound1cnt_l.w = 0;
        ioreg.sound1cnt_h.w = 0;
        ioreg.sound1cnt_x.w = 0;
        ioreg.sound2cnt_l.w = 0;
        ioreg.sound2cnt_h.w = 0;
        ioreg.sound3cnt_l.w = 0;
        ioreg.sound3cnt_h.w = 0;
        ioreg.sound3cnt_x.w = 0;
        ioreg.sound4cnt_l.w = 0;
        ioreg.sound4cnt_h.w = 0;
        ioreg.soundcnt_l.w = 0;
        ioreg.soundcnt_x.w = 0;
    }
}

static uint8_t io_read_byte_discrete(uint32_t address) {
    switch (address) {
        case REG_DISPCNT + 0: return ioreg.dispcnt.b.b0;
        case REG_DISPCNT + 1: return ioreg.dispcnt.b.b1;
        case REG_DISPSTAT + 0: return ioreg.dispstat.b.b0;
        case REG_DISPSTAT + 1: return ioreg.dispstat.b.b1;
        case REG_VCOUNT + 0: return ioreg.vcount.b.b0;
        case REG_VCOUNT + 1: return ioreg.vcount.b.b1;
        case REG_BG0CNT + 0: return ioreg.bgcnt[0].b.b0;
        case REG_BG0CNT + 1: return ioreg.bgcnt[0].b.b1;
        case REG_BG1CNT + 0: return ioreg.bgcnt[1].b.b0;
        case REG_BG1CNT + 1: return ioreg.bgcnt[1].b.b1;
        case REG_BG2CNT + 0: return ioreg.bgcnt[2].b.b0;
        case REG_BG2CNT + 1: return ioreg.bgcnt[2].b.b1;
        case REG_BG3CNT + 0: return ioreg.bgcnt[3].b.b0;
        case REG_BG3CNT + 1: return ioreg.bgcnt[3].b.b1;
        case REG_WININ + 0: return ioreg.winin.b.b0;
        case REG_WININ + 1: return ioreg.winin.b.b1;
        case REG_WINOUT + 0: return ioreg.winout.b.b0;
        case REG_WINOUT + 1: return ioreg.winout.b.b1;
        case REG_BLDCNT + 0: return ioreg.bldcnt.b.b0;
        case REG_BLDCNT + 1: return ioreg.bldcnt.b.b1;
        case REG_BLDALPHA + 0: return ioreg.bldalpha.b.b0;
        case REG_BLDALPHA + 1: return ioreg.bldalpha.b.b1;

        case REG_SOUND1CNT_L + 0: return ioreg.sound1cnt_l.b.b0 & 0x7f;
        case REG_SOUND1CNT_L + 1: return 0;
        case REG_SOUND1CNT_H + 0: return ioreg.sound1cnt_h.b.b0 & 0xc0;
        case REG_SOUND1CNT_H + 1: return ioreg.sound1cnt_h.b.b1;
        case REG_SOUND1CNT_X + 0: return 0;
        case REG_SOUND1CNT_X + 1: return ioreg.sound1cnt_x.b.b1 & 0x40;
        case REG_SOUND1CNT_X + 2: return 0;
        case REG_SOUND1CNT_X + 3: return 0;
        case REG_SOUND2CNT_L + 0: return ioreg.sound2cnt_l.b.b0 & 0xc0;
        case REG_SOUND2CNT_L + 1: return ioreg.sound2cnt_l.b.b1;
        case REG_SOUND2CNT_L + 2: return 0;
        case REG_SOUND2CNT_L + 3: return 0;
        case REG_SOUND2CNT_H + 0: return 0;
        case REG_SOUND2CNT_H + 1: return ioreg.sound2cnt_h.b.b1 & 0x40;
        case REG_SOUND2CNT_H + 2: return 0;
        case REG_SOUND2CNT_H + 3: return 0;
        case REG_SOUND3CNT_L + 0: return ioreg.sound3cnt_l.b.b0 & 0xe0;
        case REG_SOUND3CNT_L + 1: return 0;
        case REG_SOUND3CNT_H + 0: return 0;
        case REG_SOUND3CNT_H + 1: return ioreg.sound3cnt_h.b.b1 & 0xe0;
        case REG_SOUND3CNT_X + 0: return 0;
        case REG_SOUND3CNT_X + 1: return ioreg.sound3cnt_x.b.b1 & 0x40;
        case REG_SOUND3CNT_X + 2: return 0;
        case REG_SOUND3CNT_X + 3: return 0;
        case REG_SOUND4CNT_L + 0: return 0;
        case REG_SOUND4CNT_L + 1: return ioreg.sound4cnt_l.b.b1;
        case REG_SOUND4CNT_L + 2: return 0;
        case REG_SOUND4CNT_L + 3: return 0;
        case REG_SOUND4CNT_H + 0: return ioreg.sound4cnt_h.b.b0;
        case REG_SOUND4CNT_H + 1: return ioreg.sound4cnt_h.b.b1 & 0x40;
        case REG_SOUND4CNT_H + 2: return 0;
        case REG_SOUND4CNT_H + 3: return 0;
        case REG_SOUNDCNT_L + 0: return ioreg.soundcnt_l.b.b0 & 0x77;
        case REG_SOUNDCNT_L + 1: return ioreg.soundcnt_l.b.b1;
        case REG_SOUNDCNT_H + 0: return ioreg.soundcnt_h.b.b0 & 0x0f;
        case REG_SOUNDCNT_H + 1: return ioreg.soundcnt_h.b.b1 & 0x77;
        case REG_SOUNDCNT_X + 0: return ioreg.soundcnt_x.b.b0 & 0x8f;
        case REG_SOUNDCNT_X + 1: return 0;
        case REG_SOUNDCNT_X + 2: return 0;
        case REG_SOUNDCNT_X + 3: return 0;
        case REG_SOUNDBIAS + 0: return ioreg.soundbias.b.b0;
        case REG_SOUNDBIAS + 1: return ioreg.soundbias.b.b1;
        case REG_SOUNDBIAS + 2: return 0;
        case REG_SOUNDBIAS + 3: return 0;
        case REG_WAVE_RAM0_L + 0: return ioreg.wave_ram0.b.b0;
        case REG_WAVE_RAM0_L + 1: return ioreg.wave_ram0.b.b1;
        case REG_WAVE_RAM0_H + 0: return ioreg.wave_ram0.b.b2;
        case REG_WAVE_RAM0_H + 1: return ioreg.wave_ram0.b.b3;
        case REG_WAVE_RAM1_L + 0: return ioreg.wave_ram1.b.b0;
        case REG_WAVE_RAM1_L + 1: return ioreg.wave_ram1.b.b1;
        case REG_WAVE_RAM1_H + 0: return ioreg.wave_ram1.b.b2;
        case REG_WAVE_RAM1_H + 1: return ioreg.wave_ram1.b.b3;
        case REG_WAVE_RAM2_L + 0: return ioreg.wave_ram2.b.b0;
        case REG_WAVE_RAM2_L + 1: return ioreg.wave_ram2.b.b1;
        case REG_WAVE_RAM2_H + 0: return ioreg.wave_ram2.b.b2;
        case REG_WAVE_RAM2_H + 1: return ioreg.wave_ram2.b.b3;
        case REG_WAVE_RAM3_L + 0: return ioreg.wave_ram3.b.b0;
        case REG_WAVE_RAM3_L + 1: return ioreg.wave_ram3.b.b1;
        case REG_WAVE_RAM3_H + 0: return ioreg.wave_ram3.b.b2;
        case REG_WAVE_RAM3_H + 1: return ioreg.wave_ram3.b.b3;

        case REG_DMA0CNT_L + 0: return 0;
        case REG_DMA0CNT_L + 1: return 0;
        case REG_DMA0CNT_H + 0: return ioreg.dma[0].cnt.b.b2;
        case REG_DMA0CNT_H + 1: return ioreg.dma[0].cnt.b.b3;
        case REG_DMA1CNT_L + 0: return 0;
        case REG_DMA1CNT_L + 1: return 0;
        case REG_DMA1CNT_H + 0: return ioreg.dma[1].cnt.b.b2;
        case REG_DMA1CNT_H + 1: return ioreg.dma[1].cnt.b.b3;
        case REG_DMA2CNT_L + 0: return 0;
        case REG_DMA2CNT_L + 1: return 0;
        case REG_DMA2CNT_H + 0: return ioreg.dma[2].cnt.b.b2;
        case REG_DMA2CNT_H + 1: return ioreg.dma[2].cnt.b.b3;
        case REG_DMA3CNT_L + 0: return 0;
        case REG_DMA3CNT_L + 1: return 0;
        case REG_DMA3CNT_H + 0: return ioreg.dma[3].cnt.b.b2;
        case REG_DMA3CNT_H + 1: return ioreg.dma[3].cnt.b.b3;

        case REG_TM0CNT_L + 0: return ioreg.timer[0].counter.b.b0;
        case REG_TM0CNT_L + 1: return ioreg.timer[0].counter.b.b1;
        case REG_TM0CNT_H + 0: return ioreg.timer[0].control.b.b0;
        case REG_TM0CNT_H + 1: return ioreg.timer[0].control.b.b1;
        case REG_TM1CNT_L + 0: return ioreg.timer[1].counter.b.b0;
        case REG_TM1CNT_L + 1: return ioreg.timer[1].counter.b.b1;
        case REG_TM1CNT_H + 0: return ioreg.timer[1].control.b.b0;
        case REG_TM1CNT_H + 1: return ioreg.timer[1].control.b.b1;
        case REG_TM2CNT_L + 0: return ioreg.timer[2].counter.b.b0;
        case REG_TM2CNT_L + 1: return ioreg.timer[2].counter.b.b1;
        case REG_TM2CNT_H + 0: return ioreg.timer[2].control.b.b0;
        case REG_TM2CNT_H + 1: return ioreg.timer[2].control.b.b1;
        case REG_TM3CNT_L + 0: return ioreg.timer[3].counter.b.b0;
        case REG_TM3CNT_L + 1: return ioreg.timer[3].counter.b.b1;
        case REG_TM3CNT_H + 0: return ioreg.timer[3].control.b.b0;
        case REG_TM3CNT_H + 1: return ioreg.timer[3].control.b.b1;

        case REG_SIOMULTI0 + 0: return ioreg.siomulti[0].b.b0;
        case REG_SIOMULTI0 + 1: return ioreg.siomulti[0].b.b1;
        case REG_SIOMULTI1 + 0: return ioreg.siomulti[1].b.b0;
        case REG_SIOMULTI1 + 1: return ioreg.siomulti[1].b.b1;
        case REG_SIOMULTI2 + 0: return ioreg.siomulti[2].b.b0;
        case REG_SIOMULTI2 + 1: return ioreg.siomulti[2].b.b1;
        case REG_SIOMULTI3 + 0: return ioreg.siomulti[3].b.b0;
        case REG_SIOMULTI3 + 1: return ioreg.siomulti[3].b.b1;
        case REG_SIOCNT + 0: return ioreg.siocnt.b.b0;
        case REG_SIOCNT + 1: return ioreg.siocnt.b.b1;
        case REG_SIOMLT_SEND + 0: return ioreg.siomlt_send.b.b0;
        case REG_SIOMLT_SEND + 1: return ioreg.siomlt_send.b.b1;

        case REG_RCNT + 0: return ioreg.rcnt.b.b0;
        case REG_RCNT + 1: return ioreg.rcnt.b.b1;
        case REG_JOYCNT + 0: return ioreg.joycnt.b.b0;
        case REG_JOYCNT + 1: return ioreg.joycnt.b.b1;
        case REG_JOYCNT + 2: return 0;
        case REG_JOYCNT + 3: return 0;
        case REG_JOY_RECV_L + 0: return ioreg.joy_recv.b.b0;
        case REG_JOY_RECV_L + 1: return ioreg.joy_recv.b.b1;
        case REG_JOY_RECV_H + 0: return ioreg.joy_recv.b.b2;
        case REG_JOY_RECV_H + 1: return ioreg.joy_recv.b.b3;
        case REG_JOY_TRANS_L + 0: return ioreg.joy_trans.b.b0;
        case REG_JOY_TRANS_L + 1: return ioreg.joy_trans.b.b1;
        case REG_JOY_TRANS_H + 0: return ioreg.joy_trans.b.b2;
        case REG_JOY_TRANS_H + 1: return ioreg.joy_trans.b.b3;
        case REG_JOYSTAT + 0: return ioreg.joystat.b.b0;
        case REG_JOYSTAT + 1: return ioreg.joystat.b.b1;
        case REG_JOYSTAT + 2: return 0;
        case REG_JOYSTAT + 3: return 0;

        case REG_IE + 0: return ioreg.ie.b.b0;
        case REG_IE + 1: return ioreg.ie.b.b1;
        case REG_IF + 0: return ioreg.irq.b.b0;
        case REG_IF + 1: return ioreg.irq.b.b1;
        case REG_WAITCNT + 0: return ioreg.waitcnt.b.b0;
        case REG_WAITCNT + 1: return ioreg.waitcnt.b.b1;
        case REG_WAITCNT + 2: return 0;
        case REG_WAITCNT + 3: return 0;
        case REG_IME + 0: return ioreg.ime.b.b0;
        case REG_IME + 1: return ioreg.ime.b.b1;
        case REG_IME + 2: return 0;
        case REG_IME + 3: return 0;
        case REG_POSTFLG: return ioreg.postflg;
        case REG_HALTCNT: return 0;

        default:
#ifdef LOG_BAD_MEMORY_ACCESS
            fmt::print("io_read_byte_discrete(0x{:08x});\n", address);
#endif
            break;
    }

    return (uint8_t) (memory_open_bus() >> 8 * (address & 3));
}

static void io_write_byte_discrete(uint32_t address, uint8_t value) {
    uint8_t old_value;

    switch (address) {
        case REG_DISPCNT + 0: ioreg.dispcnt.b.b0 = (ioreg.dispcnt.b.b0 & 0x08) | (value & 0xf7); break;
        case REG_DISPCNT + 1: ioreg.dispcnt.b.b1 = value; break;
        case REG_DISPSTAT + 0: ioreg.dispstat.b.b0 = (ioreg.dispstat.b.b0 & 0x07) | (value & 0x38); break;
        case REG_DISPSTAT + 1: ioreg.dispstat.b.b1 = value; break;
        case REG_BG0CNT + 0: ioreg.bgcnt[0].b.b0 = value; break;
        case REG_BG0CNT + 1: ioreg.bgcnt[0].b.b1 = value & 0xdf; break;
        case REG_BG1CNT + 0: ioreg.bgcnt[1].b.b0 = value; break;
        case REG_BG1CNT + 1: ioreg.bgcnt[1].b.b1 = value & 0xdf; break;
        case REG_BG2CNT + 0: ioreg.bgcnt[2].b.b0 = value; break;
        case REG_BG2CNT + 1: ioreg.bgcnt[2].b.b1 = value; break;
        case REG_BG3CNT + 0: ioreg.bgcnt[3].b.b0 = value; break;
        case REG_BG3CNT + 1: ioreg.bgcnt[3].b.b1 = value; break;
        case REG_BG0HOFS + 0: ioreg.bg_text[0].x.b.b0 = value; break;
        case REG_BG0HOFS + 1: ioreg.bg_text[0].x.b.b1 = value & 0x01; break;
        case REG_BG0VOFS + 0: ioreg.bg_text[0].y.b.b0 = value; break;
        case REG_BG0VOFS + 1: ioreg.bg_text[0].y.b.b1 = value & 0x01; break;
        case REG_BG1HOFS + 0: ioreg.bg_text[1].x.b.b0 = value; break;
        case REG_BG1HOFS + 1: ioreg.bg_text[1].x.b.b1 = value & 0x01; break;
        case REG_BG1VOFS + 0: ioreg.bg_text[1].y.b.b0 = value; break;
        case REG_BG1VOFS + 1: ioreg.bg_text[1].y.b.b1 = value & 0x01; break;
        case REG_BG2HOFS + 0: ioreg.bg_text[2].x.b.b0 = value; break;
        case REG_BG2HOFS + 1: ioreg.bg_text[2].x.b.b1 = value & 0x01; break;
        case REG_BG2VOFS + 0: ioreg.bg_text[2].y.b.b0 = value; break;
        case REG_BG2VOFS + 1: ioreg.bg_text[2].y.b.b1 = value & 0x01; break;
        case REG_BG3HOFS + 0: ioreg.bg_text[3].x.b.b0 = value; break;
        case REG_BG3HOFS + 1: ioreg.bg_text[3].x.b.b1 = value & 0x01; break;
        case REG_BG3VOFS + 0: ioreg.bg_text[3].y.b.b0 = value; break;
        case REG_BG3VOFS + 1: ioreg.bg_text[3].y.b.b1 = value & 0x01; break;
        case REG_BG2PA + 0: ioreg.bg_affine[0].pa.b.b0 = value; break;
        case REG_BG2PA + 1: ioreg.bg_affine[0].pa.b.b1 = value; break;
        case REG_BG2PB + 0: ioreg.bg_affine[0].pb.b.b0 = value; break;
        case REG_BG2PB + 1: ioreg.bg_affine[0].pb.b.b1 = value; break;
        case REG_BG2PC + 0: ioreg.bg_affine[0].pc.b.b0 = value; break;
        case REG_BG2PC + 1: ioreg.bg_affine[0].pc.b.b1 = value; break;
        case REG_BG2PD + 0: ioreg.bg_affine[0].pd.b.b0 = value; break;
        case REG_BG2PD + 1: ioreg.bg_affine[0].pd.b.b1 = value; break;
        case REG_BG2X_L + 0:
            ioreg.bg_affine[0].x0.b.b0 = value;
            video_bg_affine_reset(0);
            break;
        case REG_BG2X_L + 1:
            ioreg.bg_affine[0].x0.b.b1 = value;
            video_bg_affine_reset(0);
            break;
        case REG_BG2X_H + 0:
            ioreg.bg_affine[0].x0.b.b2 = value;
            video_bg_affine_reset(0);
            break;
        case REG_BG2X_H + 1:
            ioreg.bg_affine[0].x0.b.b3 = value & 0x0f;
            video_bg_affine_reset(0);
            break;
        case REG_BG2Y_L + 0:
            ioreg.bg_affine[0].y0.b.b0 = value;
            video_bg_affine_reset(0);
            break;
        case REG_BG2Y_L + 1:
            ioreg.bg_affine[0].y0.b.b1 = value;
            video_bg_affine_reset(0);
            break;
        case REG_BG2Y_H + 0:
            ioreg.bg_affine[0].y0.b.b2 = value;
            video_bg_affine_reset(0);
            break;
        case REG_BG2Y_H + 1:
            ioreg.bg_affine[0].y0.b.b3 = value & 0x0f;
            video_bg_affine_reset(0);
            break;
        case REG_BG3PA + 0: ioreg.bg_affine[1].pa.b.b0 = value; break;
        case REG_BG3PA + 1: ioreg.bg_affine[1].pa.b.b1 = value; break;
        case REG_BG3PB + 0: ioreg.bg_affine[1].pb.b.b0 = value; break;
        case REG_BG3PB + 1: ioreg.bg_affine[1].pb.b.b1 = value; break;
        case REG_BG3PC + 0: ioreg.bg_affine[1].pc.b.b0 = value; break;
        case REG_BG3PC + 1: ioreg.bg_affine[1].pc.b.b1 = value; break;
        case REG_BG3PD + 0: ioreg.bg_affine[1].pd.b.b0 = value; break;
        case REG_BG3PD + 1: ioreg.bg_affine[1].pd.b.b1 = value; break;
        case REG_BG3X_L + 0:
            ioreg.bg_affine[1].x0.b.b0 = value;
            video_bg_affine_reset(1);
            break;
        case REG_BG3X_L + 1:
            ioreg.bg_affine[1].x0.b.b1 = value;
            video_bg_affine_reset(1);
            break;
        case REG_BG3X_H + 0:
            ioreg.bg_affine[1].x0.b.b2 = value;
            video_bg_affine_reset(1);
            break;
        case REG_BG3X_H + 1:
            ioreg.bg_affine[1].x0.b.b3 = value & 0x0f;
            video_bg_affine_reset(1);
            break;
        case REG_BG3Y_L + 0:
            ioreg.bg_affine[1].y0.b.b0 = value;
            video_bg_affine_reset(1);
            break;
        case REG_BG3Y_L + 1:
            ioreg.bg_affine[1].y0.b.b1 = value;
            video_bg_affine_reset(1);
            break;
        case REG_BG3Y_H + 0:
            ioreg.bg_affine[1].y0.b.b2 = value;
            video_bg_affine_reset(1);
            break;
        case REG_BG3Y_H + 1:
            ioreg.bg_affine[1].y0.b.b3 = value & 0x0f;
            video_bg_affine_reset(1);
            break;
        case REG_WIN0H + 0: ioreg.winh[0].b.b0 = value; break;
        case REG_WIN0H + 1: ioreg.winh[0].b.b1 = value; break;
        case REG_WIN1H + 0: ioreg.winh[1].b.b0 = value; break;
        case REG_WIN1H + 1: ioreg.winh[1].b.b1 = value; break;
        case REG_WIN0V + 0: ioreg.winv[0].b.b0 = value; break;
        case REG_WIN0V + 1: ioreg.winv[0].b.b1 = value; break;
        case REG_WIN1V + 0: ioreg.winv[1].b.b0 = value; break;
        case REG_WIN1V + 1: ioreg.winv[1].b.b1 = value; break;
        case REG_WININ + 0: ioreg.winin.b.b0 = value & 0x3f; break;
        case REG_WININ + 1: ioreg.winin.b.b1 = value & 0x3f; break;
        case REG_WINOUT + 0: ioreg.winout.b.b0 = value & 0x3f; break;
        case REG_WINOUT + 1: ioreg.winout.b.b1 = value & 0x3f; break;
        case REG_MOSAIC + 0: ioreg.mosaic.b.b0 = value; break;
        case REG_MOSAIC + 1: ioreg.mosaic.b.b1 = value; break;
        case REG_BLDCNT + 0: ioreg.bldcnt.b.b0 = value; break;
        case REG_BLDCNT + 1: ioreg.bldcnt.b.b1 = value & 0x3f; break;
        case REG_BLDALPHA + 0: ioreg.bldalpha.b.b0 = value & 0x1f; break;
        case REG_BLDALPHA + 1: ioreg.bldalpha.b.b1 = value & 0x1f; break;
        case REG_BLDY + 0: ioreg.bldy.b.b0 = value & 0x1f; break;
        case REG_BLDY + 1: break;

        case REG_SOUND1CNT_L + 0:
            if (sound_powered) ioreg.sound1cnt_l.b.b0 = value & 0x7f;
            break;
        case REG_SOUND1CNT_L + 1:
            break;
        case REG_SOUND1CNT_H + 0:
            if (sound_powered) ioreg.sound1cnt_h.b.b0 = value;
            break;
        case REG_SOUND1CNT_H + 1:
            if (sound_powered) ioreg.sound1cnt_h.b.b1 = value;
            break;
        case REG_SOUND1CNT_X + 0:
            if (sound_powered) ioreg.sound1cnt_x.b.b0 = value;
            break;
        case REG_SOUND1CNT_X + 1:
            if (sound_powered) ioreg.sound1cnt_x.b.b1 = value & 0xc7;
            break;
        case REG_SOUND2CNT_L + 0:
            if (sound_powered) ioreg.sound2cnt_l.b.b0 = value;
            break;
        case REG_SOUND2CNT_L + 1:
            if (sound_powered) ioreg.sound2cnt_l.b.b1 = value;
            break;
        case REG_SOUND2CNT_H + 0:
            if (sound_powered) ioreg.sound2cnt_h.b.b0 = value;
            break;
        case REG_SOUND2CNT_H + 1:
            if (sound_powered) ioreg.sound2cnt_h.b.b1 = value & 0xc7;
            break;
        case REG_SOUND3CNT_L + 0:
            if (sound_powered) ioreg.sound3cnt_l.b.b0 = value & 0xe0;
            break;
        case REG_SOUND3CNT_L + 1:
            break;
        case REG_SOUND3CNT_H + 0:
            if (sound_powered) ioreg.sound3cnt_h.b.b0 = value;
            break;
        case REG_SOUND3CNT_H + 1:
            if (sound_powered) ioreg.sound3cnt_h.b.b1 = value & 0xe0;
            break;
        case REG_SOUND3CNT_X + 0:
            if (sound_powered) ioreg.sound3cnt_x.b.b0 = value;
            break;
        case REG_SOUND3CNT_X + 1:
            if (sound_powered) ioreg.sound3cnt_x.b.b1 = value & 0xc7;
            break;
        case REG_SOUND4CNT_L + 0:
            if (sound_powered) ioreg.sound4cnt_l.b.b0 = value & 0x3f;
            break;
        case REG_SOUND4CNT_L + 1:
            if (sound_powered) ioreg.sound4cnt_l.b.b1 = value;
            break;
        case REG_SOUND4CNT_H + 0:
            if (sound_powered) ioreg.sound4cnt_h.b.b0 = value;
            break;
        case REG_SOUND4CNT_H + 1:
            if (sound_powered) ioreg.sound4cnt_h.b.b1 = value & 0xc0;
            break;
        case REG_SOUNDCNT_L + 0:
            if (sound_powered) ioreg.soundcnt_l.b.b0 = value & 0x77;
            break;
        case REG_SOUNDCNT_L + 1:
            if (sound_powered) ioreg.soundcnt_l.b.b1 = value;
            break;
        case REG_SOUNDCNT_H + 0:
            ioreg.soundcnt_h.b.b0 = value & 0x0f;
            break;
        case REG_SOUNDCNT_H + 1:
            ioreg.soundcnt_h.b.b1 = value;
            break;
        case REG_SOUNDCNT_X + 0:
            ioreg.soundcnt_x.b.b0 = (ioreg.soundcnt_x.b.b0 & 0x0f) | (value & 0x80);
            set_sound_powered(value & 0x80);
            break;
        case REG_SOUNDCNT_X + 1:
            break;
        case REG_SOUNDBIAS + 0: ioreg.soundbias.b.b0 = value & 0xfe; break;
        case REG_SOUNDBIAS + 1: ioreg.soundbias.b.b1 = value & 0xc3; break;
        case REG_WAVE_RAM0_L + 0: ioreg.wave_ram0.b.b0 = value; break;
        case REG_WAVE_RAM0_L + 1: ioreg.wave_ram0.b.b1 = value; break;
        case REG_WAVE_RAM0_H + 0: ioreg.wave_ram0.b.b2 = value; break;
        case REG_WAVE_RAM0_H + 1: ioreg.wave_ram0.b.b3 = value; break;
        case REG_WAVE_RAM1_L + 0: ioreg.wave_ram1.b.b0 = value; break;
        case REG_WAVE_RAM1_L + 1: ioreg.wave_ram1.b.b1 = value; break;
        case REG_WAVE_RAM1_H + 0: ioreg.wave_ram1.b.b2 = value; break;
        case REG_WAVE_RAM1_H + 1: ioreg.wave_ram1.b.b3 = value; break;
        case REG_WAVE_RAM2_L + 0: ioreg.wave_ram2.b.b0 = value; break;
        case REG_WAVE_RAM2_L + 1: ioreg.wave_ram2.b.b1 = value; break;
        case REG_WAVE_RAM2_H + 0: ioreg.wave_ram2.b.b2 = value; break;
        case REG_WAVE_RAM2_H + 1: ioreg.wave_ram2.b.b3 = value; break;
        case REG_WAVE_RAM3_L + 0: ioreg.wave_ram3.b.b0 = value; break;
        case REG_WAVE_RAM3_L + 1: ioreg.wave_ram3.b.b1 = value; break;
        case REG_WAVE_RAM3_H + 0: ioreg.wave_ram3.b.b2 = value; break;
        case REG_WAVE_RAM3_H + 1: ioreg.wave_ram3.b.b3 = value; break;

        case REG_DMA0SAD_L + 0: ioreg.dma[0].sad.b.b0 = value; break;
        case REG_DMA0SAD_L + 1: ioreg.dma[0].sad.b.b1 = value; break;
        case REG_DMA0SAD_H + 0: ioreg.dma[0].sad.b.b2 = value; break;
        case REG_DMA0SAD_H + 1: ioreg.dma[0].sad.b.b3 = value; break;
        case REG_DMA0DAD_L + 0: ioreg.dma[0].dad.b.b0 = value; break;
        case REG_DMA0DAD_L + 1: ioreg.dma[0].dad.b.b1 = value; break;
        case REG_DMA0DAD_H + 0: ioreg.dma[0].dad.b.b2 = value; break;
        case REG_DMA0DAD_H + 1: ioreg.dma[0].dad.b.b3 = value; break;
        case REG_DMA0CNT_L + 0: ioreg.dma[0].cnt.b.b0 = value; break;
        case REG_DMA0CNT_L + 1: ioreg.dma[0].cnt.b.b1 = value & 0x3f; break;
        case REG_DMA0CNT_H + 0: ioreg.dma[0].cnt.b.b2 = value & 0xe0; break;
        case REG_DMA0CNT_H + 1:
            old_value = ioreg.dma[0].cnt.b.b3;
            ioreg.dma[0].cnt.b.b3 = value & 0xf7;
            if (!(old_value & 0x80) && (value & 0x80)) {
                dma_reset(0);
                dma_update(DMA_NOW);
            }
            break;
        case REG_DMA1SAD_L + 0: ioreg.dma[1].sad.b.b0 = value; break;
        case REG_DMA1SAD_L + 1: ioreg.dma[1].sad.b.b1 = value; break;
        case REG_DMA1SAD_H + 0: ioreg.dma[1].sad.b.b2 = value; break;
        case REG_DMA1SAD_H + 1: ioreg.dma[1].sad.b.b3 = value; break;
        case REG_DMA1DAD_L + 0: ioreg.dma[1].dad.b.b0 = value; break;
        case REG_DMA1DAD_L + 1: ioreg.dma[1].dad.b.b1 = value; break;
        case REG_DMA1DAD_H + 0: ioreg.dma[1].dad.b.b2 = value; break;
        case REG_DMA1DAD_H + 1: ioreg.dma[1].dad.b.b3 = value; break;
        case REG_DMA1CNT_L + 0: ioreg.dma[1].cnt.b.b0 = value; break;
        case REG_DMA1CNT_L + 1: ioreg.dma[1].cnt.b.b1 = value & 0x3f; break;
        case REG_DMA1CNT_H + 0: ioreg.dma[1].cnt.b.b2 = value & 0xe0; break;
        case REG_DMA1CNT_H + 1:
            old_value = ioreg.dma[1].cnt.b.b3;
            ioreg.dma[1].cnt.b.b3 = value & 0xf7;
            if (!(old_value & 0x80) && (value & 0x80)) {
                dma_reset(1);
                dma_update(DMA_NOW);
            }
            break;
        case REG_DMA2SAD_L + 0: ioreg.dma[2].sad.b.b0 = value; break;
        case REG_DMA2SAD_L + 1: ioreg.dma[2].sad.b.b1 = value; break;
        case REG_DMA2SAD_H + 0: ioreg.dma[2].sad.b.b2 = value; break;
        case REG_DMA2SAD_H + 1: ioreg.dma[2].sad.b.b3 = value; break;
        case REG_DMA2DAD_L + 0: ioreg.dma[2].dad.b.b0 = value; break;
        case REG_DMA2DAD_L + 1: ioreg.dma[2].dad.b.b1 = value; break;
        case REG_DMA2DAD_H + 0: ioreg.dma[2].dad.b.b2 = value; break;
        case REG_DMA2DAD_H + 1: ioreg.dma[2].dad.b.b3 = value; break;
        case REG_DMA2CNT_L + 0: ioreg.dma[2].cnt.b.b0 = value; break;
        case REG_DMA2CNT_L + 1: ioreg.dma[2].cnt.b.b1 = value & 0x3f; break;
        case REG_DMA2CNT_H + 0: ioreg.dma[2].cnt.b.b2 = value & 0xe0; break;
        case REG_DMA2CNT_H + 1:
            old_value = ioreg.dma[2].cnt.b.b3;
            ioreg.dma[2].cnt.b.b3 = value & 0xf7;
            if (!(old_value & 0x80) && (value & 0x80)) {
                dma_reset(2);
                dma_update(DMA_NOW);
            }
            break;
        case REG_DMA3SAD_L + 0: ioreg.dma[3].sad.b.b0 = value; break;
        case REG_DMA3SAD_L + 1: ioreg.dma[3].sad.b.b1 = value; break;
        case REG_DMA3SAD_H + 0: ioreg.dma[3].sad.b.b2 = value; break;
        case REG_DMA3SAD_H + 1: ioreg.dma[3].sad.b.b3 = value; break;
        case REG_DMA3DAD_L + 0: ioreg.dma[3].dad.b.b0 = value; break;
        case REG_DMA3DAD_L + 1: ioreg.dma[3].dad.b.b1 = value; break;
        case REG_DMA3DAD_H + 0: ioreg.dma[3].dad.b.b2 = value; break;
        case REG_DMA3DAD_H + 1: ioreg.dma[3].dad.b.b3 = value; break;
        case REG_DMA3CNT_L + 0: ioreg.dma[3].cnt.b.b0 = value; break;
        case REG_DMA3CNT_L + 1: ioreg.dma[3].cnt.b.b1 = value; break;
        case REG_DMA3CNT_H + 0: ioreg.dma[3].cnt.b.b2 = value & 0xe0; break;
        case REG_DMA3CNT_H + 1:
            old_value = ioreg.dma[3].cnt.b.b3;
            ioreg.dma[3].cnt.b.b3 = value;
            if (!(old_value & 0x80) && (value & 0x80)) {
                dma_reset(3);
                dma_update(DMA_NOW);
            }
            break;

        case REG_TM0CNT_L + 0: ioreg.timer[0].reload.b.b0 = value; break;
        case REG_TM0CNT_L + 1: ioreg.timer[0].reload.b.b1 = value; break;
        case REG_TM0CNT_H + 0:
            old_value = ioreg.timer[0].control.b.b0;
            ioreg.timer[0].control.b.b0 = value & 0xc7;
            if (!(old_value & 0x80) && (value & 0x80)) {
                timer_reset(0);
            }
            break;
        case REG_TM0CNT_H + 1:
            break;
        case REG_TM1CNT_L + 0: ioreg.timer[1].reload.b.b0 = value; break;
        case REG_TM1CNT_L + 1: ioreg.timer[1].reload.b.b1 = value; break;
        case REG_TM1CNT_H + 0:
            old_value = ioreg.timer[1].control.b.b0;
            ioreg.timer[1].control.b.b0 = value & 0xc7;
            if (!(old_value & 0x80) && (value & 0x80)) {
                timer_reset(1);
            }
            break;
        case REG_TM1CNT_H + 1:
            break;
        case REG_TM2CNT_L + 0: ioreg.timer[2].reload.b.b0 = value; break;
        case REG_TM2CNT_L + 1: ioreg.timer[2].reload.b.b1 = value; break;
        case REG_TM2CNT_H + 0:
            old_value = ioreg.timer[2].control.b.b0;
            ioreg.timer[2].control.b.b0 = value & 0xc7;
            if (!(old_value & 0x80) && (value & 0x80)) {
                timer_reset(2);
            }
            break;
        case REG_TM2CNT_H + 1:
            break;
        case REG_TM3CNT_L + 0: ioreg.timer[3].reload.b.b0 = value; break;
        case REG_TM3CNT_L + 1: ioreg.timer[3].reload.b.b1 = value; break;
        case REG_TM3CNT_H + 0:
            old_value = ioreg.timer[3].control.b.b0;
            ioreg.timer[3].control.b.b0 = value & 0xc7;
            if (!(old_value & 0x80) && (value & 0x80)) {
                timer_reset(3);
            }
            break;
        case REG_TM3CNT_H + 1:
            break;

            //case REG_SIOMULTI0 + 0:
            //case REG_SIOMULTI0 + 1:
            //case REG_SIOMULTI1 + 0:
            //case REG_SIOMULTI1 + 1:
            //case REG_SIOMULTI2 + 0:
            //case REG_SIOMULTI2 + 1:
            //case REG_SIOMULTI3 + 0:
            //case REG_SIOMULTI3 + 1:
            //case REG_SIOCNT + 0:
            //case REG_SIOCNT + 1:
            //case REG_SIOMLT_SEND + 0:
            //case REG_SIOMLT_SEND + 1:

        case REG_RCNT + 0:
            ioreg.rcnt.b.b0 = value;
            break;
        case REG_RCNT + 1:
            ioreg.rcnt.b.b1 = value & 0xc1;
            break;

            //case REG_JOYCNT + 0:
            //case REG_JOYCNT + 1:
            //case REG_JOY_RECV_L + 0:
            //case REG_JOY_RECV_L + 1:
            //case REG_JOY_RECV_H + 0:
            //case REG_JOY_RECV_H + 1:
            //case REG_JOY_TRANS_L + 0:
            //case REG_JOY_TRANS_L + 1:
            //case REG_JOY_TRANS_H + 0:
            //case REG_JOY_TRANS_H + 1:
            //case REG_JOYSTAT + 0:
            //case REG_JOYSTAT + 1:

        case REG_IE + 0: ioreg.ie.b.b0 = value; break;
        case REG_IE + 1: ioreg.ie.b.b1 = value & 0x3f; break;
        case REG_IF + 0: ioreg.irq.b.b0 &= ~value; break;
        case REG_IF + 1: ioreg.irq.b.b1 &= ~value; break;
        case REG_WAITCNT + 0: ioreg.waitcnt.b.b0 = value; break;
        case REG_WAITCNT + 1: ioreg.waitcnt.b.b1 = (ioreg.waitcnt.b.b1 & 0x80) | (value & 0x5f); break;
        case REG_IME + 0: ioreg.ime.b.b0 = value & 0x01; break;
        case REG_IME + 1: break;
        case REG_POSTFLG: ioreg.postflg = value & 0x01; break;
        case REG_HALTCNT:
            ioreg.haltcnt = value & 0x80;
            halted = true;
            break;

        default:
#ifdef LOG_BAD_MEMORY_ACCESS
            fmt::print("io_write_byte_discrete(0x{:08x}, 0x{:02x});\n", address, value);
#endif
            break;
    }
}

uint8_t io_read_byte(uint32_t address) {
    switch (address) {
        case REG_KEYINPUT + 0: check_keypad_interrupt(); return ioreg.keyinput.b.b0;
        case REG_KEYINPUT + 1: check_keypad_interrupt(); return ioreg.keyinput.b.b1;
        case REG_KEYCNT + 0: return ioreg.keycnt.b.b0;
        case REG_KEYCNT + 1: return ioreg.keycnt.b.b1;

        default:
            return io_read_byte_discrete(address);
    }
}

void io_write_byte(uint32_t address, uint8_t value) {
    switch (address) {
        case REG_FIFO_A_L + 0: audio_fifo_a(value); break;
        case REG_FIFO_A_L + 1: audio_fifo_a(value << 8); break;
        case REG_FIFO_A_H + 0: audio_fifo_a(value << 16); break;
        case REG_FIFO_A_H + 1: audio_fifo_a(value << 24); break;
        case REG_FIFO_B_L + 0: audio_fifo_b(value); break;
        case REG_FIFO_B_L + 1: audio_fifo_b(value << 8); break;
        case REG_FIFO_B_H + 0: audio_fifo_b(value << 16); break;
        case REG_FIFO_B_H + 1: audio_fifo_b(value << 24); break;

        case REG_KEYCNT + 0:
            ioreg.keycnt.b.b0 = value;
            check_keypad_interrupt();
            break;
        case REG_KEYCNT + 1:
            ioreg.keycnt.b.b1 = value & 0xc3;
            check_keypad_interrupt();
            break;

        default:
            io_write_byte_discrete(address, value);
            break;
    }
}

uint16_t io_read_halfword(uint32_t address) {
    switch (address) {
        case REG_KEYINPUT: check_keypad_interrupt(); return ioreg.keyinput.w;
        case REG_KEYCNT: return ioreg.keycnt.w;

        default:
            uint16_t result = io_read_byte_discrete(address);
            result |= io_read_byte_discrete(address + 1) << 8;
            return result;
    }
}

void io_write_halfword(uint32_t address, uint16_t value) {
    switch (address) {
        case REG_FIFO_A_L: audio_fifo_a(value); break;
        case REG_FIFO_A_H: audio_fifo_a(value << 16); break;
        case REG_FIFO_B_L: audio_fifo_b(value); break;
        case REG_FIFO_B_H: audio_fifo_b(value << 16); break;

        case REG_KEYCNT:
            ioreg.keycnt.w = value & 0xc3ff;
            check_keypad_interrupt();
            break;

        default:
            io_write_byte_discrete(address, (uint8_t) value);
            io_write_byte_discrete(address + 1, (uint8_t) (value >> 8));
            break;
    }
}

uint32_t io_read_word(uint32_t address) {
    switch (address) {
        case REG_KEYINPUT: check_keypad_interrupt(); return ioreg.keyinput.w | ioreg.keycnt.w << 16;

        default:
            uint32_t result = io_read_byte_discrete(address);
            result |= io_read_byte_discrete(address + 1) << 8;
            result |= io_read_byte_discrete(address + 2) << 16;
            result |= io_read_byte_discrete(address + 3) << 24;
            return result;
    }
}

void io_write_word(uint32_t address, uint32_t value) {
    switch (address) {
        case REG_FIFO_A_L: audio_fifo_a(value); break;
        case REG_FIFO_B_L: audio_fifo_b(value); break;

        case REG_KEYINPUT:
            ioreg.keycnt.w = (value >> 16) & 0xc3ff;
            check_keypad_interrupt();
            break;

        default:
            io_write_byte_discrete(address, (uint8_t) value);
            io_write_byte_discrete(address + 1, (uint8_t) (value >> 8));
            io_write_byte_discrete(address + 2, (uint8_t) (value >> 16));
            io_write_byte_discrete(address + 3, (uint8_t) (value >> 24));
            break;
    }
}
