// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>

#include "io.h"
#include "main.h"

//#define LOG_BAD_MEMORY_ACCESS

io_registers ioreg;

uint8_t io_read_byte(uint32_t address) {
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

        case REG_SOUND1CNT_L + 0: return (uint8_t)(ioreg.io_sound1cnt_l >> 0);
        case REG_SOUND1CNT_L + 1: return (uint8_t)(ioreg.io_sound1cnt_l >> 8);
        case REG_SOUND1CNT_H + 0: return (uint8_t)(ioreg.io_sound1cnt_h >> 0) & 0xc0;
        case REG_SOUND1CNT_H + 1: return (uint8_t)(ioreg.io_sound1cnt_h >> 8);
        case REG_SOUND1CNT_X + 0: return (uint8_t)(ioreg.io_sound1cnt_x >> 0) & 0x00;
        case REG_SOUND1CNT_X + 1: return (uint8_t)(ioreg.io_sound1cnt_x >> 8) & 0x78;
        case 0x66: return 0;
        case 0x67: return 0;
        case REG_SOUND2CNT_L + 0: return (uint8_t)(ioreg.io_sound2cnt_l >> 0) & 0xc0;
        case REG_SOUND2CNT_L + 1: return (uint8_t)(ioreg.io_sound2cnt_l >> 8);
        case 0x6a: return 0;
        case 0x6b: return 0;
        case REG_SOUND2CNT_H + 0: return (uint8_t)(ioreg.io_sound2cnt_h >> 0) & 0x00;
        case REG_SOUND2CNT_H + 1: return (uint8_t)(ioreg.io_sound2cnt_h >> 8) & 0x78;
        case 0x6e: return 0;
        case 0x6f: return 0;
        case REG_SOUND3CNT_L + 0: return (uint8_t)(ioreg.io_sound3cnt_l >> 0);
        case REG_SOUND3CNT_L + 1: return (uint8_t)(ioreg.io_sound3cnt_l >> 8);
        case REG_SOUND3CNT_H + 0: return (uint8_t)(ioreg.io_sound3cnt_h >> 0) & 0x00;
        case REG_SOUND3CNT_H + 1: return (uint8_t)(ioreg.io_sound3cnt_h >> 8);
        case REG_SOUND3CNT_X + 0: return (uint8_t)(ioreg.io_sound3cnt_x >> 0) & 0x00;
        case REG_SOUND3CNT_X + 1: return (uint8_t)(ioreg.io_sound3cnt_x >> 8) & 0x78;
        case 0x76: return 0;
        case 0x77: return 0;
        case REG_SOUND4CNT_L + 0: return (uint8_t)(ioreg.io_sound4cnt_l >> 0) & 0xc0;
        case REG_SOUND4CNT_L + 1: return (uint8_t)(ioreg.io_sound4cnt_l >> 8);
        case 0x7a: return 0;
        case 0x7b: return 0;
        case REG_SOUND4CNT_H + 0: return (uint8_t)(ioreg.io_sound4cnt_h >> 0);
        case REG_SOUND4CNT_H + 1: return (uint8_t)(ioreg.io_sound4cnt_h >> 8) & 0x7f;
        case 0x7e: return 0;
        case 0x7f: return 0;
        case REG_SOUNDCNT_L + 0: return (uint8_t)(ioreg.io_soundcnt_l >> 0);
        case REG_SOUNDCNT_L + 1: return (uint8_t)(ioreg.io_soundcnt_l >> 8);
        case REG_SOUNDCNT_H + 0: return (uint8_t)(ioreg.io_soundcnt_h >> 0);
        case REG_SOUNDCNT_H + 1: return (uint8_t)(ioreg.io_soundcnt_h >> 8) & 0x77;
        case REG_SOUNDCNT_X + 0: return (uint8_t)(ioreg.io_soundcnt_x >> 0) & 0xf0;
        case REG_SOUNDCNT_X + 1: return (uint8_t)(ioreg.io_soundcnt_x >> 8);
        case 0x86: return 0;
        case 0x87: return 0;
        case REG_SOUNDBIAS + 0: return (uint8_t)(ioreg.io_soundbias >> 0);
        case REG_SOUNDBIAS + 1: return (uint8_t)(ioreg.io_soundbias >> 8);
        case 0x8a: return 0;
        case 0x8b: return 0;
        case REG_WAVE_RAM0_L + 0: return (uint8_t)(ioreg.io_wave_ram0 >> 0);
        case REG_WAVE_RAM0_L + 1: return (uint8_t)(ioreg.io_wave_ram0 >> 8);
        case REG_WAVE_RAM0_H + 0: return (uint8_t)(ioreg.io_wave_ram0 >> 16);
        case REG_WAVE_RAM0_H + 1: return (uint8_t)(ioreg.io_wave_ram0 >> 24);
        case REG_WAVE_RAM1_L + 0: return (uint8_t)(ioreg.io_wave_ram1 >> 0);
        case REG_WAVE_RAM1_L + 1: return (uint8_t)(ioreg.io_wave_ram1 >> 8);
        case REG_WAVE_RAM1_H + 0: return (uint8_t)(ioreg.io_wave_ram1 >> 16);
        case REG_WAVE_RAM1_H + 1: return (uint8_t)(ioreg.io_wave_ram1 >> 24);
        case REG_WAVE_RAM2_L + 0: return (uint8_t)(ioreg.io_wave_ram2 >> 0);
        case REG_WAVE_RAM2_L + 1: return (uint8_t)(ioreg.io_wave_ram2 >> 8);
        case REG_WAVE_RAM2_H + 0: return (uint8_t)(ioreg.io_wave_ram2 >> 16);
        case REG_WAVE_RAM2_H + 1: return (uint8_t)(ioreg.io_wave_ram2 >> 24);
        case REG_WAVE_RAM3_L + 0: return (uint8_t)(ioreg.io_wave_ram3 >> 0);
        case REG_WAVE_RAM3_L + 1: return (uint8_t)(ioreg.io_wave_ram3 >> 8);
        case REG_WAVE_RAM3_H + 0: return (uint8_t)(ioreg.io_wave_ram3 >> 16);
        case REG_WAVE_RAM3_H + 1: return (uint8_t)(ioreg.io_wave_ram3 >> 24);

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

        case REG_KEYINPUT + 0: gba_check_keypad_interrupt(); return ioreg.keyinput.b.b0;
        case REG_KEYINPUT + 1: gba_check_keypad_interrupt(); return ioreg.keyinput.b.b1;
        case REG_KEYCNT + 0: return ioreg.keycnt.b.b0;
        case REG_KEYCNT + 1: return ioreg.keycnt.b.b1;

        case REG_RCNT + 0: return ioreg.rcnt.b.b0;
        case REG_RCNT + 1: return ioreg.rcnt.b.b1;
        case REG_JOYCNT + 0: return ioreg.joycnt.b.b0;
        case REG_JOYCNT + 1: return ioreg.joycnt.b.b1;
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

        case REG_IE + 0: return ioreg.ie.b.b0;
        case REG_IE + 1: return ioreg.ie.b.b1;
        case REG_IF + 0: return ioreg.irq.b.b0;
        case REG_IF + 1: return ioreg.irq.b.b1;
        case REG_WAITCNT + 0: return ioreg.waitcnt.b.b0;
        case REG_WAITCNT + 1: return ioreg.waitcnt.b.b1;
        case REG_IME + 0: return ioreg.ime.b.b0;
        case REG_IME + 1: return ioreg.ime.b.b1;
        case REG_POSTFLG: return ioreg.postflg;
        case REG_HALTCNT: return 0;

        default:
#ifdef LOG_BAD_MEMORY_ACCESS
            printf("io_read_byte(0x%08x);\n", address);
#endif
            break;
    }

    return (uint8_t)(open_bus() >> 8 * (address & 3));
}

