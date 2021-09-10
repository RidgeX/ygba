// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#ifndef IO_H
#define IO_H

#include <stdint.h>

#define FIFO_SIZE 8192

typedef union {
    uint16_t w;
    struct {
        uint8_t b0;
        uint8_t b1;
    } b;
} io_union16;

typedef union {
    uint32_t dw;
    struct {
        uint16_t w0;
        uint16_t w1;
    } w;
    struct {
        uint8_t b0;
        uint8_t b1;
        uint8_t b2;
        uint8_t b3;
    } b;
} io_union32;

typedef struct {
    // LCD I/O Registers
    io_union16 dispcnt;
    io_union16 dispstat;
    io_union16 vcount;
    io_union16 bgcnt[4];
    struct {
        io_union16 x;
        io_union16 y;
    } bg_text[4];
    struct {
        io_union16 dx;
        io_union16 dmx;
        io_union16 dy;
        io_union16 dmy;
        io_union32 x;
        io_union32 y;
    } bg_affine[2];
    io_union16 winh[2];
    io_union16 winv[2];
    io_union16 winin;
    io_union16 winout;
    io_union16 mosaic;
    io_union16 bldcnt;
    io_union16 bldalpha;
    io_union16 bldy;

    // Sound Registers
    uint16_t io_sound1cnt_l, io_sound1cnt_h, io_sound1cnt_x;
    uint16_t io_sound2cnt_l, io_sound2cnt_h;
    uint16_t io_sound3cnt_l, io_sound3cnt_h, io_sound3cnt_x;
    uint16_t io_sound4cnt_l, io_sound4cnt_h;
    uint16_t io_soundcnt_l, io_soundcnt_h, io_soundcnt_x;
    uint16_t io_soundbias;
    uint32_t io_wave_ram0;  // 32
    uint32_t io_wave_ram1;  // 32
    uint32_t io_wave_ram2;  // 32
    uint32_t io_wave_ram3;  // 32
    uint8_t fifo_a[FIFO_SIZE];
    uint8_t fifo_b[FIFO_SIZE];
    int fifo_a_r, fifo_b_r;
    int fifo_a_w, fifo_b_w;
    bool fifo_a_refill, fifo_b_refill;
    int fifo_a_ticks, fifo_b_ticks;

    // DMA Transfer Channels
    struct {
        io_union32 sad;
        io_union32 dad;
        io_union32 cnt;
        uint32_t src_addr;
        uint32_t dst_addr;
        uint16_t count;
    } dma[4];
    io_union32 dma_value;

    // Timer Registers
    struct {
        io_union16 counter;
        io_union16 reload;
        io_union16 control;
        uint32_t elapsed;
    } timer[4];

    // Serial Communication (1)
    io_union16 siomulti[4];
    io_union16 siocnt;
    io_union16 siomlt_send;

    // Keypad Input
    io_union16 keyinput;
    io_union16 keycnt;

    // Serial Communication (2)
    io_union16 rcnt;
    io_union16 joycnt;
    io_union32 joy_recv;
    io_union32 joy_trans;
    io_union16 joystat;

    // Interrupt, Waitstate, and Power-Down Control
    io_union16 ie;
    io_union16 irq;
    io_union16 waitcnt;
    io_union16 ime;
    uint8_t postflg;
    uint8_t haltcnt;
} io_registers;

extern io_registers ioreg;

// LCD I/O Registers
#define REG_DISPCNT     0
#define REG_DISPSTAT    4
#define REG_VCOUNT      6
#define REG_BG0CNT      8
#define REG_BG1CNT      0xa
#define REG_BG2CNT      0xc
#define REG_BG3CNT      0xe
#define REG_BG0HOFS     0x10
#define REG_BG0VOFS     0x12
#define REG_BG1HOFS     0x14
#define REG_BG1VOFS     0x16
#define REG_BG2HOFS     0x18
#define REG_BG2VOFS     0x1a
#define REG_BG3HOFS     0x1c
#define REG_BG3VOFS     0x1e
#define REG_BG2PA       0x20
#define REG_BG2PB       0x22
#define REG_BG2PC       0x24
#define REG_BG2PD       0x26
#define REG_BG2X_L      0x28
#define REG_BG2X_H      0x2a
#define REG_BG2Y_L      0x2c
#define REG_BG2Y_H      0x2e
#define REG_BG3PA       0x30
#define REG_BG3PB       0x32
#define REG_BG3PC       0x34
#define REG_BG3PD       0x36
#define REG_BG3X_L      0x38
#define REG_BG3X_H      0x3a
#define REG_BG3Y_L      0x3c
#define REG_BG3Y_H      0x3e
#define REG_WIN0H       0x40
#define REG_WIN1H       0x42
#define REG_WIN0V       0x44
#define REG_WIN1V       0x46
#define REG_WININ       0x48
#define REG_WINOUT      0x4a
#define REG_MOSAIC      0x4c
#define REG_BLDCNT      0x50
#define REG_BLDALPHA    0x52
#define REG_BLDY        0x54

// Sound Registers
#define REG_SOUND1CNT_L 0x60
#define REG_SOUND1CNT_H 0x62
#define REG_SOUND1CNT_X 0x64
#define REG_SOUND2CNT_L 0x68
#define REG_SOUND2CNT_H 0x6c
#define REG_SOUND3CNT_L 0x70
#define REG_SOUND3CNT_H 0x72
#define REG_SOUND3CNT_X 0x74
#define REG_SOUND4CNT_L 0x78
#define REG_SOUND4CNT_H 0x7c
#define REG_SOUNDCNT_L  0x80
#define REG_SOUNDCNT_H  0x82
#define REG_SOUNDCNT_X  0x84
#define REG_SOUNDBIAS   0x88
#define REG_WAVE_RAM0_L 0x90
#define REG_WAVE_RAM0_H 0x92
#define REG_WAVE_RAM1_L 0x94
#define REG_WAVE_RAM1_H 0x96
#define REG_WAVE_RAM2_L 0x98
#define REG_WAVE_RAM2_H 0x9a
#define REG_WAVE_RAM3_L 0x9c
#define REG_WAVE_RAM3_H 0x9e
#define REG_FIFO_A_L    0xa0
#define REG_FIFO_A_H    0xa2
#define REG_FIFO_B_L    0xa4
#define REG_FIFO_B_H    0xa6

// DMA Transfer Channels
#define REG_DMA0SAD_L   0xb0
#define REG_DMA0SAD_H   0xb2
#define REG_DMA0DAD_L   0xb4
#define REG_DMA0DAD_H   0xb6
#define REG_DMA0CNT_L   0xb8
#define REG_DMA0CNT_H   0xba
#define REG_DMA1SAD_L   0xbc
#define REG_DMA1SAD_H   0xbe
#define REG_DMA1DAD_L   0xc0
#define REG_DMA1DAD_H   0xc2
#define REG_DMA1CNT_L   0xc4
#define REG_DMA1CNT_H   0xc6
#define REG_DMA2SAD_L   0xc8
#define REG_DMA2SAD_H   0xca
#define REG_DMA2DAD_L   0xcc
#define REG_DMA2DAD_H   0xce
#define REG_DMA2CNT_L   0xd0
#define REG_DMA2CNT_H   0xd2
#define REG_DMA3SAD_L   0xd4
#define REG_DMA3SAD_H   0xd6
#define REG_DMA3DAD_L   0xd8
#define REG_DMA3DAD_H   0xda
#define REG_DMA3CNT_L   0xdc
#define REG_DMA3CNT_H   0xde

// Timer Registers
#define REG_TM0CNT_L    0x100
#define REG_TM0CNT_H    0x102
#define REG_TM1CNT_L    0x104
#define REG_TM1CNT_H    0x106
#define REG_TM2CNT_L    0x108
#define REG_TM2CNT_H    0x10a
#define REG_TM3CNT_L    0x10c
#define REG_TM3CNT_H    0x10e

// Serial Communication (1)
#define REG_SIODATA32_L 0x120
#define REG_SIOMULTI0   0x120
#define REG_SIODATA32_H 0x122
#define REG_SIOMULTI1   0x122
#define REG_SIOMULTI2   0x124
#define REG_SIOMULTI3   0x126
#define REG_SIOCNT      0x128
#define REG_SIOMLT_SEND 0x12a
#define REG_SIODATA8    0x12a

// Keypad Input
#define REG_KEYINPUT    0x130
#define REG_KEYCNT      0x132

// Serial Communication (2)
#define REG_RCNT        0x134
#define REG_JOYCNT      0x140
#define REG_JOY_RECV_L  0x150
#define REG_JOY_RECV_H  0x152
#define REG_JOY_TRANS_L 0x154
#define REG_JOY_TRANS_H 0x156
#define REG_JOYSTAT     0x158

// Interrupt, Waitstate, and Power-Down Control
#define REG_IE          0x200
#define REG_IF          0x202
#define REG_WAITCNT     0x204
#define REG_IME         0x208
#define REG_POSTFLG     0x300
#define REG_HALTCNT     0x301

uint8_t io_read_byte(uint32_t address);
void io_write_byte(uint32_t address, uint8_t value);
uint16_t io_read_halfword(uint32_t address);
void io_write_halfword(uint32_t address, uint16_t value);
uint32_t io_read_word(uint32_t address);
void io_write_word(uint32_t address, uint32_t value);

#endif  // IO_H