void io_write_byte(uint32_t address, uint8_t value) {
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
        case REG_BG2PA + 0: ioreg.bg_affine[0].dx.b.b0 = value; break;
        case REG_BG2PA + 1: ioreg.bg_affine[0].dx.b.b1 = value; break;
        case REG_BG2PB + 0: ioreg.bg_affine[0].dmx.b.b0 = value; break;
        case REG_BG2PB + 1: ioreg.bg_affine[0].dmx.b.b1 = value; break;
        case REG_BG2PC + 0: ioreg.bg_affine[0].dy.b.b0 = value; break;
        case REG_BG2PC + 1: ioreg.bg_affine[0].dy.b.b1 = value; break;
        case REG_BG2PD + 0: ioreg.bg_affine[0].dmy.b.b0 = value; break;
        case REG_BG2PD + 1: ioreg.bg_affine[0].dmy.b.b1 = value; break;
        case REG_BG2X_L + 0: ioreg.bg_affine[0].x.b.b0 = value; break;
        case REG_BG2X_L + 1: ioreg.bg_affine[0].x.b.b1 = value; break;
        case REG_BG2X_H + 0: ioreg.bg_affine[0].x.b.b2 = value; break;
        case REG_BG2X_H + 1: ioreg.bg_affine[0].x.b.b3 = value & 0x0f; break;
        case REG_BG2Y_L + 0: ioreg.bg_affine[0].y.b.b0 = value; break;
        case REG_BG2Y_L + 1: ioreg.bg_affine[0].y.b.b1 = value; break;
        case REG_BG2Y_H + 0: ioreg.bg_affine[0].y.b.b2 = value; break;
        case REG_BG2Y_H + 1: ioreg.bg_affine[0].y.b.b3 = value & 0x0f; break;
        case REG_BG3PA + 0: ioreg.bg_affine[1].dx.b.b0 = value; break;
        case REG_BG3PA + 1: ioreg.bg_affine[1].dx.b.b1 = value; break;
        case REG_BG3PB + 0: ioreg.bg_affine[1].dmx.b.b0 = value; break;
        case REG_BG3PB + 1: ioreg.bg_affine[1].dmx.b.b1 = value; break;
        case REG_BG3PC + 0: ioreg.bg_affine[1].dy.b.b0 = value; break;
        case REG_BG3PC + 1: ioreg.bg_affine[1].dy.b.b1 = value; break;
        case REG_BG3PD + 0: ioreg.bg_affine[1].dmy.b.b0 = value; break;
        case REG_BG3PD + 1: ioreg.bg_affine[1].dmy.b.b1 = value; break;
        case REG_BG3X_L + 0: ioreg.bg_affine[1].x.b.b0 = value; break;
        case REG_BG3X_L + 1: ioreg.bg_affine[1].x.b.b1 = value; break;
        case REG_BG3X_H + 0: ioreg.bg_affine[1].x.b.b2 = value; break;
        case REG_BG3X_H + 1: ioreg.bg_affine[1].x.b.b3 = value & 0x0f; break;
        case REG_BG3Y_L + 0: ioreg.bg_affine[1].y.b.b0 = value; break;
        case REG_BG3Y_L + 1: ioreg.bg_affine[1].y.b.b1 = value; break;
        case REG_BG3Y_H + 0: ioreg.bg_affine[1].y.b.b2 = value; break;
        case REG_BG3Y_H + 1: ioreg.bg_affine[1].y.b.b3 = value & 0x0f; break;
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
        case REG_BLDY + 1: ioreg.bldy.b.b1 = value & 0x00; break;

        case REG_SOUND1CNT_L + 0: ioreg.io_sound1cnt_l = (ioreg.io_sound1cnt_l & 0xff00) | ((value << 0) & 0x007f); break;
        case REG_SOUND1CNT_L + 1: ioreg.io_sound1cnt_l = (ioreg.io_sound1cnt_l & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_SOUND1CNT_H + 0: ioreg.io_sound1cnt_h = (ioreg.io_sound1cnt_h & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUND1CNT_H + 1: ioreg.io_sound1cnt_h = (ioreg.io_sound1cnt_h & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_SOUND1CNT_X + 0: ioreg.io_sound1cnt_x = (ioreg.io_sound1cnt_x & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUND1CNT_X + 1: ioreg.io_sound1cnt_x = (ioreg.io_sound1cnt_x & 0x00ff) | ((value << 8) & 0xc700); break;
        case REG_SOUND2CNT_L + 0: ioreg.io_sound2cnt_l = (ioreg.io_sound2cnt_l & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUND2CNT_L + 1: ioreg.io_sound2cnt_l = (ioreg.io_sound2cnt_l & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_SOUND2CNT_H + 0: ioreg.io_sound2cnt_h = (ioreg.io_sound2cnt_h & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUND2CNT_H + 1: ioreg.io_sound2cnt_h = (ioreg.io_sound2cnt_h & 0x00ff) | ((value << 8) & 0xc700); break;
        case REG_SOUND3CNT_L + 0: ioreg.io_sound3cnt_l = (ioreg.io_sound3cnt_l & 0xff00) | ((value << 0) & 0x00e0); break;
        case REG_SOUND3CNT_L + 1: ioreg.io_sound3cnt_l = (ioreg.io_sound3cnt_l & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_SOUND3CNT_H + 0: ioreg.io_sound3cnt_h = (ioreg.io_sound3cnt_h & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUND3CNT_H + 1: ioreg.io_sound3cnt_h = (ioreg.io_sound3cnt_h & 0x00ff) | ((value << 8) & 0xe000); break;
        case REG_SOUND3CNT_X + 0: ioreg.io_sound3cnt_x = (ioreg.io_sound3cnt_x & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUND3CNT_X + 1: ioreg.io_sound3cnt_x = (ioreg.io_sound3cnt_x & 0x00ff) | ((value << 8) & 0xc700); break;
        case REG_SOUND4CNT_L + 0: ioreg.io_sound4cnt_l = (ioreg.io_sound4cnt_l & 0xff00) | ((value << 0) & 0x003f); break;
        case REG_SOUND4CNT_L + 1: ioreg.io_sound4cnt_l = (ioreg.io_sound4cnt_l & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_SOUND4CNT_H + 0: ioreg.io_sound4cnt_h = (ioreg.io_sound4cnt_h & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUND4CNT_H + 1: ioreg.io_sound4cnt_h = (ioreg.io_sound4cnt_h & 0x00ff) | ((value << 8) & 0xc000); break;
        case REG_SOUNDCNT_L + 0: ioreg.io_soundcnt_l = (ioreg.io_soundcnt_l & 0xff00) | ((value << 0) & 0x0077); break;
        case REG_SOUNDCNT_L + 1: ioreg.io_soundcnt_l = (ioreg.io_soundcnt_l & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_SOUNDCNT_H + 0: ioreg.io_soundcnt_h = (ioreg.io_soundcnt_h & 0xff00) | ((value << 0) & 0x000f); break;
        case REG_SOUNDCNT_H + 1: ioreg.io_soundcnt_h = (ioreg.io_soundcnt_h & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_SOUNDCNT_X + 0: ioreg.io_soundcnt_x = (ioreg.io_soundcnt_x & 0xff00) | ((value << 0) & 0x008f); break;
        case REG_SOUNDCNT_X + 1: ioreg.io_soundcnt_x = (ioreg.io_soundcnt_x & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_SOUNDBIAS + 0: ioreg.io_soundbias = (ioreg.io_soundbias & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUNDBIAS + 1: ioreg.io_soundbias = (ioreg.io_soundbias & 0x00ff) | ((value << 8) & 0xc300); break;
        case REG_WAVE_RAM0_L + 0: ioreg.io_wave_ram0 = (ioreg.io_wave_ram0 & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_WAVE_RAM0_L + 1: ioreg.io_wave_ram0 = (ioreg.io_wave_ram0 & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_WAVE_RAM0_H + 0: ioreg.io_wave_ram0 = (ioreg.io_wave_ram0 & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_WAVE_RAM0_H + 1: ioreg.io_wave_ram0 = (ioreg.io_wave_ram0 & 0x00ffffff) | ((value << 24) & 0xff000000); break;
        case REG_WAVE_RAM1_L + 0: ioreg.io_wave_ram1 = (ioreg.io_wave_ram1 & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_WAVE_RAM1_L + 1: ioreg.io_wave_ram1 = (ioreg.io_wave_ram1 & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_WAVE_RAM1_H + 0: ioreg.io_wave_ram1 = (ioreg.io_wave_ram1 & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_WAVE_RAM1_H + 1: ioreg.io_wave_ram1 = (ioreg.io_wave_ram1 & 0x00ffffff) | ((value << 24) & 0xff000000); break;
        case REG_WAVE_RAM2_L + 0: ioreg.io_wave_ram2 = (ioreg.io_wave_ram2 & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_WAVE_RAM2_L + 1: ioreg.io_wave_ram2 = (ioreg.io_wave_ram2 & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_WAVE_RAM2_H + 0: ioreg.io_wave_ram2 = (ioreg.io_wave_ram2 & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_WAVE_RAM2_H + 1: ioreg.io_wave_ram2 = (ioreg.io_wave_ram2 & 0x00ffffff) | ((value << 24) & 0xff000000); break;
        case REG_WAVE_RAM3_L + 0: ioreg.io_wave_ram3 = (ioreg.io_wave_ram3 & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_WAVE_RAM3_L + 1: ioreg.io_wave_ram3 = (ioreg.io_wave_ram3 & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_WAVE_RAM3_H + 0: ioreg.io_wave_ram3 = (ioreg.io_wave_ram3 & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_WAVE_RAM3_H + 1: ioreg.io_wave_ram3 = (ioreg.io_wave_ram3 & 0x00ffffff) | ((value << 24) & 0xff000000); break;
        case REG_FIFO_A_L + 0: gba_audio_fifo_a(value); break;
        case REG_FIFO_A_L + 1: gba_audio_fifo_a(value << 8); break;
        case REG_FIFO_A_H + 0: gba_audio_fifo_a(value << 16); break;
        case REG_FIFO_A_H + 1: gba_audio_fifo_a(value << 24); break;
        case REG_FIFO_B_L + 0: gba_audio_fifo_b(value); break;
        case REG_FIFO_B_L + 1: gba_audio_fifo_b(value << 8); break;
        case REG_FIFO_B_H + 0: gba_audio_fifo_b(value << 16); break;
        case REG_FIFO_B_H + 1: gba_audio_fifo_b(value << 24); break;

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
        case REG_DMA0CNT_H + 1: old_value = ioreg.dma[0].cnt.b.b3; ioreg.dma[0].cnt.b.b3 = value & 0xf7; if (!(old_value & 0x80) && (value & 0x80)) { gba_dma_reset(0); gba_dma_update(DMA_NOW); } break;
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
        case REG_DMA1CNT_H + 1: old_value = ioreg.dma[1].cnt.b.b3; ioreg.dma[1].cnt.b.b3 = value & 0xf7; if (!(old_value & 0x80) && (value & 0x80)) { gba_dma_reset(1); gba_dma_update(DMA_NOW); } break;
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
        case REG_DMA2CNT_H + 1: old_value = ioreg.dma[2].cnt.b.b3; ioreg.dma[2].cnt.b.b3 = value & 0xf7; if (!(old_value & 0x80) && (value & 0x80)) { gba_dma_reset(2); gba_dma_update(DMA_NOW); } break;
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
        case REG_DMA3CNT_H + 1: old_value = ioreg.dma[3].cnt.b.b3; ioreg.dma[3].cnt.b.b3 = value; if (!(old_value & 0x80) && (value & 0x80)) { gba_dma_reset(3); gba_dma_update(DMA_NOW); } break;

        case REG_TM0CNT_L + 0: ioreg.timer[0].reload.b.b0 = value; break;
        case REG_TM0CNT_L + 1: ioreg.timer[0].reload.b.b1 = value; break;
        case REG_TM0CNT_H + 0: old_value = ioreg.timer[0].control.b.b0; ioreg.timer[0].control.b.b0 = value & 0xc7; if (!(old_value & 0x80) && (value & 0x80)) { gba_timer_reset(0); } break;
        case REG_TM0CNT_H + 1: ioreg.timer[0].control.b.b1 = value & 0x00; break;
        case REG_TM1CNT_L + 0: ioreg.timer[1].reload.b.b0 = value; break;
        case REG_TM1CNT_L + 1: ioreg.timer[1].reload.b.b1 = value; break;
        case REG_TM1CNT_H + 0: old_value = ioreg.timer[1].control.b.b0; ioreg.timer[1].control.b.b0 = value & 0xc7; if (!(old_value & 0x80) && (value & 0x80)) { gba_timer_reset(1); } break;
        case REG_TM1CNT_H + 1: ioreg.timer[1].control.b.b1 = value & 0x00; break;
        case REG_TM2CNT_L + 0: ioreg.timer[2].reload.b.b0 = value; break;
        case REG_TM2CNT_L + 1: ioreg.timer[2].reload.b.b1 = value; break;
        case REG_TM2CNT_H + 0: old_value = ioreg.timer[2].control.b.b0; ioreg.timer[2].control.b.b0 = value & 0xc7; if (!(old_value & 0x80) && (value & 0x80)) { gba_timer_reset(2); } break;
        case REG_TM2CNT_H + 1: ioreg.timer[2].control.b.b1 = value & 0x00; break;
        case REG_TM3CNT_L + 0: ioreg.timer[3].reload.b.b0 = value; break;
        case REG_TM3CNT_L + 1: ioreg.timer[3].reload.b.b1 = value; break;
        case REG_TM3CNT_H + 0: old_value = ioreg.timer[3].control.b.b0; ioreg.timer[3].control.b.b0 = value & 0xc7; if (!(old_value & 0x80) && (value & 0x80)) { gba_timer_reset(3); } break;
        case REG_TM3CNT_H + 1: ioreg.timer[3].control.b.b1 = value & 0x00; break;

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

        case REG_KEYCNT + 0: ioreg.keycnt.b.b0 = value; gba_check_keypad_interrupt(); break;
        case REG_KEYCNT + 1: ioreg.keycnt.b.b1 = value & 0xc3; gba_check_keypad_interrupt(); break;

        case REG_RCNT + 0: ioreg.rcnt.b.b0 = value; break;
        case REG_RCNT + 1: ioreg.rcnt.b.b1 = value & 0xc1; break;
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
        case REG_IME + 1: ioreg.ime.b.b1 = value & 0x00; break;
        case REG_POSTFLG: ioreg.postflg = value & 0x01; break;
        case REG_HALTCNT: ioreg.haltcnt = value & 0x80; halted = true; break;

        default:
#ifdef LOG_BAD_MEMORY_ACCESS
            printf("io_write_byte(0x%08x, 0x%02x);\n", address, value);
#endif
            break;
    }
}

uint16_t io_read_halfword(uint32_t address) {
    switch (address) {
        case REG_DISPCNT: return ioreg.dispcnt.w;
        case REG_DISPSTAT: return ioreg.dispstat.w;
        case REG_VCOUNT: return ioreg.vcount.w;
        case REG_BG0CNT: return ioreg.bgcnt[0].w;
        case REG_BG1CNT: return ioreg.bgcnt[1].w;
        case REG_BG2CNT: return ioreg.bgcnt[2].w;
        case REG_BG3CNT: return ioreg.bgcnt[3].w;
        case REG_WININ: return ioreg.winin.w;
        case REG_WINOUT: return ioreg.winout.w;
        case REG_BLDCNT: return ioreg.bldcnt.w;
        case REG_BLDALPHA: return ioreg.bldalpha.w;

        case REG_SOUND1CNT_L: return ioreg.io_sound1cnt_l;
        case REG_SOUND1CNT_H: return ioreg.io_sound1cnt_h & 0xffc0;
        case REG_SOUND1CNT_X: return ioreg.io_sound1cnt_x & 0x7800;
        case 0x66: return 0;
        case REG_SOUND2CNT_L: return ioreg.io_sound2cnt_l & 0xffc0;
        case 0x6a: return 0;
        case REG_SOUND2CNT_H: return ioreg.io_sound2cnt_h & 0x7800;
        case 0x6e: return 0;
        case REG_SOUND3CNT_L: return ioreg.io_sound3cnt_l;
        case REG_SOUND3CNT_H: return ioreg.io_sound3cnt_h & 0xff00;
        case REG_SOUND3CNT_X: return ioreg.io_sound3cnt_x & 0x7800;
        case 0x76: return 0;
        case REG_SOUND4CNT_L: return ioreg.io_sound4cnt_l & 0xffc0;
        case 0x7a: return 0;
        case REG_SOUND4CNT_H: return ioreg.io_sound4cnt_h & 0x7fff;
        case 0x7e: return 0;
        case REG_SOUNDCNT_L: return ioreg.io_soundcnt_l;
        case REG_SOUNDCNT_H: return ioreg.io_soundcnt_h & 0x77ff;
        case REG_SOUNDCNT_X: return ioreg.io_soundcnt_x & 0xfff0;
        case 0x86: return 0;
        case REG_SOUNDBIAS: return ioreg.io_soundbias;
        case 0x8a: return 0;
        case REG_WAVE_RAM0_L: return (uint16_t)(ioreg.io_wave_ram0 >> 0);
        case REG_WAVE_RAM0_H: return (uint16_t)(ioreg.io_wave_ram0 >> 16);
        case REG_WAVE_RAM1_L: return (uint16_t)(ioreg.io_wave_ram1 >> 0);
        case REG_WAVE_RAM1_H: return (uint16_t)(ioreg.io_wave_ram1 >> 16);
        case REG_WAVE_RAM2_L: return (uint16_t)(ioreg.io_wave_ram2 >> 0);
        case REG_WAVE_RAM2_H: return (uint16_t)(ioreg.io_wave_ram2 >> 16);
        case REG_WAVE_RAM3_L: return (uint16_t)(ioreg.io_wave_ram3 >> 0);
        case REG_WAVE_RAM3_H: return (uint16_t)(ioreg.io_wave_ram3 >> 16);

        case REG_DMA0CNT_L: return 0;
        case REG_DMA0CNT_H: return ioreg.dma[0].cnt.w.w1;
        case REG_DMA1CNT_L: return 0;
        case REG_DMA1CNT_H: return ioreg.dma[1].cnt.w.w1;
        case REG_DMA2CNT_L: return 0;
        case REG_DMA2CNT_H: return ioreg.dma[2].cnt.w.w1;
        case REG_DMA3CNT_L: return 0;
        case REG_DMA3CNT_H: return ioreg.dma[3].cnt.w.w1;

        case REG_TM0CNT_L: return ioreg.timer[0].counter.w;
        case REG_TM0CNT_H: return ioreg.timer[0].control.w;
        case REG_TM1CNT_L: return ioreg.timer[1].counter.w;
        case REG_TM1CNT_H: return ioreg.timer[1].control.w;
        case REG_TM2CNT_L: return ioreg.timer[2].counter.w;
        case REG_TM2CNT_H: return ioreg.timer[2].control.w;
        case REG_TM3CNT_L: return ioreg.timer[3].counter.w;
        case REG_TM3CNT_H: return ioreg.timer[3].control.w;

        case REG_SIOMULTI0: return ioreg.siomulti[0].w;
        case REG_SIOMULTI1: return ioreg.siomulti[1].w;
        case REG_SIOMULTI2: return ioreg.siomulti[2].w;
        case REG_SIOMULTI3: return ioreg.siomulti[3].w;
        case REG_SIOCNT: return ioreg.siocnt.w;
        case REG_SIOMLT_SEND: return ioreg.siomlt_send.w;

        case REG_KEYINPUT: gba_check_keypad_interrupt(); return ioreg.keyinput.w;
        case REG_KEYCNT: return ioreg.keycnt.w;

        case REG_RCNT: return ioreg.rcnt.w;
        case REG_JOYCNT: return ioreg.joycnt.w;
        case REG_JOY_RECV_L: return ioreg.joy_recv.w.w0;
        case REG_JOY_RECV_H: return ioreg.joy_recv.w.w1;
        case REG_JOY_TRANS_L: return ioreg.joy_trans.w.w0;
        case REG_JOY_TRANS_H: return ioreg.joy_trans.w.w1;
        case REG_JOYSTAT: return ioreg.joystat.w;

        case REG_IE: return ioreg.ie.w;
        case REG_IF: return ioreg.irq.w;
        case REG_WAITCNT: return ioreg.waitcnt.w;
        case REG_IME: return ioreg.ime.w;
        case REG_POSTFLG: return ioreg.postflg;

        default:
#ifdef LOG_BAD_MEMORY_ACCESS
            printf("io_read_halfword(0x%08x);\n", address);
#endif
            break;
    }

    return (uint16_t)(open_bus() >> 8 * (address & 2));
}

void io_write_halfword(uint32_t address, uint16_t value) {
    uint16_t old_value;

    switch (address) {
        case REG_DISPCNT: ioreg.dispcnt.w = (ioreg.dispcnt.w & 0x0008) | (value & 0xfff7); break;
        case REG_DISPSTAT: ioreg.dispstat.w = (ioreg.dispstat.w & 0x0007) | (value & 0xff38); break;
        case REG_BG0CNT: ioreg.bgcnt[0].w = value & 0xdfff; break;
        case REG_BG1CNT: ioreg.bgcnt[1].w = value & 0xdfff; break;
        case REG_BG2CNT: ioreg.bgcnt[2].w = value; break;
        case REG_BG3CNT: ioreg.bgcnt[3].w = value; break;
        case REG_BG0HOFS: ioreg.bg_text[0].x.w = value & 0x01ff; break;
        case REG_BG0VOFS: ioreg.bg_text[0].y.w = value & 0x01ff; break;
        case REG_BG1HOFS: ioreg.bg_text[1].x.w = value & 0x01ff; break;
        case REG_BG1VOFS: ioreg.bg_text[1].y.w = value & 0x01ff; break;
        case REG_BG2HOFS: ioreg.bg_text[2].x.w = value & 0x01ff; break;
        case REG_BG2VOFS: ioreg.bg_text[2].y.w = value & 0x01ff; break;
        case REG_BG3HOFS: ioreg.bg_text[3].x.w = value & 0x01ff; break;
        case REG_BG3VOFS: ioreg.bg_text[3].y.w = value & 0x01ff; break;
        case REG_BG2PA: ioreg.bg_affine[0].dx.w = value; break;
        case REG_BG2PB: ioreg.bg_affine[0].dmx.w = value; break;
        case REG_BG2PC: ioreg.bg_affine[0].dy.w = value; break;
        case REG_BG2PD: ioreg.bg_affine[0].dmy.w = value; break;
        case REG_BG2X_L: ioreg.bg_affine[0].x.w.w0 = value; break;
        case REG_BG2X_H: ioreg.bg_affine[0].x.w.w1 = value & 0x0fff; break;
        case REG_BG2Y_L: ioreg.bg_affine[0].y.w.w0 = value; break;
        case REG_BG2Y_H: ioreg.bg_affine[0].y.w.w1 = value & 0x0fff; break;
        case REG_BG3PA: ioreg.bg_affine[1].dx.w = value; break;
        case REG_BG3PB: ioreg.bg_affine[1].dmx.w = value; break;
        case REG_BG3PC: ioreg.bg_affine[1].dy.w = value; break;
        case REG_BG3PD: ioreg.bg_affine[1].dmy.w = value; break;
        case REG_BG3X_L: ioreg.bg_affine[1].x.w.w0 = value; break;
        case REG_BG3X_H: ioreg.bg_affine[1].x.w.w1 = value & 0x0fff; break;
        case REG_BG3Y_L: ioreg.bg_affine[1].y.w.w0 = value; break;
        case REG_BG3Y_H: ioreg.bg_affine[1].y.w.w1 = value & 0x0fff; break;
        case REG_WIN0H: ioreg.winh[0].w = value; break;
        case REG_WIN1H: ioreg.winh[1].w = value; break;
        case REG_WIN0V: ioreg.winv[0].w = value; break;
        case REG_WIN1V: ioreg.winv[1].w = value; break;
        case REG_WININ: ioreg.winin.w = value & 0x3f3f; break;
        case REG_WINOUT: ioreg.winout.w = value & 0x3f3f; break;
        case REG_MOSAIC: ioreg.mosaic.w = value; break;
        case REG_BLDCNT: ioreg.bldcnt.w = value & 0x3fff; break;
        case REG_BLDALPHA: ioreg.bldalpha.w = value & 0x1f1f; break;
        case REG_BLDY: ioreg.bldy.w = value & 0x001f; break;

        case REG_SOUND1CNT_L: ioreg.io_sound1cnt_l = value & 0x007f; break;
        case REG_SOUND1CNT_H: ioreg.io_sound1cnt_h = value & 0xffff; break;
        case REG_SOUND1CNT_X: ioreg.io_sound1cnt_x = value & 0xc7ff; break;
        case REG_SOUND2CNT_L: ioreg.io_sound2cnt_l = value & 0xffff; break;
        case REG_SOUND2CNT_H: ioreg.io_sound2cnt_h = value & 0xc7ff; break;
        case REG_SOUND3CNT_L: ioreg.io_sound3cnt_l = value & 0x00e0; break;
        case REG_SOUND3CNT_H: ioreg.io_sound3cnt_h = value & 0xe0ff; break;
        case REG_SOUND3CNT_X: ioreg.io_sound3cnt_x = value & 0xc7ff; break;
        case REG_SOUND4CNT_L: ioreg.io_sound4cnt_l = value & 0xff3f; break;
        case REG_SOUND4CNT_H: ioreg.io_sound4cnt_h = value & 0xc0ff; break;
        case REG_SOUNDCNT_L: ioreg.io_soundcnt_l = value & 0xff77; break;
        case REG_SOUNDCNT_H: ioreg.io_soundcnt_h = value & 0xff0f; break;
        case REG_SOUNDCNT_X: ioreg.io_soundcnt_x = value & 0x008f; break;
        case REG_SOUNDBIAS: ioreg.io_soundbias = value & 0xc3ff; break;
        case REG_WAVE_RAM0_L: ioreg.io_wave_ram0 = (ioreg.io_wave_ram0 & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_WAVE_RAM0_H: ioreg.io_wave_ram0 = (ioreg.io_wave_ram0 & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_WAVE_RAM1_L: ioreg.io_wave_ram1 = (ioreg.io_wave_ram1 & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_WAVE_RAM1_H: ioreg.io_wave_ram1 = (ioreg.io_wave_ram1 & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_WAVE_RAM2_L: ioreg.io_wave_ram2 = (ioreg.io_wave_ram2 & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_WAVE_RAM2_H: ioreg.io_wave_ram2 = (ioreg.io_wave_ram2 & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_WAVE_RAM3_L: ioreg.io_wave_ram3 = (ioreg.io_wave_ram3 & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_WAVE_RAM3_H: ioreg.io_wave_ram3 = (ioreg.io_wave_ram3 & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_FIFO_A_L: gba_audio_fifo_a(value); break;
        case REG_FIFO_A_H: gba_audio_fifo_a(value << 16); break;
        case REG_FIFO_B_L: gba_audio_fifo_b(value); break;
        case REG_FIFO_B_H: gba_audio_fifo_b(value << 16); break;

        case REG_DMA0SAD_L: ioreg.dma[0].sad.w.w0 = value; break;
        case REG_DMA0SAD_H: ioreg.dma[0].sad.w.w1 = value; break;
        case REG_DMA0DAD_L: ioreg.dma[0].dad.w.w0 = value; break;
        case REG_DMA0DAD_H: ioreg.dma[0].dad.w.w1 = value; break;
        case REG_DMA0CNT_L: ioreg.dma[0].cnt.w.w0 = value & 0x3fff; break;
        case REG_DMA0CNT_H: old_value = ioreg.dma[0].cnt.w.w1; ioreg.dma[0].cnt.w.w1 = value & 0xf7e0; if (!(old_value & 0x8000) && (value & 0x8000)) { gba_dma_reset(0); gba_dma_update(DMA_NOW); } break;
        case REG_DMA1SAD_L: ioreg.dma[1].sad.w.w0 = value; break;
        case REG_DMA1SAD_H: ioreg.dma[1].sad.w.w1 = value; break;
        case REG_DMA1DAD_L: ioreg.dma[1].dad.w.w0 = value; break;
        case REG_DMA1DAD_H: ioreg.dma[1].dad.w.w1 = value; break;
        case REG_DMA1CNT_L: ioreg.dma[1].cnt.w.w0 = value & 0x3fff; break;
        case REG_DMA1CNT_H: old_value = ioreg.dma[1].cnt.w.w1; ioreg.dma[1].cnt.w.w1 = value & 0xf7e0; if (!(old_value & 0x8000) && (value & 0x8000)) { gba_dma_reset(1); gba_dma_update(DMA_NOW); } break;
        case REG_DMA2SAD_L: ioreg.dma[2].sad.w.w0 = value; break;
        case REG_DMA2SAD_H: ioreg.dma[2].sad.w.w1 = value; break;
        case REG_DMA2DAD_L: ioreg.dma[2].dad.w.w0 = value; break;
        case REG_DMA2DAD_H: ioreg.dma[2].dad.w.w1 = value; break;
        case REG_DMA2CNT_L: ioreg.dma[2].cnt.w.w0 = value & 0x3fff; break;
        case REG_DMA2CNT_H: old_value = ioreg.dma[2].cnt.w.w1; ioreg.dma[2].cnt.w.w1 = value & 0xf7e0; if (!(old_value & 0x8000) && (value & 0x8000)) { gba_dma_reset(2); gba_dma_update(DMA_NOW); } break;
        case REG_DMA3SAD_L: ioreg.dma[3].sad.w.w0 = value; break;
        case REG_DMA3SAD_H: ioreg.dma[3].sad.w.w1 = value; break;
        case REG_DMA3DAD_L: ioreg.dma[3].dad.w.w0 = value; break;
        case REG_DMA3DAD_H: ioreg.dma[3].dad.w.w1 = value; break;
        case REG_DMA3CNT_L: ioreg.dma[3].cnt.w.w0 = value; break;
        case REG_DMA3CNT_H: old_value = ioreg.dma[3].cnt.w.w1; ioreg.dma[3].cnt.w.w1 = value & 0xffe0; if (!(old_value & 0x8000) && (value & 0x8000)) { gba_dma_reset(3); gba_dma_update(DMA_NOW); } break;

        case REG_TM0CNT_L: ioreg.timer[0].reload.w = value; break;
        case REG_TM0CNT_H: old_value = ioreg.timer[0].control.w; ioreg.timer[0].control.w = value & 0x00c7; if (!(old_value & 0x80) && (value & 0x80)) { gba_timer_reset(0); } break;
        case REG_TM1CNT_L: ioreg.timer[1].reload.w = value; break;
        case REG_TM1CNT_H: old_value = ioreg.timer[1].control.w; ioreg.timer[1].control.w = value & 0x00c7; if (!(old_value & 0x80) && (value & 0x80)) { gba_timer_reset(1); } break;
        case REG_TM2CNT_L: ioreg.timer[2].reload.w = value; break;
        case REG_TM2CNT_H: old_value = ioreg.timer[2].control.w; ioreg.timer[2].control.w = value & 0x00c7; if (!(old_value & 0x80) && (value & 0x80)) { gba_timer_reset(2); } break;
        case REG_TM3CNT_L: ioreg.timer[3].reload.w = value; break;
        case REG_TM3CNT_H: old_value = ioreg.timer[3].control.w; ioreg.timer[3].control.w = value & 0x00c7; if (!(old_value & 0x80) && (value & 0x80)) { gba_timer_reset(3); } break;

        //case REG_SIOMULTI0:
        //case REG_SIOMULTI1:
        //case REG_SIOMULTI2:
        //case REG_SIOMULTI3:
        //case REG_SIOCNT:
        //case REG_SIOMLT_SEND:

        case REG_KEYCNT: ioreg.keycnt.w = value & 0xc3ff; gba_check_keypad_interrupt(); break;

        case REG_RCNT: ioreg.rcnt.w = value & 0xc1ff; break;
        //case REG_JOYCNT:
        //case REG_JOY_RECV_L:
        //case REG_JOY_RECV_H:
        //case REG_JOY_TRANS_L:
        //case REG_JOY_TRANS_H:
        //case REG_JOYSTAT:

        case REG_IE: ioreg.ie.w = value & 0x3fff; break;
        case REG_IF: ioreg.irq.w &= ~value; break;
        case REG_WAITCNT: ioreg.waitcnt.w = (ioreg.waitcnt.w & 0x8000) | (value & 0x5fff); break;
        case REG_IME: ioreg.ime.w = value & 0x0001; break;
        case REG_POSTFLG: ioreg.postflg = value & 0x01; ioreg.haltcnt = (value >> 8) & 0x80; halted = true; break;

        default:
#ifdef LOG_BAD_MEMORY_ACCESS
            printf("io_write_halfword(0x%08x, 0x%04x);\n", address, value);
#endif
            break;
    }
}

uint32_t io_read_word(uint32_t address) {
    switch (address) {
        case REG_DISPCNT: return ioreg.dispcnt.w | (open_bus() & 0xffff0000);
        case REG_DISPSTAT: return ioreg.dispstat.w | ioreg.vcount.w << 16;
        case REG_BG0CNT: return ioreg.bgcnt[0].w | ioreg.bgcnt[1].w << 16;
        case REG_BG2CNT: return ioreg.bgcnt[2].w | ioreg.bgcnt[3].w << 16;
        case REG_WININ: return ioreg.winin.w | ioreg.winout.w << 16;
        case REG_BLDCNT: return ioreg.bldcnt.w | ioreg.bldalpha.w << 16;
        case REG_BLDY: return ioreg.bldy.w | (open_bus() & 0xffff0000);

        case REG_SOUND1CNT_L: return ioreg.io_sound1cnt_l | (ioreg.io_sound1cnt_h & 0xffc0) << 16;
        case REG_SOUND1CNT_X: return ioreg.io_sound1cnt_x & 0x7800;
        case REG_SOUND2CNT_L: return ioreg.io_sound2cnt_l & 0xffc0;
        case REG_SOUND2CNT_H: return ioreg.io_sound2cnt_h & 0x7800;
        case REG_SOUND3CNT_L: return ioreg.io_sound3cnt_l | (ioreg.io_sound3cnt_h & 0xff00) << 16;
        case REG_SOUND3CNT_X: return ioreg.io_sound3cnt_x & 0x7800;
        case REG_SOUND4CNT_L: return ioreg.io_sound4cnt_l & 0xffc0;
        case REG_SOUND4CNT_H: return ioreg.io_sound4cnt_h & 0x7fff;
        case REG_SOUNDCNT_L: return ioreg.io_soundcnt_l | (ioreg.io_soundcnt_h & 0x77ff) << 16;
        case REG_SOUNDCNT_X: return ioreg.io_soundcnt_x & 0xfff0;
        case REG_SOUNDBIAS: return ioreg.io_soundbias;
        case REG_WAVE_RAM0_L: return ioreg.io_wave_ram0;
        case REG_WAVE_RAM1_L: return ioreg.io_wave_ram1;
        case REG_WAVE_RAM2_L: return ioreg.io_wave_ram2;
        case REG_WAVE_RAM3_L: return ioreg.io_wave_ram3;

        case REG_DMA0CNT_L: return ioreg.dma[0].cnt.w.w1 << 16;
        case REG_DMA1CNT_L: return ioreg.dma[1].cnt.w.w1 << 16;
        case REG_DMA2CNT_L: return ioreg.dma[2].cnt.w.w1 << 16;
        case REG_DMA3CNT_L: return ioreg.dma[3].cnt.w.w1 << 16;

        case REG_TM0CNT_L: return ioreg.timer[0].counter.w | ioreg.timer[0].control.w << 16;
        case REG_TM1CNT_L: return ioreg.timer[1].counter.w | ioreg.timer[1].control.w << 16;
        case REG_TM2CNT_L: return ioreg.timer[2].counter.w | ioreg.timer[2].control.w << 16;
        case REG_TM3CNT_L: return ioreg.timer[3].counter.w | ioreg.timer[3].control.w << 16;

        case REG_SIOMULTI0: return ioreg.siomulti[0].w | ioreg.siomulti[1].w << 16;
        case REG_SIOMULTI2: return ioreg.siomulti[2].w | ioreg.siomulti[3].w << 16;
        case REG_SIOCNT: return ioreg.siocnt.w | ioreg.siomlt_send.w << 16;

        case REG_KEYINPUT: gba_check_keypad_interrupt(); return ioreg.keyinput.w | ioreg.keycnt.w << 16;

        case REG_RCNT: return ioreg.rcnt.w;
        case REG_JOYCNT: return ioreg.joycnt.w;
        case REG_JOY_RECV_L: return ioreg.joy_recv.dw;
        case REG_JOY_TRANS_L: return ioreg.joy_trans.dw;
        case REG_JOYSTAT: return ioreg.joystat.w;

        case REG_IE: return ioreg.ie.w | ioreg.irq.w << 16;
        case REG_WAITCNT: return ioreg.waitcnt.w;
        case REG_IME: return ioreg.ime.w;
        case REG_POSTFLG: return ioreg.postflg;

        default:
#ifdef LOG_BAD_MEMORY_ACCESS
            printf("io_read_word(0x%08x);\n", address);
#endif
            break;
    }

    return open_bus();
}

void io_write_word(uint32_t address, uint32_t value) {
    uint32_t old_value;

    switch (address) {
        case REG_DISPCNT: ioreg.dispcnt.w = (ioreg.dispcnt.w & 0x0008) | (value & 0xfff7); break;
        case REG_DISPSTAT: ioreg.dispstat.w = (ioreg.dispstat.w & 0x0007) | (value & 0xff38); break;
        case REG_BG0CNT: ioreg.bgcnt[0].w = value & 0xdfff; ioreg.bgcnt[1].w = (value >> 16) & 0xdfff; break;
        case REG_BG2CNT: ioreg.bgcnt[2].w = value & 0xffff; ioreg.bgcnt[3].w = (value >> 16) & 0xffff; break;
        case REG_BG0HOFS: ioreg.bg_text[0].x.w = value & 0x01ff; ioreg.bg_text[0].y.w = (value >> 16) & 0x01ff; break;
        case REG_BG1HOFS: ioreg.bg_text[1].x.w = value & 0x01ff; ioreg.bg_text[1].y.w = (value >> 16) & 0x01ff; break;
        case REG_BG2HOFS: ioreg.bg_text[2].x.w = value & 0x01ff; ioreg.bg_text[2].y.w = (value >> 16) & 0x01ff; break;
        case REG_BG3HOFS: ioreg.bg_text[3].x.w = value & 0x01ff; ioreg.bg_text[3].y.w = (value >> 16) & 0x01ff; break;
        case REG_BG2PA: ioreg.bg_affine[0].dx.w = value & 0xffff; ioreg.bg_affine[0].dmx.w = (value >> 16) & 0xffff; break;
        case REG_BG2PC: ioreg.bg_affine[0].dy.w = value & 0xffff; ioreg.bg_affine[0].dmy.w = (value >> 16) & 0xffff; break;
        case REG_BG2X_L: ioreg.bg_affine[0].x.dw = value & 0x0fffffff; break;
        case REG_BG2Y_L: ioreg.bg_affine[0].y.dw = value & 0x0fffffff; break;
        case REG_BG3PA: ioreg.bg_affine[1].dx.w = value & 0xffff; ioreg.bg_affine[1].dmx.w = (value >> 16) & 0xffff; break;
        case REG_BG3PC: ioreg.bg_affine[1].dy.w = value & 0xffff; ioreg.bg_affine[1].dmy.w = (value >> 16) & 0xffff; break;
        case REG_BG3X_L: ioreg.bg_affine[1].x.dw = value & 0x0fffffff; break;
        case REG_BG3Y_L: ioreg.bg_affine[1].y.dw = value & 0x0fffffff; break;
        case REG_WIN0H: ioreg.winh[0].w = value & 0xffff; ioreg.winh[1].w = (value >> 16) & 0xffff; break;
        case REG_WIN0V: ioreg.winv[0].w = value & 0xffff; ioreg.winv[1].w = (value >> 16) & 0xffff; break;
        case REG_WININ: ioreg.winin.w = value & 0x3f3f; ioreg.winout.w = (value >> 16) & 0x3f3f; break;
        case REG_MOSAIC: ioreg.mosaic.w = value & 0xffff; break;
        case REG_BLDCNT: ioreg.bldcnt.w = value & 0x3fff; ioreg.bldalpha.w = (value >> 16) & 0x1f1f; break;
        case REG_BLDY: ioreg.bldy.w = value & 0x001f; break;

        case REG_SOUND1CNT_L: ioreg.io_sound1cnt_l = value & 0x007f; ioreg.io_sound1cnt_h = (value >> 16) & 0xffff; break;
        case REG_SOUND1CNT_X: ioreg.io_sound1cnt_x = value & 0xc7ff; break;
        case REG_SOUND2CNT_L: ioreg.io_sound2cnt_l = value & 0xffff; break;
        case REG_SOUND2CNT_H: ioreg.io_sound2cnt_h = value & 0xc7ff; break;
        case REG_SOUND3CNT_L: ioreg.io_sound3cnt_l = value & 0x00e0; ioreg.io_sound3cnt_h = (value >> 16) & 0xe0ff; break;
        case REG_SOUND3CNT_X: ioreg.io_sound3cnt_x = value & 0xc7ff; break;
        case REG_SOUND4CNT_L: ioreg.io_sound4cnt_l = value & 0xff3f; break;
        case REG_SOUND4CNT_H: ioreg.io_sound4cnt_h = value & 0xc0ff; break;
        case REG_SOUNDCNT_L: ioreg.io_soundcnt_l = value & 0xff77; ioreg.io_soundcnt_h = (value >> 16) & 0xff0f; break;
        case REG_SOUNDCNT_X: ioreg.io_soundcnt_x = value & 0x008f; break;
        case REG_SOUNDBIAS: ioreg.io_soundbias = value & 0xc3ff; break;
        case REG_WAVE_RAM0_L: ioreg.io_wave_ram0 = value; break;
        case REG_WAVE_RAM1_L: ioreg.io_wave_ram1 = value; break;
        case REG_WAVE_RAM2_L: ioreg.io_wave_ram2 = value; break;
        case REG_WAVE_RAM3_L: ioreg.io_wave_ram3 = value; break;
        case REG_FIFO_A_L: gba_audio_fifo_a(value); break;
        case REG_FIFO_B_L: gba_audio_fifo_b(value); break;

        case REG_DMA0SAD_L: ioreg.dma[0].sad.dw = value; break;
        case REG_DMA0DAD_L: ioreg.dma[0].dad.dw = value; break;
        case REG_DMA0CNT_L: old_value = ioreg.dma[0].cnt.dw; ioreg.dma[0].cnt.dw = value & 0xf7e03fff; if (!(old_value & 0x80000000) && (value & 0x80000000)) { gba_dma_reset(0); gba_dma_update(DMA_NOW); } break;
        case REG_DMA1SAD_L: ioreg.dma[1].sad.dw = value; break;
        case REG_DMA1DAD_L: ioreg.dma[1].dad.dw = value; break;
        case REG_DMA1CNT_L: old_value = ioreg.dma[1].cnt.dw; ioreg.dma[1].cnt.dw = value & 0xf7e03fff; if (!(old_value & 0x80000000) && (value & 0x80000000)) { gba_dma_reset(1); gba_dma_update(DMA_NOW); } break;
        case REG_DMA2SAD_L: ioreg.dma[2].sad.dw = value; break;
        case REG_DMA2DAD_L: ioreg.dma[2].dad.dw = value; break;
        case REG_DMA2CNT_L: old_value = ioreg.dma[2].cnt.dw; ioreg.dma[2].cnt.dw = value & 0xf7e03fff; if (!(old_value & 0x80000000) && (value & 0x80000000)) { gba_dma_reset(2); gba_dma_update(DMA_NOW); } break;
        case REG_DMA3SAD_L: ioreg.dma[3].sad.dw = value; break;
        case REG_DMA3DAD_L: ioreg.dma[3].dad.dw = value; break;
        case REG_DMA3CNT_L: old_value = ioreg.dma[3].cnt.dw; ioreg.dma[3].cnt.dw = value & 0xffe0ffff; if (!(old_value & 0x80000000) && (value & 0x80000000)) { gba_dma_reset(3); gba_dma_update(DMA_NOW); } break;

        case REG_TM0CNT_L: ioreg.timer[0].reload.w = value & 0xffff; old_value = ioreg.timer[0].control.w; ioreg.timer[0].control.w = (value >> 16) & 0x00c7; if (!(old_value & 0x80) && ((value >> 16) & 0x80)) { gba_timer_reset(0); } break;
        case REG_TM1CNT_L: ioreg.timer[1].reload.w = value & 0xffff; old_value = ioreg.timer[1].control.w; ioreg.timer[1].control.w = (value >> 16) & 0x00c7; if (!(old_value & 0x80) && ((value >> 16) & 0x80)) { gba_timer_reset(1); } break;
        case REG_TM2CNT_L: ioreg.timer[2].reload.w = value & 0xffff; old_value = ioreg.timer[2].control.w; ioreg.timer[2].control.w = (value >> 16) & 0x00c7; if (!(old_value & 0x80) && ((value >> 16) & 0x80)) { gba_timer_reset(2); } break;
        case REG_TM3CNT_L: ioreg.timer[3].reload.w = value & 0xffff; old_value = ioreg.timer[3].control.w; ioreg.timer[3].control.w = (value >> 16) & 0x00c7; if (!(old_value & 0x80) && ((value >> 16) & 0x80)) { gba_timer_reset(3); } break;

        //case REG_SIOMULTI0:
        //case REG_SIOMULTI2:
        //case REG_SIOCNT:

        case REG_KEYINPUT: ioreg.keycnt.w = (value >> 16) & 0xc3ff; gba_check_keypad_interrupt(); break;

        case REG_RCNT: ioreg.rcnt.w = value & 0xc1ff; break;
        //case REG_JOYCNT:
        //case REG_JOY_RECV_L:
        //case REG_JOY_TRANS_L:
        //case REG_JOYSTAT:

        case REG_IE: ioreg.ie.w = value & 0x3fff; ioreg.irq.w &= ~(uint16_t)(value >> 16); break;
        case REG_WAITCNT: ioreg.waitcnt.w = (ioreg.waitcnt.w & 0x8000) | (value & 0x5fff); break;
        case REG_IME: ioreg.ime.w = value & 0x0001; break;
        case REG_POSTFLG: ioreg.postflg = value & 0x01; ioreg.haltcnt = (value >> 8) & 0x80; halted = true; break;

        default:
#ifdef LOG_BAD_MEMORY_ACCESS
            printf("io_write_word(0x%08x, 0x%08x);\n", address, value);
#endif
            break;
    }
}
