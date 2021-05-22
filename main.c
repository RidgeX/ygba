// gcc -std=c99 -Wall -Wextra -Wpedantic -O2 -o gba gba.c -lmingw32 -lSDL2main -lSDL2 && gba

#include <SDL2/SDL.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "algorithms.h"
#include "cpu.h"

//#define LOG_BAD_MEMORY_ACCESS

bool single_step = false;
uint64_t start_logging_at = 0;
//uint64_t end_logging_at = 200000;
int ppu_cycles = 0;
int timer_cycles = 0;
bool halted = false;
uint32_t last_bios_access = 0xe4;
bool skip_bios = true;
bool has_eeprom = false;
bool has_flash = false;
//bool has_rtc = false;
bool has_sram = false;

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 160
#define RENDER_SCALE  3
#define RENDER_WIDTH  (SCREEN_WIDTH * RENDER_SCALE)
#define RENDER_HEIGHT (SCREEN_HEIGHT * RENDER_SCALE)

#define NUM_KEYS 10
bool keys[NUM_KEYS];

uint8_t system_rom[0x4000];
uint8_t cpu_ewram[0x40000];
uint8_t cpu_iwram[0x8000];
uint8_t palette_ram[0x400];
uint8_t video_ram[0x18000];
uint8_t object_ram[0x400];
uint8_t game_rom[0x2000000];
uint32_t game_rom_size;
uint32_t game_rom_mask;
uint8_t backup_flash[0x20000];
uint8_t backup_sram[0x10000];

#define DCNT_GB         (1 << 3)
#define DCNT_PAGE       (1 << 4)
#define DCNT_OAM_HBL    (1 << 5)
#define DCNT_OBJ_1D     (1 << 6)
#define DCNT_BLANK      (1 << 7)
#define DCNT_BG0        (1 << 8)
#define DCNT_BG1        (1 << 9)
#define DCNT_BG2        (1 << 10)
#define DCNT_BG3        (1 << 11)
#define DCNT_OBJ        (1 << 12)
#define DCNT_WIN0       (1 << 13)
#define DCNT_WIN1       (1 << 14)
#define DCNT_WINOBJ     (1 << 15)

#define DSTAT_IN_VBL    (1 << 0)
#define DSTAT_IN_HBL    (1 << 1)
#define DSTAT_IN_VCT    (1 << 2)
#define DSTAT_VBL_IRQ   (1 << 3)
#define DSTAT_HBL_IRQ   (1 << 4)
#define DSTAT_VCT_IRQ   (1 << 5)

#define DMA_INC         0
#define DMA_DEC         1
#define DMA_FIXED       2
#define DMA_RELOAD      3
#define DMA_REPEAT      (1 << 25)
#define DMA_32          (1 << 26)
#define DMA_DRQ         (1 << 27)
#define DMA_NOW         0
#define DMA_AT_VBLANK   1
#define DMA_AT_HBLANK   2
#define DMA_AT_REFRESH  3
#define DMA_IRQ         (1 << 30)
#define DMA_ENABLE      (1 << 31)

#define INT_VBLANK      (1 << 0)
#define INT_HBLANK      (1 << 1)
#define INT_VCOUNT      (1 << 2)
#define INT_TIMER0      (1 << 3)
#define INT_TIMER1      (1 << 4)
#define INT_TIMER2      (1 << 5)
#define INT_TIMER3      (1 << 6)
#define INT_COM         (1 << 7)
#define INT_DMA0        (1 << 8)
#define INT_DMA1        (1 << 9)
#define INT_DMA2        (1 << 10)
#define INT_DMA3        (1 << 11)
#define INT_BUTTON      (1 << 12)
#define INT_CART        (1 << 13)

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
#define REG_TM0CNT_L    0x100
#define REG_TM0CNT_H    0x102
#define REG_TM1CNT_L    0x104
#define REG_TM1CNT_H    0x106
#define REG_TM2CNT_L    0x108
#define REG_TM2CNT_H    0x10a
#define REG_TM3CNT_L    0x10c
#define REG_TM3CNT_H    0x10e

#define REG_SIODATA32   0x120
#define REG_SIOCNT      0x128
#define REG_KEYINPUT    0x130
#define REG_KEYCNT      0x132
#define REG_RCNT        0x134
#define REG_IE          0x200
#define REG_IF          0x202
#define REG_WAITCNT     0x204
#define REG_IME         0x208
#define REG_POSTFLG     0x300
#define REG_HALTCNT     0x301

uint16_t io_dispcnt = 0x80;
uint16_t io_dispstat;
uint16_t io_vcount;
uint16_t io_bg0cnt;
uint16_t io_bg1cnt;
uint16_t io_bg2cnt;
uint16_t io_bg3cnt;
uint16_t io_bg0hofs;
uint16_t io_bg0vofs;
uint16_t io_bg1hofs;
uint16_t io_bg1vofs;
uint16_t io_bg2hofs;
uint16_t io_bg2vofs;
uint16_t io_bg3hofs;
uint16_t io_bg3vofs;
uint16_t io_bg2pa = 0x100;
uint16_t io_bg2pb;
uint16_t io_bg2pc;
uint16_t io_bg2pd = 0x100;
uint32_t io_bg2x;  // 32
uint32_t io_bg2y;  // 32
uint16_t io_bg3pa = 0x100;
uint16_t io_bg3pb;
uint16_t io_bg3pc;
uint16_t io_bg3pd = 0x100;
uint32_t io_bg3x;  // 32
uint32_t io_bg3y;  // 32
uint16_t io_win0h;
uint16_t io_win1h;
uint16_t io_win0v;
uint16_t io_win1v;
uint16_t io_winin;
uint16_t io_winout;
uint16_t io_mosaic;
uint16_t io_bldcnt;
uint16_t io_bldalpha;
uint16_t io_bldy;
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
uint32_t io_fifo_a;  // 32
uint32_t io_fifo_b;  // 32
uint32_t io_dma0sad;  // 32
uint32_t io_dma0dad;  // 32
uint16_t io_dma0cnt_l, io_dma0cnt_h;
uint32_t io_dma1sad;  // 32
uint32_t io_dma1dad;  // 32
uint16_t io_dma1cnt_l, io_dma1cnt_h;
uint32_t io_dma2sad;  // 32
uint32_t io_dma2dad;  // 32
uint16_t io_dma2cnt_l, io_dma2cnt_h;
uint32_t io_dma3sad;  // 32
uint32_t io_dma3dad;  // 32
uint16_t io_dma3cnt_l, io_dma3cnt_h;

bool fifo_a_refill, fifo_b_refill;
uint16_t timer_0_counter, timer_0_reload, timer_0_control;
uint16_t timer_1_counter, timer_1_reload, timer_1_control;
uint16_t timer_2_counter, timer_2_reload, timer_2_control;
uint16_t timer_3_counter, timer_3_reload, timer_3_control;

uint16_t io_keyinput;
uint16_t io_keycnt;
uint16_t io_rcnt;
uint16_t io_ie;
uint16_t io_if;
uint16_t io_waitcnt;
uint16_t io_ime;
uint8_t io_haltcnt;

uint8_t io_read_byte(uint32_t address) {
    switch (address) {
        case REG_DISPCNT + 0: return (uint8_t)(io_dispcnt >> 0);
        case REG_DISPCNT + 1: return (uint8_t)(io_dispcnt >> 8);
        case 0x2: return 0xad;
        case 0x3: return 0xde;
        case REG_DISPSTAT + 0: return (uint8_t)(io_dispstat >> 0);
        case REG_DISPSTAT + 1: return (uint8_t)(io_dispstat >> 8);
        case REG_VCOUNT + 0: return (uint8_t)(io_vcount >> 0);
        case REG_VCOUNT + 1: return (uint8_t)(io_vcount >> 8);
        case REG_BG0CNT + 0: return (uint8_t)(io_bg0cnt >> 0);
        case REG_BG0CNT + 1: return (uint8_t)(io_bg0cnt >> 8);
        case REG_BG1CNT + 0: return (uint8_t)(io_bg1cnt >> 0);
        case REG_BG1CNT + 1: return (uint8_t)(io_bg1cnt >> 8);
        case REG_BG2CNT + 0: return (uint8_t)(io_bg2cnt >> 0);
        case REG_BG2CNT + 1: return (uint8_t)(io_bg2cnt >> 8);
        case REG_BG3CNT + 0: return (uint8_t)(io_bg3cnt >> 0);
        case REG_BG3CNT + 1: return (uint8_t)(io_bg3cnt >> 8);
        case REG_BG0HOFS + 0: return 0xad;
        case REG_BG0HOFS + 1: return 0xde;
        case REG_BG0VOFS + 0: return 0xad;
        case REG_BG0VOFS + 1: return 0xde;
        case REG_BG1HOFS + 0: return 0xad;
        case REG_BG1HOFS + 1: return 0xde;
        case REG_BG1VOFS + 0: return 0xad;
        case REG_BG1VOFS + 1: return 0xde;
        case REG_BG2HOFS + 0: return 0xad;
        case REG_BG2HOFS + 1: return 0xde;
        case REG_BG2VOFS + 0: return 0xad;
        case REG_BG2VOFS + 1: return 0xde;
        case REG_BG3HOFS + 0: return 0xad;
        case REG_BG3HOFS + 1: return 0xde;
        case REG_BG3VOFS + 0: return 0xad;
        case REG_BG3VOFS + 1: return 0xde;
        case REG_BG2PA + 0: return 0xad;
        case REG_BG2PA + 1: return 0xde;
        case REG_BG2PB + 0: return 0xad;
        case REG_BG2PB + 1: return 0xde;
        case REG_BG2PC + 0: return 0xad;
        case REG_BG2PC + 1: return 0xde;
        case REG_BG2PD + 0: return 0xad;
        case REG_BG2PD + 1: return 0xde;
        case REG_BG2X_L + 0: return 0xad;
        case REG_BG2X_L + 1: return 0xde;
        case REG_BG2X_H + 0: return 0xad;
        case REG_BG2X_H + 1: return 0xde;
        case REG_BG2Y_L + 0: return 0xad;
        case REG_BG2Y_L + 1: return 0xde;
        case REG_BG2Y_H + 0: return 0xad;
        case REG_BG2Y_H + 1: return 0xde;
        case REG_BG3PA + 0: return 0xad;
        case REG_BG3PA + 1: return 0xde;
        case REG_BG3PB + 0: return 0xad;
        case REG_BG3PB + 1: return 0xde;
        case REG_BG3PC + 0: return 0xad;
        case REG_BG3PC + 1: return 0xde;
        case REG_BG3PD + 0: return 0xad;
        case REG_BG3PD + 1: return 0xde;
        case REG_BG3X_L + 0: return 0xad;
        case REG_BG3X_L + 1: return 0xde;
        case REG_BG3X_H + 0: return 0xad;
        case REG_BG3X_H + 1: return 0xde;
        case REG_BG3Y_L + 0: return 0xad;
        case REG_BG3Y_L + 1: return 0xde;
        case REG_BG3Y_H + 0: return 0xad;
        case REG_BG3Y_H + 1: return 0xde;
        case REG_WIN0H + 0: return 0xad;
        case REG_WIN0H + 1: return 0xde;
        case REG_WIN1H + 0: return 0xad;
        case REG_WIN1H + 1: return 0xde;
        case REG_WIN0V + 0: return 0xad;
        case REG_WIN0V + 1: return 0xde;
        case REG_WIN1V + 0: return 0xad;
        case REG_WIN1V + 1: return 0xde;
        case REG_WININ + 0: return (uint8_t)(io_winin >> 0);
        case REG_WININ + 1: return (uint8_t)(io_winin >> 8);
        case REG_WINOUT + 0: return (uint8_t)(io_winout >> 0);
        case REG_WINOUT + 1: return (uint8_t)(io_winout >> 8);
        case REG_MOSAIC + 0: return 0xad;
        case REG_MOSAIC + 1: return 0xde;
        case 0x4e: return 0xad;
        case 0x4f: return 0xde;
        case REG_BLDCNT + 0: return (uint8_t)(io_bldcnt >> 0);
        case REG_BLDCNT + 1: return (uint8_t)(io_bldcnt >> 8);
        case REG_BLDALPHA + 0: return (uint8_t)(io_bldalpha >> 0);
        case REG_BLDALPHA + 1: return (uint8_t)(io_bldalpha >> 8);
        case REG_BLDY + 0: return 0xad;
        case REG_BLDY + 1: return 0xde;
        case 0x56: return 0xad;
        case 0x57: return 0xde;
        case 0x58: return 0xad;
        case 0x59: return 0xde;
        case 0x5a: return 0xad;
        case 0x5b: return 0xde;
        case 0x5c: return 0xad;
        case 0x5d: return 0xde;
        case 0x5e: return 0xad;
        case 0x5f: return 0xde;
        case REG_SOUND1CNT_L + 0: return (uint8_t)(io_sound1cnt_l >> 0);
        case REG_SOUND1CNT_L + 1: return (uint8_t)(io_sound1cnt_l >> 8);
        case REG_SOUND1CNT_H + 0: return (uint8_t)(io_sound1cnt_h >> 0) & 0xc0;
        case REG_SOUND1CNT_H + 1: return (uint8_t)(io_sound1cnt_h >> 8);
        case REG_SOUND1CNT_X + 0: return (uint8_t)(io_sound1cnt_x >> 0) & 0x00;
        case REG_SOUND1CNT_X + 1: return (uint8_t)(io_sound1cnt_x >> 8) & 0x78;
        case 0x66: return 0;
        case 0x67: return 0;
        case REG_SOUND2CNT_L + 0: return (uint8_t)(io_sound2cnt_l >> 0) & 0xc0;
        case REG_SOUND2CNT_L + 1: return (uint8_t)(io_sound2cnt_l >> 8);
        case 0x6a: return 0;
        case 0x6b: return 0;
        case REG_SOUND2CNT_H + 0: return (uint8_t)(io_sound2cnt_h >> 0) & 0x00;
        case REG_SOUND2CNT_H + 1: return (uint8_t)(io_sound2cnt_h >> 8) & 0x78;
        case 0x6e: return 0;
        case 0x6f: return 0;
        case REG_SOUND3CNT_L + 0: return (uint8_t)(io_sound3cnt_l >> 0);
        case REG_SOUND3CNT_L + 1: return (uint8_t)(io_sound3cnt_l >> 8);
        case REG_SOUND3CNT_H + 0: return (uint8_t)(io_sound3cnt_h >> 0) & 0x00;
        case REG_SOUND3CNT_H + 1: return (uint8_t)(io_sound3cnt_h >> 8);
        case REG_SOUND3CNT_X + 0: return (uint8_t)(io_sound3cnt_x >> 0) & 0x00;
        case REG_SOUND3CNT_X + 1: return (uint8_t)(io_sound3cnt_x >> 8) & 0x78;
        case 0x76: return 0;
        case 0x77: return 0;
        case REG_SOUND4CNT_L + 0: return (uint8_t)(io_sound4cnt_l >> 0) & 0xc0;
        case REG_SOUND4CNT_L + 1: return (uint8_t)(io_sound4cnt_l >> 8);
        case 0x7a: return 0;
        case 0x7b: return 0;
        case REG_SOUND4CNT_H + 0: return (uint8_t)(io_sound4cnt_h >> 0);
        case REG_SOUND4CNT_H + 1: return (uint8_t)(io_sound4cnt_h >> 8) & 0x7f;
        case 0x7e: return 0;
        case 0x7f: return 0;
        case REG_SOUNDCNT_L + 0: return (uint8_t)(io_soundcnt_l >> 0);
        case REG_SOUNDCNT_L + 1: return (uint8_t)(io_soundcnt_l >> 8);
        case REG_SOUNDCNT_H + 0: return (uint8_t)(io_soundcnt_h >> 0);
        case REG_SOUNDCNT_H + 1: return (uint8_t)(io_soundcnt_h >> 8) & 0x77;
        case REG_SOUNDCNT_X + 0: return (uint8_t)(io_soundcnt_x >> 0) & 0xf0;
        case REG_SOUNDCNT_X + 1: return (uint8_t)(io_soundcnt_x >> 8);
        case 0x86: return 0;
        case 0x87: return 0;
        case REG_SOUNDBIAS + 0: return (uint8_t)(io_soundbias >> 0);
        case REG_SOUNDBIAS + 1: return (uint8_t)(io_soundbias >> 8);
        case 0x8a: return 0;
        case 0x8b: return 0;
        case 0x8c: return 0xad;
        case 0x8d: return 0xde;
        case 0x8e: return 0xad;
        case 0x8f: return 0xde;
        case REG_WAVE_RAM0_L + 0: return (uint8_t)(io_wave_ram0 >> 0);
        case REG_WAVE_RAM0_L + 1: return (uint8_t)(io_wave_ram0 >> 8);
        case REG_WAVE_RAM0_H + 0: return (uint8_t)(io_wave_ram0 >> 16);
        case REG_WAVE_RAM0_H + 1: return (uint8_t)(io_wave_ram0 >> 24);
        case REG_WAVE_RAM1_L + 0: return (uint8_t)(io_wave_ram1 >> 0);
        case REG_WAVE_RAM1_L + 1: return (uint8_t)(io_wave_ram1 >> 8);
        case REG_WAVE_RAM1_H + 0: return (uint8_t)(io_wave_ram1 >> 16);
        case REG_WAVE_RAM1_H + 1: return (uint8_t)(io_wave_ram1 >> 24);
        case REG_WAVE_RAM2_L + 0: return (uint8_t)(io_wave_ram2 >> 0);
        case REG_WAVE_RAM2_L + 1: return (uint8_t)(io_wave_ram2 >> 8);
        case REG_WAVE_RAM2_H + 0: return (uint8_t)(io_wave_ram2 >> 16);
        case REG_WAVE_RAM2_H + 1: return (uint8_t)(io_wave_ram2 >> 24);
        case REG_WAVE_RAM3_L + 0: return (uint8_t)(io_wave_ram3 >> 0);
        case REG_WAVE_RAM3_L + 1: return (uint8_t)(io_wave_ram3 >> 8);
        case REG_WAVE_RAM3_H + 0: return (uint8_t)(io_wave_ram3 >> 16);
        case REG_WAVE_RAM3_H + 1: return (uint8_t)(io_wave_ram3 >> 24);
        case REG_FIFO_A_L + 0: return 0xad;
        case REG_FIFO_A_L + 1: return 0xde;
        case REG_FIFO_A_H + 0: return 0xad;
        case REG_FIFO_A_H + 1: return 0xde;
        case REG_FIFO_B_L + 0: return 0xad;
        case REG_FIFO_B_L + 1: return 0xde;
        case REG_FIFO_B_H + 0: return 0xad;
        case REG_FIFO_B_H + 1: return 0xde;
        case 0xa8: return 0xad;
        case 0xa9: return 0xde;
        case 0xaa: return 0xad;
        case 0xab: return 0xde;
        case 0xac: return 0xad;
        case 0xad: return 0xde;
        case 0xae: return 0xad;
        case 0xaf: return 0xde;
        case REG_DMA0SAD_L + 0: return 0xad;
        case REG_DMA0SAD_L + 1: return 0xde;
        case REG_DMA0SAD_H + 0: return 0xad;
        case REG_DMA0SAD_H + 1: return 0xde;
        case REG_DMA0DAD_L + 0: return 0xad;
        case REG_DMA0DAD_L + 1: return 0xde;
        case REG_DMA0DAD_H + 0: return 0xad;
        case REG_DMA0DAD_H + 1: return 0xde;
        case REG_DMA0CNT_L + 0: return 0;
        case REG_DMA0CNT_L + 1: return 0;
        case REG_DMA0CNT_H + 0: return (uint8_t)(io_dma0cnt_h >> 0);
        case REG_DMA0CNT_H + 1: return (uint8_t)(io_dma0cnt_h >> 8);
        case REG_DMA1SAD_L + 0: return 0xad;
        case REG_DMA1SAD_L + 1: return 0xde;
        case REG_DMA1SAD_H + 0: return 0xad;
        case REG_DMA1SAD_H + 1: return 0xde;
        case REG_DMA1DAD_L + 0: return 0xad;
        case REG_DMA1DAD_L + 1: return 0xde;
        case REG_DMA1DAD_H + 0: return 0xad;
        case REG_DMA1DAD_H + 1: return 0xde;
        case REG_DMA1CNT_L + 0: return 0;
        case REG_DMA1CNT_L + 1: return 0;
        case REG_DMA1CNT_H + 0: return (uint8_t)(io_dma1cnt_h >> 0);
        case REG_DMA1CNT_H + 1: return (uint8_t)(io_dma1cnt_h >> 8);
        case REG_DMA2SAD_L + 0: return 0xad;
        case REG_DMA2SAD_L + 1: return 0xde;
        case REG_DMA2SAD_H + 0: return 0xad;
        case REG_DMA2SAD_H + 1: return 0xde;
        case REG_DMA2DAD_L + 0: return 0xad;
        case REG_DMA2DAD_L + 1: return 0xde;
        case REG_DMA2DAD_H + 0: return 0xad;
        case REG_DMA2DAD_H + 1: return 0xde;
        case REG_DMA2CNT_L + 0: return 0;
        case REG_DMA2CNT_L + 1: return 0;
        case REG_DMA2CNT_H + 0: return (uint8_t)(io_dma2cnt_h >> 0);
        case REG_DMA2CNT_H + 1: return (uint8_t)(io_dma2cnt_h >> 8);
        case REG_DMA3SAD_L + 0: return 0xad;
        case REG_DMA3SAD_L + 1: return 0xde;
        case REG_DMA3SAD_H + 0: return 0xad;
        case REG_DMA3SAD_H + 1: return 0xde;
        case REG_DMA3DAD_L + 0: return 0xad;
        case REG_DMA3DAD_L + 1: return 0xde;
        case REG_DMA3DAD_H + 0: return 0xad;
        case REG_DMA3DAD_H + 1: return 0xde;
        case REG_DMA3CNT_L + 0: return 0;
        case REG_DMA3CNT_L + 1: return 0;
        case REG_DMA3CNT_H + 0: return (uint8_t)(io_dma3cnt_h >> 0);
        case REG_DMA3CNT_H + 1: return (uint8_t)(io_dma3cnt_h >> 8);
        case 0xe0: return 0xad;
        case 0xe1: return 0xde;
        case 0xe2: return 0xad;
        case 0xe3: return 0xde;
        case 0xe4: return 0xad;
        case 0xe5: return 0xde;
        case 0xe6: return 0xad;
        case 0xe7: return 0xde;
        case 0xe8: return 0xad;
        case 0xe9: return 0xde;
        case 0xea: return 0xad;
        case 0xeb: return 0xde;
        case 0xec: return 0xad;
        case 0xed: return 0xde;
        case 0xee: return 0xad;
        case 0xef: return 0xde;
        case 0xf0: return 0xad;
        case 0xf1: return 0xde;
        case 0xf2: return 0xad;
        case 0xf3: return 0xde;
        case 0xf4: return 0xad;
        case 0xf5: return 0xde;
        case 0xf6: return 0xad;
        case 0xf7: return 0xde;
        case 0xf8: return 0xad;
        case 0xf9: return 0xde;
        case 0xfa: return 0xad;
        case 0xfb: return 0xde;
        case 0xfc: return 0xad;
        case 0xfd: return 0xde;
        case 0xfe: return 0xad;
        case 0xff: return 0xde;
        case 0x100c: return 0xad;
        case 0x100d: return 0xde;
        case 0x100e: return 0xad;
        case 0x100f: return 0xde;

        case REG_TM0CNT_L + 0: return (uint8_t) timer_0_counter;
        case REG_TM0CNT_L + 1: return (uint8_t)(timer_0_counter >> 8);
        case REG_TM1CNT_L + 0: return (uint8_t) timer_1_counter;
        case REG_TM1CNT_L + 1: return (uint8_t)(timer_1_counter >> 8);
        case REG_TM2CNT_L + 0: return (uint8_t) timer_2_counter;
        case REG_TM2CNT_L + 1: return (uint8_t)(timer_2_counter >> 8);
        case REG_TM3CNT_L + 0: return (uint8_t) timer_3_counter;
        case REG_TM3CNT_L + 1: return (uint8_t)(timer_3_counter >> 8);

        default:
#ifdef LOG_BAD_MEMORY_ACCESS
            printf("io_read_byte(0x%08x);\n", address);
#endif
            return 0;
    }
}

void io_write_byte(uint32_t address, uint8_t value) {
    switch (address) {
        case REG_DISPCNT + 0: io_dispcnt = (io_dispcnt & 0xff08) | ((value << 0) & 0x00f7); break;
        case REG_DISPCNT + 1: io_dispcnt = (io_dispcnt & 0x00ff) | ((value << 8) & 0xff00); break;
        case 0x2: break;
        case 0x3: break;
        case REG_DISPSTAT + 0: io_dispstat = (io_dispstat & 0xff07) | ((value << 0) & 0x0038); break;
        case REG_DISPSTAT + 1: io_dispstat = (io_dispstat & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_VCOUNT + 0: break;
        case REG_VCOUNT + 1: break;
        case REG_BG0CNT + 0: io_bg0cnt = (io_bg0cnt & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG0CNT + 1: io_bg0cnt = (io_bg0cnt & 0x00ff) | ((value << 8) & 0xdf00); break;
        case REG_BG1CNT + 0: io_bg1cnt = (io_bg1cnt & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG1CNT + 1: io_bg1cnt = (io_bg1cnt & 0x00ff) | ((value << 8) & 0xdf00); break;
        case REG_BG2CNT + 0: io_bg2cnt = (io_bg2cnt & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG2CNT + 1: io_bg2cnt = (io_bg2cnt & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_BG3CNT + 0: io_bg3cnt = (io_bg3cnt & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG3CNT + 1: io_bg3cnt = (io_bg3cnt & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_BG0HOFS + 0: io_bg0hofs = (io_bg0hofs & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG0HOFS + 1: io_bg0hofs = (io_bg0hofs & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_BG0VOFS + 0: io_bg0vofs = (io_bg0vofs & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG0VOFS + 1: io_bg0vofs = (io_bg0vofs & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_BG1HOFS + 0: io_bg1hofs = (io_bg1hofs & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG1HOFS + 1: io_bg1hofs = (io_bg1hofs & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_BG1VOFS + 0: io_bg1vofs = (io_bg1vofs & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG1VOFS + 1: io_bg1vofs = (io_bg1vofs & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_BG2HOFS + 0: io_bg2hofs = (io_bg2hofs & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG2HOFS + 1: io_bg2hofs = (io_bg2hofs & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_BG2VOFS + 0: io_bg2vofs = (io_bg2vofs & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG2VOFS + 1: io_bg2vofs = (io_bg2vofs & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_BG3HOFS + 0: io_bg3hofs = (io_bg3hofs & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG3HOFS + 1: io_bg3hofs = (io_bg3hofs & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_BG3VOFS + 0: io_bg3vofs = (io_bg3vofs & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG3VOFS + 1: io_bg3vofs = (io_bg3vofs & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_BG2PA + 0: io_bg2pa = (io_bg2pa & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG2PA + 1: io_bg2pa = (io_bg2pa & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_BG2PB + 0: io_bg2pb = (io_bg2pb & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG2PB + 1: io_bg2pb = (io_bg2pb & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_BG2PC + 0: io_bg2pc = (io_bg2pc & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG2PC + 1: io_bg2pc = (io_bg2pc & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_BG2PD + 0: io_bg2pd = (io_bg2pd & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG2PD + 1: io_bg2pd = (io_bg2pd & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_BG2X_L + 0: io_bg2x = (io_bg2x & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_BG2X_L + 1: io_bg2x = (io_bg2x & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_BG2X_H + 0: io_bg2x = (io_bg2x & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_BG2X_H + 1: io_bg2x = (io_bg2x & 0x00ffffff) | ((value << 24) & 0xff000000); break;
        case REG_BG2Y_L + 0: io_bg2y = (io_bg2y & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_BG2Y_L + 1: io_bg2y = (io_bg2y & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_BG2Y_H + 0: io_bg2y = (io_bg2y & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_BG2Y_H + 1: io_bg2y = (io_bg2y & 0x00ffffff) | ((value << 24) & 0xff000000); break;
        case REG_BG3PA + 0: io_bg3pa = (io_bg3pa & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG3PA + 1: io_bg3pa = (io_bg3pa & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_BG3PB + 0: io_bg3pb = (io_bg3pb & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG3PB + 1: io_bg3pb = (io_bg3pb & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_BG3PC + 0: io_bg3pc = (io_bg3pc & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG3PC + 1: io_bg3pc = (io_bg3pc & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_BG3PD + 0: io_bg3pd = (io_bg3pd & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG3PD + 1: io_bg3pd = (io_bg3pd & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_BG3X_L + 0: io_bg3x = (io_bg3x & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_BG3X_L + 1: io_bg3x = (io_bg3x & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_BG3X_H + 0: io_bg3x = (io_bg3x & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_BG3X_H + 1: io_bg3x = (io_bg3x & 0x00ffffff) | ((value << 24) & 0xff000000); break;
        case REG_BG3Y_L + 0: io_bg3y = (io_bg3y & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_BG3Y_L + 1: io_bg3y = (io_bg3y & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_BG3Y_H + 0: io_bg3y = (io_bg3y & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_BG3Y_H + 1: io_bg3y = (io_bg3y & 0x00ffffff) | ((value << 24) & 0xff000000); break;
        case REG_WIN0H + 0: io_win0h = (io_win0h & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_WIN0H + 1: io_win0h = (io_win0h & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_WIN1H + 0: io_win1h = (io_win1h & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_WIN1H + 1: io_win1h = (io_win1h & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_WIN0V + 0: io_win0v = (io_win0v & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_WIN0V + 1: io_win0v = (io_win0v & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_WIN1V + 0: io_win1v = (io_win1v & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_WIN1V + 1: io_win1v = (io_win1v & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_WININ + 0: io_winin = (io_winin & 0xff00) | ((value << 0) & 0x003f); break;
        case REG_WININ + 1: io_winin = (io_winin & 0x00ff) | ((value << 8) & 0x3f00); break;
        case REG_WINOUT + 0: io_winout = (io_winout & 0xff00) | ((value << 0) & 0x003f); break;
        case REG_WINOUT + 1: io_winout = (io_winout & 0x00ff) | ((value << 8) & 0x3f00); break;
        case REG_MOSAIC + 0: io_mosaic = (io_mosaic & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_MOSAIC + 1: io_mosaic = (io_mosaic & 0x00ff) | ((value << 8) & 0xff00); break;
        case 0x4e: break;
        case 0x4f: break;
        case REG_BLDCNT + 0: io_bldcnt = (io_bldcnt & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BLDCNT + 1: io_bldcnt = (io_bldcnt & 0x00ff) | ((value << 8) & 0x3f00); break;
        case REG_BLDALPHA + 0: io_bldalpha = (io_bldalpha & 0xff00) | ((value << 0) & 0x001f); break;
        case REG_BLDALPHA + 1: io_bldalpha = (io_bldalpha & 0x00ff) | ((value << 8) & 0x1f00); break;
        case REG_BLDY + 0: io_bldy = (io_bldy & 0xff00) | ((value << 0) & 0x001f); break;
        case REG_BLDY + 1: io_bldy = (io_bldy & 0x00ff) | ((value << 8) & 0x0000); break;
        case 0x56: break;
        case 0x57: break;
        case 0x58: break;
        case 0x59: break;
        case 0x5a: break;
        case 0x5b: break;
        case 0x5c: break;
        case 0x5d: break;
        case 0x5e: break;
        case 0x5f: break;
        case REG_SOUND1CNT_L + 0: io_sound1cnt_l = (io_sound1cnt_l & 0xff00) | ((value << 0) & 0x007f); break;
        case REG_SOUND1CNT_L + 1: io_sound1cnt_l = (io_sound1cnt_l & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_SOUND1CNT_H + 0: io_sound1cnt_h = (io_sound1cnt_h & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUND1CNT_H + 1: io_sound1cnt_h = (io_sound1cnt_h & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_SOUND1CNT_X + 0: io_sound1cnt_x = (io_sound1cnt_x & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUND1CNT_X + 1: io_sound1cnt_x = (io_sound1cnt_x & 0x00ff) | ((value << 8) & 0xc700); break;
        case 0x66: break;
        case 0x67: break;
        case REG_SOUND2CNT_L + 0: io_sound2cnt_l = (io_sound2cnt_l & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUND2CNT_L + 1: io_sound2cnt_l = (io_sound2cnt_l & 0x00ff) | ((value << 8) & 0xff00); break;
        case 0x6a: break;
        case 0x6b: break;
        case REG_SOUND2CNT_H + 0: io_sound2cnt_h = (io_sound2cnt_h & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUND2CNT_H + 1: io_sound2cnt_h = (io_sound2cnt_h & 0x00ff) | ((value << 8) & 0xc700); break;
        case 0x6e: break;
        case 0x6f: break;
        case REG_SOUND3CNT_L + 0: io_sound3cnt_l = (io_sound3cnt_l & 0xff00) | ((value << 0) & 0x00e0); break;
        case REG_SOUND3CNT_L + 1: io_sound3cnt_l = (io_sound3cnt_l & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_SOUND3CNT_H + 0: io_sound3cnt_h = (io_sound3cnt_h & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUND3CNT_H + 1: io_sound3cnt_h = (io_sound3cnt_h & 0x00ff) | ((value << 8) & 0xe000); break;
        case REG_SOUND3CNT_X + 0: io_sound3cnt_x = (io_sound3cnt_x & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUND3CNT_X + 1: io_sound3cnt_x = (io_sound3cnt_x & 0x00ff) | ((value << 8) & 0xc700); break;
        case 0x76: break;
        case 0x77: break;
        case REG_SOUND4CNT_L + 0: io_sound4cnt_l = (io_sound4cnt_l & 0xff00) | ((value << 0) & 0x003f); break;
        case REG_SOUND4CNT_L + 1: io_sound4cnt_l = (io_sound4cnt_l & 0x00ff) | ((value << 8) & 0xff00); break;
        case 0x7a: break;
        case 0x7b: break;
        case REG_SOUND4CNT_H + 0: io_sound4cnt_h = (io_sound4cnt_h & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUND4CNT_H + 1: io_sound4cnt_h = (io_sound4cnt_h & 0x00ff) | ((value << 8) & 0xc000); break;
        case 0x7e: break;
        case 0x7f: break;
        case REG_SOUNDCNT_L + 0: io_soundcnt_l = (io_soundcnt_l & 0xff00) | ((value << 0) & 0x0077); break;
        case REG_SOUNDCNT_L + 1: io_soundcnt_l = (io_soundcnt_l & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_SOUNDCNT_H + 0: io_soundcnt_h = (io_soundcnt_h & 0xff00) | ((value << 0) & 0x000f); break;
        case REG_SOUNDCNT_H + 1: io_soundcnt_h = (io_soundcnt_h & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_SOUNDCNT_X + 0: io_soundcnt_x = (io_soundcnt_x & 0xff00) | ((value << 0) & 0x008f); break;
        case REG_SOUNDCNT_X + 1: io_soundcnt_x = (io_soundcnt_x & 0x00ff) | ((value << 8) & 0x0000); break;
        case 0x86: break;
        case 0x87: break;
        case REG_SOUNDBIAS + 0: io_soundbias = (io_soundbias & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUNDBIAS + 1: io_soundbias = (io_soundbias & 0x00ff) | ((value << 8) & 0xc300); break;
        case 0x8a: break;
        case 0x8b: break;
        case 0x8c: break;
        case 0x8d: break;
        case 0x8e: break;
        case 0x8f: break;
        case REG_WAVE_RAM0_L + 0: io_wave_ram0 = (io_wave_ram0 & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_WAVE_RAM0_L + 1: io_wave_ram0 = (io_wave_ram0 & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_WAVE_RAM0_H + 0: io_wave_ram0 = (io_wave_ram0 & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_WAVE_RAM0_H + 1: io_wave_ram0 = (io_wave_ram0 & 0x00ffffff) | ((value << 24) & 0xff000000); break;
        case REG_WAVE_RAM1_L + 0: io_wave_ram1 = (io_wave_ram1 & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_WAVE_RAM1_L + 1: io_wave_ram1 = (io_wave_ram1 & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_WAVE_RAM1_H + 0: io_wave_ram1 = (io_wave_ram1 & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_WAVE_RAM1_H + 1: io_wave_ram1 = (io_wave_ram1 & 0x00ffffff) | ((value << 24) & 0xff000000); break;
        case REG_WAVE_RAM2_L + 0: io_wave_ram2 = (io_wave_ram2 & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_WAVE_RAM2_L + 1: io_wave_ram2 = (io_wave_ram2 & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_WAVE_RAM2_H + 0: io_wave_ram2 = (io_wave_ram2 & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_WAVE_RAM2_H + 1: io_wave_ram2 = (io_wave_ram2 & 0x00ffffff) | ((value << 24) & 0xff000000); break;
        case REG_WAVE_RAM3_L + 0: io_wave_ram3 = (io_wave_ram3 & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_WAVE_RAM3_L + 1: io_wave_ram3 = (io_wave_ram3 & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_WAVE_RAM3_H + 0: io_wave_ram3 = (io_wave_ram3 & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_WAVE_RAM3_H + 1: io_wave_ram3 = (io_wave_ram3 & 0x00ffffff) | ((value << 24) & 0xff000000); break;
        case REG_FIFO_A_L + 0: io_fifo_a = (io_fifo_a & 0xffffff00) | ((value << 0) & 0x000000ff); break;  // FIXME
        case REG_FIFO_A_L + 1: io_fifo_a = (io_fifo_a & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_FIFO_A_H + 0: io_fifo_a = (io_fifo_a & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_FIFO_A_H + 1: io_fifo_a = (io_fifo_a & 0x00ffffff) | ((value << 24) & 0xff000000); break;
        case REG_FIFO_B_L + 0: io_fifo_b = (io_fifo_b & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_FIFO_B_L + 1: io_fifo_b = (io_fifo_b & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_FIFO_B_H + 0: io_fifo_b = (io_fifo_b & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_FIFO_B_H + 1: io_fifo_b = (io_fifo_b & 0x00ffffff) | ((value << 24) & 0xff000000); break;
        case 0xa8: break;
        case 0xa9: break;
        case 0xaa: break;
        case 0xab: break;
        case 0xac: break;
        case 0xad: break;
        case 0xae: break;
        case 0xaf: break;
        case REG_DMA0SAD_L + 0: io_dma0sad = (io_dma0sad & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_DMA0SAD_L + 1: io_dma0sad = (io_dma0sad & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_DMA0SAD_H + 0: io_dma0sad = (io_dma0sad & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_DMA0SAD_H + 1: io_dma0sad = (io_dma0sad & 0x00ffffff) | ((value << 24) & 0x07000000); break;
        case REG_DMA0DAD_L + 0: io_dma0dad = (io_dma0dad & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_DMA0DAD_L + 1: io_dma0dad = (io_dma0dad & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_DMA0DAD_H + 0: io_dma0dad = (io_dma0dad & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_DMA0DAD_H + 1: io_dma0dad = (io_dma0dad & 0x00ffffff) | ((value << 24) & 0x07000000); break;
        case REG_DMA0CNT_L + 0: io_dma0cnt_l = (io_dma0cnt_l & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_DMA0CNT_L + 1: io_dma0cnt_l = (io_dma0cnt_l & 0x00ff) | ((value << 8) & 0x3f00); break;
        case REG_DMA0CNT_H + 0: io_dma0cnt_h = (io_dma0cnt_h & 0xff00) | ((value << 0) & 0x00e0); break;
        case REG_DMA0CNT_H + 1: io_dma0cnt_h = (io_dma0cnt_h & 0x00ff) | ((value << 8) & 0xf700); break;
        case REG_DMA1SAD_L + 0: io_dma1sad = (io_dma1sad & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_DMA1SAD_L + 1: io_dma1sad = (io_dma1sad & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_DMA1SAD_H + 0: io_dma1sad = (io_dma1sad & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_DMA1SAD_H + 1: io_dma1sad = (io_dma1sad & 0x00ffffff) | ((value << 24) & 0x0f000000); break;
        case REG_DMA1DAD_L + 0: io_dma1dad = (io_dma1dad & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_DMA1DAD_L + 1: io_dma1dad = (io_dma1dad & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_DMA1DAD_H + 0: io_dma1dad = (io_dma1dad & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_DMA1DAD_H + 1: io_dma1dad = (io_dma1dad & 0x00ffffff) | ((value << 24) & 0x07000000); break;
        case REG_DMA1CNT_L + 0: io_dma1cnt_l = (io_dma1cnt_l & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_DMA1CNT_L + 1: io_dma1cnt_l = (io_dma1cnt_l & 0x00ff) | ((value << 8) & 0x3f00); break;
        case REG_DMA1CNT_H + 0: io_dma1cnt_h = (io_dma1cnt_h & 0xff00) | ((value << 0) & 0x00e0); break;
        case REG_DMA1CNT_H + 1: io_dma1cnt_h = (io_dma1cnt_h & 0x00ff) | ((value << 8) & 0xf700); break;
        case REG_DMA2SAD_L + 0: io_dma2sad = (io_dma2sad & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_DMA2SAD_L + 1: io_dma2sad = (io_dma2sad & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_DMA2SAD_H + 0: io_dma2sad = (io_dma2sad & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_DMA2SAD_H + 1: io_dma2sad = (io_dma2sad & 0x00ffffff) | ((value << 24) & 0x0f000000); break;
        case REG_DMA2DAD_L + 0: io_dma2dad = (io_dma2dad & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_DMA2DAD_L + 1: io_dma2dad = (io_dma2dad & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_DMA2DAD_H + 0: io_dma2dad = (io_dma2dad & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_DMA2DAD_H + 1: io_dma2dad = (io_dma2dad & 0x00ffffff) | ((value << 24) & 0x07000000); break;
        case REG_DMA2CNT_L + 0: io_dma2cnt_l = (io_dma2cnt_l & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_DMA2CNT_L + 1: io_dma2cnt_l = (io_dma2cnt_l & 0x00ff) | ((value << 8) & 0x3f00); break;
        case REG_DMA2CNT_H + 0: io_dma2cnt_h = (io_dma2cnt_h & 0xff00) | ((value << 0) & 0x00e0); break;
        case REG_DMA2CNT_H + 1: io_dma2cnt_h = (io_dma2cnt_h & 0x00ff) | ((value << 8) & 0xf700); break;
        case REG_DMA3SAD_L + 0: io_dma3sad = (io_dma3sad & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_DMA3SAD_L + 1: io_dma3sad = (io_dma3sad & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_DMA3SAD_H + 0: io_dma3sad = (io_dma3sad & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_DMA3SAD_H + 1: io_dma3sad = (io_dma3sad & 0x00ffffff) | ((value << 24) & 0x0f000000); break;
        case REG_DMA3DAD_L + 0: io_dma3dad = (io_dma3dad & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_DMA3DAD_L + 1: io_dma3dad = (io_dma3dad & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_DMA3DAD_H + 0: io_dma3dad = (io_dma3dad & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_DMA3DAD_H + 1: io_dma3dad = (io_dma3dad & 0x00ffffff) | ((value << 24) & 0x0f000000); break;
        case REG_DMA3CNT_L + 0: io_dma3cnt_l = (io_dma3cnt_l & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_DMA3CNT_L + 1: io_dma3cnt_l = (io_dma3cnt_l & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_DMA3CNT_H + 0: io_dma3cnt_h = (io_dma3cnt_h & 0xff00) | ((value << 0) & 0x00e0); break;
        case REG_DMA3CNT_H + 1: io_dma3cnt_h = (io_dma3cnt_h & 0x00ff) | ((value << 8) & 0xff00); break;
        case 0xe0: break;
        case 0xe1: break;
        case 0xe2: break;
        case 0xe3: break;
        case 0xe4: break;
        case 0xe5: break;
        case 0xe6: break;
        case 0xe7: break;
        case 0xe8: break;
        case 0xe9: break;
        case 0xea: break;
        case 0xeb: break;
        case 0xec: break;
        case 0xed: break;
        case 0xee: break;
        case 0xef: break;
        case 0xf0: break;
        case 0xf1: break;
        case 0xf2: break;
        case 0xf3: break;
        case 0xf4: break;
        case 0xf5: break;
        case 0xf6: break;
        case 0xf7: break;
        case 0xf8: break;
        case 0xf9: break;
        case 0xfa: break;
        case 0xfb: break;
        case 0xfc: break;
        case 0xfd: break;
        case 0xfe: break;
        case 0xff: break;
        case 0x100c: break;
        case 0x100d: break;
        case 0x100e: break;
        case 0x100f: break;

        case REG_TM0CNT_L + 0: assert(false); break;
        case REG_TM0CNT_L + 1: assert(false); break;
        case REG_TM1CNT_L + 0: assert(false); break;
        case REG_TM1CNT_L + 1: assert(false); break;
        case REG_TM2CNT_L + 0: assert(false); break;
        case REG_TM2CNT_L + 1: assert(false); break;
        case REG_TM3CNT_L + 0: assert(false); break;
        case REG_TM3CNT_L + 1: assert(false); break;

        case REG_IF:
            io_if &= ~value;
            break;

        case REG_IME:
            io_ime = (io_ime & 0xff00) | value;
            break;

        case REG_HALTCNT:
            io_haltcnt = value;
            halted = true;
            break;

        default:
#ifdef LOG_BAD_MEMORY_ACCESS
            printf("io_write_byte(0x%08x, 0x%02x);\n", address, value);
#endif
            break;
    }
}

uint16_t io_read_halfword(uint32_t address) {
    switch (address) {
        case REG_DISPCNT: return io_dispcnt;
        case 0x2: return 0xdead;
        case REG_DISPSTAT: return io_dispstat;
        case REG_VCOUNT: return io_vcount;
        case REG_BG0CNT: return io_bg0cnt;
        case REG_BG1CNT: return io_bg1cnt;
        case REG_BG2CNT: return io_bg2cnt;
        case REG_BG3CNT: return io_bg3cnt;
        case REG_BG0HOFS: return 0xdead;
        case REG_BG0VOFS: return 0xdead;
        case REG_BG1HOFS: return 0xdead;
        case REG_BG1VOFS: return 0xdead;
        case REG_BG2HOFS: return 0xdead;
        case REG_BG2VOFS: return 0xdead;
        case REG_BG3HOFS: return 0xdead;
        case REG_BG3VOFS: return 0xdead;
        case REG_BG2PA: return 0xdead;
        case REG_BG2PB: return 0xdead;
        case REG_BG2PC: return 0xdead;
        case REG_BG2PD: return 0xdead;
        case REG_BG2X_L: return 0xdead;
        case REG_BG2X_H: return 0xdead;
        case REG_BG2Y_L: return 0xdead;
        case REG_BG2Y_H: return 0xdead;
        case REG_BG3PA: return 0xdead;
        case REG_BG3PB: return 0xdead;
        case REG_BG3PC: return 0xdead;
        case REG_BG3PD: return 0xdead;
        case REG_BG3X_L: return 0xdead;
        case REG_BG3X_H: return 0xdead;
        case REG_BG3Y_L: return 0xdead;
        case REG_BG3Y_H: return 0xdead;
        case REG_WIN0H: return 0xdead;
        case REG_WIN1H: return 0xdead;
        case REG_WIN0V: return 0xdead;
        case REG_WIN1V: return 0xdead;
        case REG_WININ: return io_winin;
        case REG_WINOUT: return io_winout;
        case REG_MOSAIC: return 0xdead;
        case 0x4e: return 0xdead;
        case REG_BLDCNT: return io_bldcnt;
        case REG_BLDALPHA: return io_bldalpha;
        case REG_BLDY: return 0xdead;
        case 0x56: return 0xdead;
        case 0x58: return 0xdead;
        case 0x5a: return 0xdead;
        case 0x5c: return 0xdead;
        case 0x5e: return 0xdead;
        case REG_SOUND1CNT_L: return io_sound1cnt_l;
        case REG_SOUND1CNT_H: return io_sound1cnt_h & 0xffc0;
        case REG_SOUND1CNT_X: return io_sound1cnt_x & 0x7800;
        case 0x66: return 0;
        case REG_SOUND2CNT_L: return io_sound2cnt_l & 0xffc0;
        case 0x6a: return 0;
        case REG_SOUND2CNT_H: return io_sound2cnt_h & 0x7800;
        case 0x6e: return 0;
        case REG_SOUND3CNT_L: return io_sound3cnt_l;
        case REG_SOUND3CNT_H: return io_sound3cnt_h & 0xff00;
        case REG_SOUND3CNT_X: return io_sound3cnt_x & 0x7800;
        case 0x76: return 0;
        case REG_SOUND4CNT_L: return io_sound4cnt_l & 0xffc0;
        case 0x7a: return 0;
        case REG_SOUND4CNT_H: return io_sound4cnt_h & 0x7fff;
        case 0x7e: return 0;
        case REG_SOUNDCNT_L: return io_soundcnt_l;
        case REG_SOUNDCNT_H: return io_soundcnt_h & 0x77ff;
        case REG_SOUNDCNT_X: return io_soundcnt_x & 0xfff0;
        case 0x86: return 0;
        case REG_SOUNDBIAS: return io_soundbias;
        case 0x8a: return 0;
        case 0x8c: return 0xdead;
        case 0x8e: return 0xdead;
        case REG_WAVE_RAM0_L: return (uint16_t)(io_wave_ram0 >> 0);
        case REG_WAVE_RAM0_H: return (uint16_t)(io_wave_ram0 >> 16);
        case REG_WAVE_RAM1_L: return (uint16_t)(io_wave_ram1 >> 0);
        case REG_WAVE_RAM1_H: return (uint16_t)(io_wave_ram1 >> 16);
        case REG_WAVE_RAM2_L: return (uint16_t)(io_wave_ram2 >> 0);
        case REG_WAVE_RAM2_H: return (uint16_t)(io_wave_ram2 >> 16);
        case REG_WAVE_RAM3_L: return (uint16_t)(io_wave_ram3 >> 0);
        case REG_WAVE_RAM3_H: return (uint16_t)(io_wave_ram3 >> 16);
        case REG_FIFO_A_L: return 0xdead;
        case REG_FIFO_A_H: return 0xdead;
        case REG_FIFO_B_L: return 0xdead;
        case REG_FIFO_B_H: return 0xdead;
        case 0xa8: return 0xdead;
        case 0xaa: return 0xdead;
        case 0xac: return 0xdead;
        case 0xae: return 0xdead;
        case REG_DMA0SAD_L: return 0xdead;
        case REG_DMA0SAD_H: return 0xdead;
        case REG_DMA0DAD_L: return 0xdead;
        case REG_DMA0DAD_H: return 0xdead;
        case REG_DMA0CNT_L: return 0;
        case REG_DMA0CNT_H: return io_dma0cnt_h;
        case REG_DMA1SAD_L: return 0xdead;
        case REG_DMA1SAD_H: return 0xdead;
        case REG_DMA1DAD_L: return 0xdead;
        case REG_DMA1DAD_H: return 0xdead;
        case REG_DMA1CNT_L: return 0;
        case REG_DMA1CNT_H: return io_dma1cnt_h;
        case REG_DMA2SAD_L: return 0xdead;
        case REG_DMA2SAD_H: return 0xdead;
        case REG_DMA2DAD_L: return 0xdead;
        case REG_DMA2DAD_H: return 0xdead;
        case REG_DMA2CNT_L: return 0;
        case REG_DMA2CNT_H: return io_dma2cnt_h;
        case REG_DMA3SAD_L: return 0xdead;
        case REG_DMA3SAD_H: return 0xdead;
        case REG_DMA3DAD_L: return 0xdead;
        case REG_DMA3DAD_H: return 0xdead;
        case REG_DMA3CNT_L: return 0;
        case REG_DMA3CNT_H: return io_dma3cnt_h;
        case 0xe0: return 0xdead;
        case 0xe2: return 0xdead;
        case 0xe4: return 0xdead;
        case 0xe6: return 0xdead;
        case 0xe8: return 0xdead;
        case 0xea: return 0xdead;
        case 0xec: return 0xdead;
        case 0xee: return 0xdead;
        case 0xf0: return 0xdead;
        case 0xf2: return 0xdead;
        case 0xf4: return 0xdead;
        case 0xf6: return 0xdead;
        case 0xf8: return 0xdead;
        case 0xfa: return 0xdead;
        case 0xfc: return 0xdead;
        case 0xfe: return 0xdead;
        case 0x100c: return 0xdead;
        case 0x100e: return 0xdead;

        case REG_TM0CNT_L: return timer_0_counter;
        case REG_TM0CNT_H: return timer_0_control;
        case REG_TM1CNT_L: return timer_1_counter;
        case REG_TM1CNT_H: return timer_1_control;
        case REG_TM2CNT_L: return timer_2_counter;
        case REG_TM2CNT_H: return timer_2_control;
        case REG_TM3CNT_L: return timer_3_counter;
        case REG_TM3CNT_H: return timer_3_control;

        case REG_SIODATA32: return 0;  // FIXME
        case REG_SIOCNT: return 0;  // FIXME

        case REG_KEYINPUT:
            io_keyinput = 0x3ff;
            for (int i = 0; i < NUM_KEYS; i++) {
                if (keys[i]) io_keyinput &= ~(1 << i);
            }
            return io_keyinput;

        //case IO_RCNT:
        //    return io_rcnt;

        case REG_IE: return io_ie;
        case REG_IF: return io_if;
        case REG_IME: return io_ime;

        default:
#ifdef LOG_BAD_MEMORY_ACCESS
            printf("io_read_halfword(0x%08x);\n", address);
#endif
            return 0;
    }
}

void io_write_halfword(uint32_t address, uint16_t value) {
    switch (address) {
        case REG_DISPCNT: io_dispcnt = (io_dispcnt & 0x0008) | (value & 0xfff7); break;
        case 0x2: break;
        case REG_DISPSTAT: io_dispstat = (io_dispstat & 0x0007) | (value & 0xff38); break;
        case REG_VCOUNT: break;
        case REG_BG0CNT: io_bg0cnt = value & 0xdfff; break;
        case REG_BG1CNT: io_bg1cnt = value & 0xdfff; break;
        case REG_BG2CNT: io_bg2cnt = value & 0xffff; break;
        case REG_BG3CNT: io_bg3cnt = value & 0xffff; break;
        case REG_BG0HOFS: io_bg0hofs = value & 0x00ff; break;
        case REG_BG0VOFS: io_bg0vofs = value & 0x00ff; break;
        case REG_BG1HOFS: io_bg1hofs = value & 0x00ff; break;
        case REG_BG1VOFS: io_bg1vofs = value & 0x00ff; break;
        case REG_BG2HOFS: io_bg2hofs = value & 0x00ff; break;
        case REG_BG2VOFS: io_bg2vofs = value & 0x00ff; break;
        case REG_BG3HOFS: io_bg3hofs = value & 0x00ff; break;
        case REG_BG3VOFS: io_bg3vofs = value & 0x00ff; break;
        case REG_BG2PA: io_bg2pa = value & 0xffff; break;
        case REG_BG2PB: io_bg2pb = value & 0xffff; break;
        case REG_BG2PC: io_bg2pc = value & 0xffff; break;
        case REG_BG2PD: io_bg2pd = value & 0xffff; break;
        case REG_BG2X_L: io_bg2x = (io_bg2x & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_BG2X_H: io_bg2x = (io_bg2x & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_BG2Y_L: io_bg2y = (io_bg2y & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_BG2Y_H: io_bg2y = (io_bg2y & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_BG3PA: io_bg3pa = value & 0xffff; break;
        case REG_BG3PB: io_bg3pb = value & 0xffff; break;
        case REG_BG3PC: io_bg3pc = value & 0xffff; break;
        case REG_BG3PD: io_bg3pd = value & 0xffff; break;
        case REG_BG3X_L: io_bg3x = (io_bg3x & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_BG3X_H: io_bg3x = (io_bg3x & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_BG3Y_L: io_bg3y = (io_bg3y & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_BG3Y_H: io_bg3y = (io_bg3y & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_WIN0H: io_win0h = value & 0xffff; break;
        case REG_WIN1H: io_win1h = value & 0xffff; break;
        case REG_WIN0V: io_win0v = value & 0xffff; break;
        case REG_WIN1V: io_win1v = value & 0xffff; break;
        case REG_WININ: io_winin = value & 0x3f3f; break;
        case REG_WINOUT: io_winout = value & 0x3f3f; break;
        case REG_MOSAIC: io_mosaic = value & 0xffff; break;
        case 0x4e: break;
        case REG_BLDCNT: io_bldcnt = value & 0x3fff; break;
        case REG_BLDALPHA: io_bldalpha = value & 0x1f1f; break;
        case REG_BLDY: io_bldy = value & 0x001f; break;
        case 0x56: break;
        case 0x58: break;
        case 0x5a: break;
        case 0x5c: break;
        case 0x5e: break;
        case REG_SOUND1CNT_L: io_sound1cnt_l = value & 0x007f; break;
        case REG_SOUND1CNT_H: io_sound1cnt_h = value & 0xffff; break;
        case REG_SOUND1CNT_X: io_sound1cnt_x = value & 0xc7ff; break;
        case 0x66: break;
        case REG_SOUND2CNT_L: io_sound2cnt_l = value & 0xffff; break;
        case 0x6a: break;
        case REG_SOUND2CNT_H: io_sound2cnt_h = value & 0xc7ff; break;
        case 0x6e: break;
        case REG_SOUND3CNT_L: io_sound3cnt_l = value & 0x00e0; break;
        case REG_SOUND3CNT_H: io_sound3cnt_h = value & 0xe0ff; break;
        case REG_SOUND3CNT_X: io_sound3cnt_x = value & 0xc7ff; break;
        case 0x76: break;
        case REG_SOUND4CNT_L: io_sound4cnt_l = value & 0xff3f; break;
        case 0x7a: break;
        case REG_SOUND4CNT_H: io_sound4cnt_h = value & 0xc0ff; break;
        case 0x7e: break;
        case REG_SOUNDCNT_L: io_soundcnt_l = value & 0xff77; break;
        case REG_SOUNDCNT_H: io_soundcnt_h = value & 0xff0f; break;
        case REG_SOUNDCNT_X: io_soundcnt_x = value & 0x008f; break;
        case 0x86: break;
        case REG_SOUNDBIAS: io_soundbias = value & 0xc3ff; break;
        case 0x8a: break;
        case 0x8c: break;
        case 0x8e: break;
        case REG_WAVE_RAM0_L: io_wave_ram0 = (io_wave_ram0 & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_WAVE_RAM0_H: io_wave_ram0 = (io_wave_ram0 & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_WAVE_RAM1_L: io_wave_ram1 = (io_wave_ram1 & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_WAVE_RAM1_H: io_wave_ram1 = (io_wave_ram1 & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_WAVE_RAM2_L: io_wave_ram2 = (io_wave_ram2 & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_WAVE_RAM2_H: io_wave_ram2 = (io_wave_ram2 & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_WAVE_RAM3_L: io_wave_ram3 = (io_wave_ram3 & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_WAVE_RAM3_H: io_wave_ram3 = (io_wave_ram3 & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_FIFO_A_L: io_fifo_a = (io_fifo_a & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_FIFO_A_H: io_fifo_a = (io_fifo_a & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_FIFO_B_L: io_fifo_b = (io_fifo_b & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_FIFO_B_H: io_fifo_b = (io_fifo_b & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case 0xa8: break;
        case 0xaa: break;
        case 0xac: break;
        case 0xae: break;
        case REG_DMA0SAD_L: io_dma0sad = (io_dma0sad & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_DMA0SAD_H: io_dma0sad = (io_dma0sad & 0x0000ffff) | ((value << 16) & 0x07ff0000); break;
        case REG_DMA0DAD_L: io_dma0dad = (io_dma0dad & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_DMA0DAD_H: io_dma0dad = (io_dma0dad & 0x0000ffff) | ((value << 16) & 0x07ff0000); break;
        case REG_DMA0CNT_L: io_dma0cnt_l = value & 0x3fff; break;
        case REG_DMA0CNT_H: io_dma0cnt_h = value & 0xf7e0; break;
        case REG_DMA1SAD_L: io_dma1sad = (io_dma1sad & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_DMA1SAD_H: io_dma1sad = (io_dma1sad & 0x0000ffff) | ((value << 16) & 0x0fff0000); break;
        case REG_DMA1DAD_L: io_dma1dad = (io_dma1dad & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_DMA1DAD_H: io_dma1dad = (io_dma1dad & 0x0000ffff) | ((value << 16) & 0x07ff0000); break;
        case REG_DMA1CNT_L: io_dma1cnt_l = value & 0x3fff; break;
        case REG_DMA1CNT_H: io_dma1cnt_h = value & 0xf7e0; break;
        case REG_DMA2SAD_L: io_dma2sad = (io_dma2sad & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_DMA2SAD_H: io_dma2sad = (io_dma2sad & 0x0000ffff) | ((value << 16) & 0x0fff0000); break;
        case REG_DMA2DAD_L: io_dma2dad = (io_dma2dad & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_DMA2DAD_H: io_dma2dad = (io_dma2dad & 0x0000ffff) | ((value << 16) & 0x07ff0000); break;
        case REG_DMA2CNT_L: io_dma2cnt_l = value & 0x3fff; break;
        case REG_DMA2CNT_H: io_dma2cnt_h = value & 0xf7e0; break;
        case REG_DMA3SAD_L: io_dma3sad = (io_dma3sad & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_DMA3SAD_H: io_dma3sad = (io_dma3sad & 0x0000ffff) | ((value << 16) & 0x0fff0000); break;
        case REG_DMA3DAD_L: io_dma3dad = (io_dma3dad & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_DMA3DAD_H: io_dma3dad = (io_dma3dad & 0x0000ffff) | ((value << 16) & 0x0fff0000); break;
        case REG_DMA3CNT_L: io_dma3cnt_l = value & 0xffff; break;
        case REG_DMA3CNT_H: io_dma3cnt_h = value & 0xffe0; break;
        case 0xe0: break;
        case 0xe2: break;
        case 0xe4: break;
        case 0xe6: break;
        case 0xe8: break;
        case 0xea: break;
        case 0xec: break;
        case 0xee: break;
        case 0xf0: break;
        case 0xf2: break;
        case 0xf4: break;
        case 0xf6: break;
        case 0xf8: break;
        case 0xfa: break;
        case 0xfc: break;
        case 0xfe: break;
        case 0x100c: break;
        case 0x100e: break;

        case REG_TM0CNT_L:
            timer_0_reload = value;
            break;

        case REG_TM0CNT_H:
            timer_0_control = value;
            if (timer_0_control & 0x80) timer_0_counter = timer_0_reload;
            break;

        case REG_TM1CNT_L:
            timer_1_reload = value;
            break;

        case REG_TM1CNT_H:
            timer_1_control = value;
            if (timer_1_control & 0x80) timer_1_counter = timer_1_reload;
            break;

        case REG_TM2CNT_L:
            timer_2_reload = value;
            break;

        case REG_TM2CNT_H:
            timer_2_control = value;
            if (timer_2_control & 0x80) timer_2_counter = timer_2_reload;
            break;

        case REG_TM3CNT_L:
            timer_3_reload = value;
            break;

        case REG_TM3CNT_H:
            timer_3_control = value;
            if (timer_3_control & 0x80) timer_3_counter = timer_3_reload;
            break;

        case REG_IE: io_ie = value; break;
        case REG_IF: io_if &= ~value; break;

        //case IO_WAITCNT:
        //    io_waitcnt = value;
        //    printf("WAITCNT = 0x%04x\n", io_waitcnt);
        //    break;

        case REG_IME: io_ime = value; break;

        default:
#ifdef LOG_BAD_MEMORY_ACCESS
            printf("io_write_halfword(0x%08x, 0x%04x);\n", address, value);
#endif
            break;
    }
}

uint32_t io_read_word(uint32_t address) {
    switch (address) {
        case REG_DISPCNT: return io_dispcnt | 0xdead << 16;
        case REG_DISPSTAT: return io_dispstat | io_vcount << 16;
        case REG_BG0CNT: return io_bg0cnt | io_bg1cnt << 16;
        case REG_BG2CNT: return io_bg2cnt | io_bg3cnt << 16;
        case REG_BG0HOFS: return 0xdeaddead;
        case REG_BG1HOFS: return 0xdeaddead;
        case REG_BG2HOFS: return 0xdeaddead;
        case REG_BG3HOFS: return 0xdeaddead;
        case REG_BG2PA: return 0xdeaddead;
        case REG_BG2PC: return 0xdeaddead;
        case REG_BG2X_L: return 0xdeaddead;
        case REG_BG2Y_L: return 0xdeaddead;
        case REG_BG3PA: return 0xdeaddead;
        case REG_BG3PC: return 0xdeaddead;
        case REG_BG3X_L: return 0xdeaddead;
        case REG_BG3Y_L: return 0xdeaddead;
        case REG_WIN0H: return 0xdeaddead;
        case REG_WIN0V: return 0xdeaddead;
        case REG_WININ: return io_winin | io_winout << 16;
        case REG_MOSAIC: return 0xdeaddead;
        case REG_BLDCNT: return io_bldcnt | io_bldalpha << 16;
        case REG_BLDY: return io_bldy | 0xdead << 16;
        case 0x58: return 0xdeaddead;
        case 0x5c: return 0xdeaddead;
        case REG_SOUND1CNT_L: return io_sound1cnt_l | (io_sound1cnt_h & 0xffc0) << 16;
        case REG_SOUND1CNT_X: return io_sound1cnt_x & 0x7800;
        case REG_SOUND2CNT_L: return io_sound2cnt_l & 0xffc0;
        case REG_SOUND2CNT_H: return io_sound2cnt_h & 0x7800;
        case REG_SOUND3CNT_L: return io_sound3cnt_l | (io_sound3cnt_h & 0xff00) << 16;
        case REG_SOUND3CNT_X: return io_sound3cnt_x & 0x7800;
        case REG_SOUND4CNT_L: return io_sound4cnt_l & 0xffc0;
        case REG_SOUND4CNT_H: return io_sound4cnt_h & 0x7fff;
        case REG_SOUNDCNT_L: return io_soundcnt_l | (io_soundcnt_h & 0x77ff) << 16;
        case REG_SOUNDCNT_X: return io_soundcnt_x & 0xfff0;
        case REG_SOUNDBIAS: return io_soundbias;
        case 0x8c: return 0xdeaddead;
        case REG_WAVE_RAM0_L: return io_wave_ram0;
        case REG_WAVE_RAM1_L: return io_wave_ram1;
        case REG_WAVE_RAM2_L: return io_wave_ram2;
        case REG_WAVE_RAM3_L: return io_wave_ram3;
        case REG_FIFO_A_L: return 0xdeaddead;
        case REG_FIFO_B_L: return 0xdeaddead;
        case 0xa8: return 0xdeaddead;
        case 0xac: return 0xdeaddead;
        case REG_DMA0SAD_L: return 0xdeaddead;
        case REG_DMA0DAD_L: return 0xdeaddead;
        case REG_DMA0CNT_L: return io_dma0cnt_h << 16;
        case REG_DMA1SAD_L: return 0xdeaddead;
        case REG_DMA1DAD_L: return 0xdeaddead;
        case REG_DMA1CNT_L: return io_dma1cnt_h << 16;
        case REG_DMA2SAD_L: return 0xdeaddead;
        case REG_DMA2DAD_L: return 0xdeaddead;
        case REG_DMA2CNT_L: return io_dma2cnt_h << 16;
        case REG_DMA3SAD_L: return 0xdeaddead;
        case REG_DMA3DAD_L: return 0xdeaddead;
        case REG_DMA3CNT_L: return io_dma3cnt_h << 16;
        case 0xe0: return 0xdeaddead;
        case 0xe4: return 0xdeaddead;
        case 0xe8: return 0xdeaddead;
        case 0xec: return 0xdeaddead;
        case 0xf0: return 0xdeaddead;
        case 0xf4: return 0xdeaddead;
        case 0xf8: return 0xdeaddead;
        case 0xfc: return 0xdeaddead;
        case 0x100c: return 0xdeaddead;

        case REG_TM0CNT_L: return timer_0_counter | timer_0_control << 16;
        case REG_TM1CNT_L: return timer_1_counter | timer_1_control << 16;
        case REG_TM2CNT_L: return timer_2_counter | timer_2_control << 16;
        case REG_TM3CNT_L: return timer_3_counter | timer_3_control << 16;

        //case IO_KEYINPUT:
        //    io_keyinput = 0x3ff;
        //    for (int i = 0; i < NUM_KEYS; i++) {
        //        if (keys[i]) io_keyinput &= ~(1 << i);
        //    }
        //    return io_keyinput | io_keycnt << 16;

        case REG_IE:
            return io_ie | io_if << 16;

        case REG_IME:
            return io_ime;

        default:
#ifdef LOG_BAD_MEMORY_ACCESS
            printf("io_read_word(0x%08x);\n", address);
#endif
            return 0;
    }
}

void io_write_word(uint32_t address, uint32_t value) {
    switch (address) {
        case REG_DISPCNT: io_dispcnt = (io_dispcnt & 0x0008) | (value & 0xfff7); break;
        case REG_DISPSTAT: io_dispstat = (io_dispstat & 0x0007) | (value & 0xff38); break;
        case REG_BG0CNT: io_bg0cnt = value & 0xdfff; io_bg1cnt = (value >> 16) & 0xdfff; break;
        case REG_BG2CNT: io_bg2cnt = value & 0xffff; io_bg3cnt = (value >> 16) & 0xffff; break;
        case REG_BG0HOFS: io_bg0hofs = value & 0x00ff; io_bg0vofs = (value >> 16) & 0x00ff; break;
        case REG_BG1HOFS: io_bg1hofs = value & 0x00ff; io_bg1vofs = (value >> 16) & 0x00ff; break;
        case REG_BG2HOFS: io_bg2hofs = value & 0x00ff; io_bg2vofs = (value >> 16) & 0x00ff; break;
        case REG_BG3HOFS: io_bg3hofs = value & 0x00ff; io_bg3vofs = (value >> 16) & 0x00ff; break;
        case REG_BG2PA: io_bg2pa = value & 0xffff; io_bg2pb = (value >> 16) & 0xffff; break;
        case REG_BG2PC: io_bg2pc = value & 0xffff; io_bg2pd = (value >> 16) & 0xffff; break;
        case REG_BG2X_L: io_bg2x = value & 0xffffffff; break;
        case REG_BG2Y_L: io_bg2y = value & 0xffffffff; break;
        case REG_BG3PA: io_bg3pa = value & 0xffff; io_bg3pb = (value >> 16) & 0xffff; break;
        case REG_BG3PC: io_bg3pc = value & 0xffff; io_bg3pd = (value >> 16) & 0xffff; break;
        case REG_BG3X_L: io_bg3x = value & 0xffffffff; break;
        case REG_BG3Y_L: io_bg3y = value & 0xffffffff; break;
        case REG_WIN0H: io_win0h = value & 0xffff; io_win1h = (value >> 16) & 0xffff; break;
        case REG_WIN0V: io_win0v = value & 0xffff; io_win1v = (value >> 16) & 0xffff; break;
        case REG_WININ: io_winin = value & 0x3f3f; io_winout = (value >> 16) & 0x3f3f; break;
        case REG_MOSAIC: io_mosaic = value & 0xffff; break;
        case REG_BLDCNT: io_bldcnt = value & 0x3fff; io_bldalpha = (value >> 16) & 0x1f1f; break;
        case REG_BLDY: io_bldy = value & 0x001f; break;
        case 0x58: break;
        case 0x5c: break;
        case REG_SOUND1CNT_L: io_sound1cnt_l = value & 0x007f; io_sound1cnt_h = (value >> 16) & 0xffff; break;
        case REG_SOUND1CNT_X: io_sound1cnt_x = value & 0xc7ff; break;
        case REG_SOUND2CNT_L: io_sound2cnt_l = value & 0xffff; break;
        case REG_SOUND2CNT_H: io_sound2cnt_h = value & 0xc7ff; break;
        case REG_SOUND3CNT_L: io_sound3cnt_l = value & 0x00e0; io_sound3cnt_h = (value >> 16) & 0xe0ff; break;
        case REG_SOUND3CNT_X: io_sound3cnt_x = value & 0xc7ff; break;
        case REG_SOUND4CNT_L: io_sound4cnt_l = value & 0xff3f; break;
        case REG_SOUND4CNT_H: io_sound4cnt_h = value & 0xc0ff; break;
        case REG_SOUNDCNT_L: io_soundcnt_l = value & 0xff77; io_soundcnt_h = (value >> 16) & 0xff0f; break;
        case REG_SOUNDCNT_X: io_soundcnt_x = value & 0x008f; break;
        case REG_SOUNDBIAS: io_soundbias = value & 0xc3ff; break;
        case 0x8c: break;
        case REG_WAVE_RAM0_L: io_wave_ram0 = value; break;
        case REG_WAVE_RAM1_L: io_wave_ram1 = value; break;
        case REG_WAVE_RAM2_L: io_wave_ram2 = value; break;
        case REG_WAVE_RAM3_L: io_wave_ram3 = value; break;
        case REG_FIFO_A_L: io_fifo_a = value; break;
        case REG_FIFO_B_L: io_fifo_b = value; break;
        case 0xa8: break;
        case 0xac: break;
        case REG_DMA0SAD_L: io_dma0sad = value & 0x07ffffff; break;
        case REG_DMA0DAD_L: io_dma0dad = value & 0x07ffffff; break;
        case REG_DMA0CNT_L: io_dma0cnt_l = value & 0x3fff; io_dma0cnt_h = (value >> 16) & 0xf7e0; break;
        case REG_DMA1SAD_L: io_dma1sad = value & 0x0fffffff; break;
        case REG_DMA1DAD_L: io_dma1dad = value & 0x07ffffff; break;
        case REG_DMA1CNT_L: io_dma1cnt_l = value & 0x3fff; io_dma1cnt_h = (value >> 16) & 0xf7e0; break;
        case REG_DMA2SAD_L: io_dma2sad = value & 0x0fffffff; break;
        case REG_DMA2DAD_L: io_dma2dad = value & 0x07ffffff; break;
        case REG_DMA2CNT_L: io_dma2cnt_l = value & 0x3fff; io_dma2cnt_h = (value >> 16) & 0xf7e0; break;
        case REG_DMA3SAD_L: io_dma3sad = value & 0x0fffffff; break;
        case REG_DMA3DAD_L: io_dma3dad = value & 0x0fffffff; break;
        case REG_DMA3CNT_L: io_dma3cnt_l = value & 0xffff; io_dma3cnt_h = (value >> 16) & 0xffe0; break;
        case 0xe0: break;
        case 0xe4: break;
        case 0xe8: break;
        case 0xec: break;
        case 0xf0: break;
        case 0xf4: break;
        case 0xf8: break;
        case 0xfc: break;
        case 0x100c: break;

        case REG_TM0CNT_L:
            timer_0_reload = (uint16_t) value;
            timer_0_control = (uint16_t)(value >> 16);
            if (timer_0_control & 0x80) timer_0_counter = timer_0_reload;
            break;

        case REG_TM1CNT_L:
            timer_1_reload = (uint16_t) value;
            timer_1_control = (uint16_t)(value >> 16);
            if (timer_1_control & 0x80) timer_1_counter = timer_1_reload;
            break;

        case REG_TM2CNT_L:
            timer_2_reload = (uint16_t) value;
            timer_2_control = (uint16_t)(value >> 16);
            if (timer_2_control & 0x80) timer_2_counter = timer_2_reload;
            break;

        case REG_TM3CNT_L:
            timer_3_reload = (uint16_t) value;
            timer_3_control = (uint16_t)(value >> 16);
            if (timer_3_control & 0x80) timer_3_counter = timer_3_reload;
            break;

        case REG_IE:
            io_ie = (uint16_t) value;
            io_if &= ~(uint16_t)(value >> 16);
            break;

        case REG_WAITCNT:
            io_waitcnt = (uint16_t) value;
            break;

        case REG_IME:
            io_ime = (uint16_t) value;
            break;

        default:
#ifdef LOG_BAD_MEMORY_ACCESS
            printf("io_write_word(0x%08x, 0x%08x);\n", address, value);
#endif
            break;
    }
}

#define MANUFACTURER_ATMEL     0x1f
#define DEVICE_AT29LV512       0x3d  // 512 Kbit
//#define DEVICE_AT29LV010     0x35  // 1 Mbit
//#define DEVICE_AT29LV020     0xba  // 2 Mbit
//#define DEVICE_AT29LV040     0x3b  // 4 Mbit

#define MANUFACTURER_PANASONIC 0x32
#define DEVICE_MN63F805MNP     0x1b  // 512 Kbit

#define MANUFACTURER_SANYO     0x62
#define DEVICE_LE26FV10N1TS    0x13  // 1 Mbit

#define MANUFACTURER_SST       0xbf
#define DEVICE_SST39VF512      0xd4  // 512 Kbit
//#define DEVICE_SST39VF010    0xd5  // 1 Mbit
//#define DEVICE_SST39VF020    0xd6  // 2 Mbit
//#define DEVICE_SST39VF040    0xd7  // 4 Mbit
//#define DEVICE_SST39VF080    0xd8  // 8 Mbit

#define MANUFACTURER_MACRONIX  0xc2
#define DEVICE_MX29L512        0x1c  // 512 Kbit
#define DEVICE_MX29L010        0x09  // 1 Mbit

uint32_t flash_bank = 0;
uint32_t flash_state = 0;
bool flash_id = false;
uint8_t flash_manufacturer = 0;
uint8_t flash_device = 0;

uint8_t backup_read_byte(uint32_t address) {
    //printf("backup_read_byte(0x%08x);\n", address);  // FIXME
    if (has_eeprom) {
        assert(false);
    } else if (has_flash) {
        flash_state &= ~7;
        if (flash_id) {
            if (address == 0) return flash_manufacturer;
            if (address == 1) return flash_device;
            assert(false);
            //return 0xff;
        }
        return backup_flash[flash_bank * 0x10000 + address];
    } else if (has_sram) {
        return backup_sram[address];
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    printf("backup_read_byte(0x%08x);\n", address);
#endif
    return 0xff;  // FIXME? 0
}

void backup_write_byte(uint32_t address, uint8_t value) {
    //printf("backup_write_byte(%08X, %02X);\n", address, value);  // FIXME
    if (has_eeprom) {
        assert(false);
    } else if (has_flash) {
        switch (flash_state) {
            case 0:
            case 4:
            case 8:
                if (address == 0x5555 && value == 0xaa) { flash_state++; break; }
                if (value != 0xf0) assert(false);
                flash_state &= ~7;
                break;

            case 1:
            case 5:
            case 9:
                if (address == 0x2aaa && value == 0x55) { flash_state++; break; }
                assert(false);
                flash_state &= ~7;
                break;

            case 2:
            case 6:
            case 10:
                if ((flash_state & ~3) == 0) {  // Normal mode
                    if (address == 0x5555 && value == 0x80) { flash_state = 4; break; }
                    if (address == 0x5555 && value == 0x90) { flash_state = 8; flash_id = true; break; }
                    if (address == 0x5555 && value == 0xa0) { flash_state = 3; break; }
                    if (address == 0x5555 && value == 0xb0) { flash_state = 7; break; }
                    assert(false);
                }
                if (flash_state & 4) {  // Erase mode
                    if (address == 0x5555 && value == 0x10) {  // Chip erase
                        printf("Chip erase\n");
                        memset(backup_flash, 0xff, sizeof(uint8_t) * sizeof(backup_flash));
                        break;
                    }
                    if (value == 0x30) {  // Sector erase
                        uint32_t sector = address >> 12;
                        printf("Sector erase, sector = %d\n", sector);
                        memset(&backup_flash[flash_bank * 0x10000 + sector * 0x1000], 0xff, sizeof(uint8_t) * 0x1000);
                        break;
                    }
                    assert(false);
                }
                if (flash_state & 8) {  // Software ID mode
                    if (address == 0x5555 && value == 0xf0) { flash_state = 0; flash_id = false; break; }
                    assert(false);
                }
                assert(false);
                flash_state &= ~7;
                break;

            case 3:  // Byte program
                backup_flash[flash_bank * 0x10000 + address] = value;
                flash_state = 0;
                break;

            case 7:  // Bank switch
                assert(address == 0);
                assert(value == 0 || value == 1);
                flash_bank = value & 1;
                flash_state = 0;
                break;

            default:
                abort();
        }
        return;
    } else if (has_sram) {
        backup_sram[address] = value;
        return;
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    printf("backup_write_byte(0x%08x, 0x%02x);\n", address, value);
#endif
}

uint16_t backup_read_halfword(uint32_t address) {
    uint16_t value = backup_read_byte(address);
    return value | value << 8;
}

void backup_write_halfword(uint32_t address, uint16_t value) {
    backup_write_byte(address, (uint8_t)(value >> 8 * (address & 1)));
}

uint32_t backup_read_word(uint32_t address) {
    uint32_t value = backup_read_byte(address);
    return value | value << 8 | value << 16 | value << 24;
}

void backup_write_word(uint32_t address, uint32_t value) {
    backup_write_byte(address, (uint8_t)(value >> 8 * (address & 3)));
}

uint8_t memory_read_byte(uint32_t address) {
    if (address < 0x4000) {
        if (r[15] < 0x4000) last_bios_access = address;
        return system_rom[last_bios_access];
    }
    if (address >= 0x02000000 && address < 0x03000000) {
        return cpu_ewram[address & 0x3ffff];
    }
    if (address >= 0x03000000 && address < 0x04000000) {
        return cpu_iwram[address & 0x7fff];
    }
    if (address >= 0x04000000 && address < 0x05000000) {
        return io_read_byte(address - 0x4000000);
    }
    if (address >= 0x05000000 && address < 0x06000000) {
        return palette_ram[address & 0x3ff];
    }
    if (address >= 0x06000000 && address < 0x07000000) {
        address &= 0x1ffff;
        if (address >= 0x18000) address -= 0x18000;        
        return video_ram[address];
    }
    if (address >= 0x07000000 && address < 0x08000000) {
        return object_ram[address & 0x3ff];
    }    
    if (address >= 0x08000000 && address < 0x0e000000) {
        return game_rom[address & game_rom_mask];
    }
    if (address >= 0x0e000000 && address < 0x10000000) {
        return backup_read_byte(address & 0xffff);
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    printf("memory_read_byte(0x%08x);\n", address);
#endif
    return 0;
}

void memory_write_byte(uint32_t address, uint8_t value) {
    if (address < 0x4000) {
        //return;  // Read only
    }
    if (address >= 0x02000000 && address < 0x03000000) {
        cpu_ewram[address & 0x3ffff] = value;
        return;
    }
    if (address >= 0x03000000 && address < 0x04000000) {
        cpu_iwram[address & 0x7fff] = value;
        return;
    }
    if (address >= 0x04000000 && address < 0x05000000) {
        io_write_byte(address - 0x4000000, value);
        return;
    }
    if (address >= 0x05000000 && address < 0x06000000) {
        *(uint16_t *)&palette_ram[address & 0x3ff] = value | value << 8;
        return;
    }
    if (address >= 0x06000000 && address < 0x07000000) {
        address &= 0x1ffff;
        if (address >= 0x18000) address -= 0x18000;        
        *(uint16_t *)&video_ram[address] = value | value << 8;  // FIXME ignored sometimes?
        return;
    }
    if (address >= 0x07000000 && address < 0x08000000) {
        *(uint16_t *)&object_ram[address & 0x3ff] = value | value << 8;  // FIXME ignored?
        return;
    }
    if (address >= 0x08000000 && address < 0x0e000000) {
        //return;  // Read only
    }
    if (address >= 0x0e000000 && address < 0x10000000) {
        backup_write_byte(address & 0xffff, value);
        return;
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    printf("memory_write_byte(0x%08x, 0x%02x);\n", address, value);
#endif
}

uint16_t memory_read_halfword(uint32_t address) {
    if (address < 0x4000) {
        if (r[15] < 0x4000) last_bios_access = address & 0x3ffe;
        return *(uint16_t *)&system_rom[last_bios_access];
    }
    if (address >= 0x02000000 && address < 0x03000000) {
        return *(uint16_t *)&cpu_ewram[address & 0x3fffe];
    }
    if (address >= 0x03000000 && address < 0x04000000) {
        return *(uint16_t *)&cpu_iwram[address & 0x7ffe];
    }
    if (address >= 0x04000000 && address < 0x05000000) {
        return io_read_halfword((address - 0x4000000) & ~1);
    }
    if (address >= 0x05000000 && address < 0x06000000) {
        return *(uint16_t *)&palette_ram[address & 0x3fe];
    }
    if (address >= 0x06000000 && address < 0x07000000) {
        address &= 0x1fffe;
        if (address >= 0x18000) address -= 0x18000;
        return *(uint16_t *)&video_ram[address];
    }
    if (address >= 0x07000000 && address < 0x08000000) {
        return *(uint16_t *)&object_ram[address & 0x3fe];
    }
    if (address >= 0x08000000 && address < 0x0e000000) {
        return *(uint16_t *)&game_rom[address & (game_rom_mask & ~1)];
    }
    if (address >= 0x0e000000 && address < 0x10000000) {
        return backup_read_halfword(address & 0xffff);
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    printf("memory_read_halfword(0x%08x);\n", address);
#endif
    return 0;
}

void memory_write_halfword(uint32_t address, uint16_t value) {
    if (address < 0x4000) {
        //return;  // Read only
    }
    if (address >= 0x02000000 && address < 0x03000000) {
        *(uint16_t *)&cpu_ewram[address & 0x3fffe] = value;
        return;
    }
    if (address >= 0x03000000 && address < 0x04000000) {
        *(uint16_t *)&cpu_iwram[address & 0x7ffe] = value;
        return;
    }
    if (address >= 0x04000000 && address < 0x05000000) {
        io_write_halfword((address - 0x4000000) & ~1, value);
        return;
    }
    if (address >= 0x05000000 && address < 0x06000000) {
        *(uint16_t *)&palette_ram[address & 0x3fe] = value;
        return;
    }
    if (address >= 0x06000000 && address < 0x07000000) {
        address &= 0x1fffe;
        if (address >= 0x18000) address -= 0x18000;
        *(uint16_t *)&video_ram[address] = value;
        return;
    }
    if (address >= 0x07000000 && address < 0x08000000) {
        *(uint16_t *)&object_ram[address & 0x3fe] = value;
        return;
    }
    if (address >= 0x08000000 && address < 0x0e000000) {
        //return;  // Read only
    }
    if (address >= 0x0e000000 && address < 0x10000000) {
        backup_write_halfword(address & 0xffff, value);
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    printf("memory_write_halfword(0x%08x, 0x%04x);\n", address, value);
#endif
}

uint32_t memory_read_word(uint32_t address) {
   if (address < 0x4000) {
        if (r[15] < 0x4000) last_bios_access = address & 0x3ffc;
        return *(uint32_t *)&system_rom[last_bios_access];
    }
    if (address < 0x02000000) {
        //return 0;  // Unmapped
    }
    if (address >= 0x02000000 && address < 0x03000000) {
        return *(uint32_t *)&cpu_ewram[address & 0x3fffc];
    }
    if (address >= 0x03000000 && address < 0x04000000) {
        return *(uint32_t *)&cpu_iwram[address & 0x7ffc];
    }
    if (address >= 0x04000000 && address < 0x05000000) {
        return io_read_word((address - 0x4000000) & ~3);
    }
    if (address >= 0x05000000 && address < 0x06000000) {
        return *(uint32_t*)&palette_ram[address & 0x3fc];
    }
    if (address >= 0x06000000 && address < 0x07000000) {
        address &= 0x1fffc;
        if (address >= 0x18000) address -= 0x18000;
        return *(uint32_t*)&video_ram[address];
    }
    if (address >= 0x07000000 && address < 0x08000000) {
        return *(uint32_t *)&object_ram[address & 0x3fc];
    }
    if (address >= 0x08000000 && address < 0x0e000000) {
        return *(uint32_t *)&game_rom[address & (game_rom_mask & ~3)];
    }
    if (address >= 0x0e000000 && address < 0x10000000) {
        return backup_read_word(address & 0xffff);
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    printf("memory_read_word(0x%08x);\n", address);
#endif
    return 0;
}

void memory_write_word(uint32_t address, uint32_t value) {
    if (address < 0x4000) {
        //return;  // Read only
    }
    if (address < 0x02000000) {
        //return;  // Unmapped
    }
    if (address >= 0x02000000 && address < 0x03000000) {
        *(uint32_t *)&cpu_ewram[address & 0x3fffc] = value;
        return;
    }
    if (address >= 0x03000000 && address < 0x04000000) {
        *(uint32_t *)&cpu_iwram[address & 0x7ffc] = value;
        return;
    }
    if (address >= 0x04000000 && address < 0x05000000) {
        io_write_word((address - 0x4000000) & ~3, value);
        return;
    }
    if (address >= 0x05000000 && address < 0x06000000) {
        *(uint32_t *)&palette_ram[address & 0x3fc] = value;
        return;
    }
    if (address >= 0x06000000 && address < 0x07000000) {
        address &= 0x1fffc;
        if (address >= 0x18000) address -= 0x18000;
        *(uint32_t *)&video_ram[address] = value;
        return;
    }
    if (address >= 0x07000000 && address < 0x08000000) {
        *(uint32_t *)&object_ram[address & 0x3fc] = value;
        return;
    }
    if (address >= 0x08000000 && address < 0x0e000000) {
        //return;  // Read only
    }
    if (address >= 0x0e000000 && address < 0x10000000) {
        backup_write_word(address & 0xffff, value);
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    printf("memory_write_word(0x%08x, 0x%08x);\n", address, value);
#endif
}

void gba_detect_cartridge_features(void) {
    void *match;

    has_eeprom = false;
    has_flash = false;
    has_sram = false;

    uint8_t *eeprom_v = (uint8_t *) "EEPROM_V";
    match = boyer_moore_matcher(game_rom, game_rom_size, eeprom_v, 8);
    if (match) { has_eeprom = true; }

    uint8_t *flash_v = (uint8_t *) "FLASH_V";
    match = boyer_moore_matcher(game_rom, game_rom_size, flash_v, 7);
    if (match) { has_flash = true; flash_manufacturer = MANUFACTURER_PANASONIC; flash_device = DEVICE_MN63F805MNP; }

    uint8_t *flash512_v = (uint8_t *) "FLASH512_V";
    match = boyer_moore_matcher(game_rom, game_rom_size, flash512_v, 10);
    if (match) { has_flash = true; flash_manufacturer = MANUFACTURER_PANASONIC; flash_device = DEVICE_MN63F805MNP; }

    uint8_t *flash1m_v = (uint8_t *) "FLASH1M_V";
    match = boyer_moore_matcher(game_rom, game_rom_size, flash1m_v, 9);
    if (match) { has_flash = true; flash_manufacturer = MANUFACTURER_SANYO; flash_device = DEVICE_LE26FV10N1TS; }

    uint8_t *sram_v = (uint8_t *) "SRAM_V";
    match = boyer_moore_matcher(game_rom, game_rom_size, sram_v, 6);
    if (match) { has_sram = true; }

    uint8_t *sram_f_v = (uint8_t *) "SRAM_F_V";
    match = boyer_moore_matcher(game_rom, game_rom_size, sram_f_v, 8);
    if (match) { has_sram = true; }  // FIXME?

    printf("has_eeprom = %s\n", (has_eeprom ? "true" : "false"));
    printf("has_flash = %s\n", (has_flash ? "true" : "false"));
    printf("has_sram = %s\n", (has_sram ? "true" : "false"));
}

void gba_init(const char *filename) {
    arm_init_lookup();
    thumb_init_lookup();

    FILE *fp = fopen("system_rom.bin", "rb");
    assert(fp != NULL);
    fread(system_rom, sizeof(uint8_t), 0x4000, fp);
    fclose(fp);

    fp = fopen(filename, "rb");
    assert(fp != NULL);
    fseek(fp, 0, SEEK_END);
    game_rom_size = ftell(fp);
    assert(game_rom_size > 0);
    game_rom_mask = next_power_of_2(game_rom_size) - 1;
    fseek(fp, 0, SEEK_SET);
    fread(game_rom, sizeof(uint8_t), game_rom_size, fp);
    fclose(fp);

    memset(r, 0, sizeof(uint32_t) * 16);

    gba_detect_cartridge_features();
    memset(backup_flash, 0xff, sizeof(uint8_t) * sizeof(backup_flash));
    memset(backup_sram, 0xff, sizeof(uint8_t) * sizeof(backup_sram));

    arm_init_registers(skip_bios);

    branch_taken = true;
    io_vcount = 227;
}

SDL_Texture *g_texture;
SDL_PixelFormat *g_format;
Uint32 *g_pixels;
int g_pitch;

Uint32 rgb555(uint32_t pixel) {
    uint32_t r, g, b;
    r = pixel & 0x1f;
    g = (pixel >> 5) & 0x1f;
    b = (pixel >> 10) & 0x1f;
    r = (r << 3) | (r >> 2);
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);
    return SDL_MapRGB(g_format, r, g, b);
}

void gba_draw_blank(int y) {
    Uint32 clear_color = rgb555(0);

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        g_pixels[y * (g_pitch / 4) + x] = clear_color;
    }
}

void gba_draw_bitmap(uint32_t mode, int y) {
    int width = (mode == 5 ? 160 : SCREEN_WIDTH);

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        uint16_t pixel = 0;
        if (mode == 3 || mode == 5) {
            if (x < width && (mode == 3 || y < 128)) {
                pixel = *(uint16_t *)&video_ram[(y * width + x) * 2];
            } else {
                pixel = *(uint16_t *)&palette_ram[0];
            }
        } else if (mode == 4) {
            bool pflip = (io_dispcnt & DCNT_PAGE) != 0;
            uint8_t pixel_index = video_ram[(pflip ? 0xa000 : 0) + y * SCREEN_WIDTH + x];
            pixel = *(uint16_t *)&palette_ram[pixel_index * 2];
        }
        g_pixels[y * (g_pitch / 4) + x] = rgb555(pixel);
    }
}

void gba_draw_tiled_cull(int x, int y, int h, uint32_t pixel) {
    x -= h;
    if (x < 0 || x >= SCREEN_WIDTH) return;
    g_pixels[y * (g_pitch / 4) + x] = rgb555(pixel);
}

void gba_draw_tiled_bg(uint32_t mode, int y, uint32_t bgcnt, uint32_t hofs, uint32_t vofs) {
    //assert(mode == 0);  // FIXME

    uint32_t tile_base = ((bgcnt >> 2) & 3) * 16384;
    uint32_t map_base = ((bgcnt >> 8) & 0x1f) * 2048;
    //uint32_t screen_size = (bgcnt >> 14) & 3;
    bool colors_256 = (bgcnt & (1 << 7)) != 0;

    uint32_t hofs_div_8 = hofs / 8;
    uint32_t vofs_div_8 = vofs / 8;
    uint32_t hofs_rem_8 = hofs % 8;
    uint32_t vofs_rem_8 = vofs % 8;
    uint32_t yv = y + vofs_rem_8;

    for (int x = 0; x < 256; x += 8) {
        uint32_t map_x = (x / 8 + hofs_div_8) % 32;
        uint32_t map_y = (yv / 8 + vofs_div_8) % 32;
        uint32_t map_address = map_base + (map_y * 32 + map_x) * 2;
        uint16_t info = *(uint16_t *)&video_ram[map_address];
        uint16_t tile_no = info & 0x3ff;
        bool hflip = (info & (1 << 10)) != 0;
        bool vflip = (info & (1 << 11)) != 0;
        uint16_t palette_no = (info >> 12) & 0xf;

        uint32_t tile_address = tile_base + tile_no * (colors_256 ? 64 : 32);
        if (tile_address >= 0x10000) continue;
        uint8_t *tile = &video_ram[tile_address];
        if (colors_256) {
            for (int i = 0; i < 8; i++) {
                uint32_t offset = (vflip ? 7 - (yv % 8) : (yv % 8)) * 8 + (hflip ? 7 - i : i);
                uint8_t pixel_index = tile[offset];
                if (pixel_index != 0) {
                    uint16_t pixel = *(uint16_t *)&palette_ram[pixel_index * 2];
                    gba_draw_tiled_cull((x / 8) * 8 + i, y, hofs_rem_8, pixel);
                }
            }
        } else {
            for (int i = 0; i < 8; i += 2) {
                uint32_t offset = (vflip ? 7 - (yv % 8) : (yv % 8)) * 4 + (hflip ? 7 - i : i) / 2;
                uint8_t pixel_indexes = tile[offset];
                uint8_t pixel_index_0 = (pixel_indexes >> (hflip ? 4 : 0)) & 0xf;
                uint8_t pixel_index_1 = (pixel_indexes >> (hflip ? 0 : 4)) & 0xf;
                if (pixel_index_0 != 0) {
                    uint16_t pixel_0 = *(uint16_t *)&palette_ram[palette_no * 32 + pixel_index_0 * 2];
                    gba_draw_tiled_cull((x / 8) * 8 + i, y, hofs_rem_8, pixel_0);
                }
                if (pixel_index_1 != 0) {
                    uint16_t pixel_1 = *(uint16_t *)&palette_ram[palette_no * 32 + pixel_index_1 * 2];
                    gba_draw_tiled_cull((x / 8) * 8 + i + 1, y, hofs_rem_8, pixel_1);
                }
            }
        }
    }
}

void gba_draw_tiled(uint32_t mode, int y) {
    for (int pri = 3; pri >= 0; pri--) {  // FIXME? reverse order
        for (int bg = 0; bg < 4; bg++) {
            bool visible = (io_dispcnt & (1 << (8 + bg))) != 0;
            if (!visible) continue;
            uint16_t bgcnt = io_read_halfword(REG_BG0CNT + 2 * bg);
            uint16_t priority = bgcnt & 3;
            if (priority == pri) {
                uint16_t hofs, vofs;
                switch (bg) {
                    case 0: hofs = io_bg0hofs; vofs = io_bg0vofs; break;
                    case 1: hofs = io_bg1hofs; vofs = io_bg1vofs; break;
                    case 2: hofs = io_bg2hofs; vofs = io_bg2vofs; break;
                    case 3: hofs = io_bg3hofs; vofs = io_bg3vofs; break;
                    default: abort();
                }
                //if (hofs & 0x8000) hofs |= ~0xffff;
                //if (vofs & 0x8000) vofs |= ~0xffff;
                gba_draw_tiled_bg(mode, y, bgcnt, hofs, vofs);
            }
        }
    }
}

void gba_draw_scanline(void) {
    gba_draw_blank(io_vcount);  // FIXME forced blank

    uint32_t mode = io_dispcnt & 7;
    switch (mode) {
        case 0:
        case 1:
        case 2:
        //case 6:
        //case 7:
            gba_draw_tiled(mode, io_vcount);
            break;

        case 3:
        case 4:
        case 5:
            gba_draw_bitmap(mode, io_vcount);
            break;

        default:
            assert(false);
            break;
    }
}

void gba_ppu_update(void) {
    if (ppu_cycles % 1232 == 0) {
        io_dispstat &= ~DSTAT_IN_HBL;
        if (io_vcount < SCREEN_HEIGHT) {
            gba_draw_scanline();
        }
        io_vcount = (io_vcount + 1) % 228;
        if (io_vcount == 0) {
            io_dispstat &= ~DSTAT_IN_VBL;
        } else if (io_vcount == 160) {
            io_dispstat |= DSTAT_IN_VBL;
            if ((io_dispstat & DSTAT_VBL_IRQ) != 0 && io_ime == 1 && (io_ie & INT_VBLANK) != 0) {
                io_if |= INT_VBLANK;
            }
        }
        if (io_vcount == (uint8_t)(io_dispstat >> 8)) {
            io_dispstat |= DSTAT_IN_VCT;
            if ((io_dispstat & DSTAT_VCT_IRQ) != 0 && io_ime == 1 && (io_ie & INT_VCOUNT) != 0) {
                io_if |= INT_VCOUNT;
            }
        } else {
            io_dispstat &= ~DSTAT_IN_VCT;
        }
    }
    if (ppu_cycles % 1232 == 960) {
        io_dispstat |= DSTAT_IN_HBL;
        if ((io_dispstat & DSTAT_HBL_IRQ) != 0 && io_ime == 1 && (io_ie & INT_HBLANK) != 0) {
            io_if |= INT_HBLANK;
        }
    }
    ppu_cycles = (ppu_cycles + 1) % 280896;
}

void gba_timer_update(void) {
    timer_cycles = (timer_cycles + 1) % 1024;
    bool last_increment = false;
    for (int i = 0; i < 4; i++) {
        uint16_t *counter, *reload, *control;
        switch (i) {
            case 0: counter = &timer_0_counter; reload = &timer_0_reload; control = &timer_0_control; break;
            case 1: counter = &timer_1_counter; reload = &timer_1_reload; control = &timer_1_control; break;
            case 2: counter = &timer_2_counter; reload = &timer_2_reload; control = &timer_2_control; break;
            case 3: counter = &timer_3_counter; reload = &timer_3_reload; control = &timer_3_control; break;
            default: abort();
        }
        if (*control & (1 << 7)) {
            bool increment = false;
            if (*control & (1 << 2)) {
                increment = last_increment;
            } else {
                uint32_t prescaler = *control & 3;
                switch (prescaler) {
                    case 0: increment = true; break;
                    case 1: increment = (ppu_cycles % 64) == 0; break;
                    case 2: increment = (ppu_cycles % 256) == 0; break;
                    case 3: increment = (ppu_cycles % 1024) == 0; break;
                    default: abort();
                }
            }
            if (increment) {
                *counter = *counter + 1;
                if (*counter == 0) {
                    *counter = *reload;
                    if ((*control & (1 << 6)) != 0 && io_ime == 1 && (io_ie & (1 << (3 + i))) != 0) {
                        io_if |= 1 << (3 + i);
                    }
                    last_increment = true;
                } else {
                    last_increment = false;
                }
            }
        }
    }
}

void gba_dma_transfer_halfwords(uint32_t dst_ctrl, uint32_t src_ctrl, uint32_t *dst_addr, uint32_t *src_addr, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        uint16_t value = memory_read_halfword(*src_addr);
        memory_write_halfword(*dst_addr, value);
        if (dst_ctrl == DMA_INC || dst_ctrl == DMA_RELOAD) *dst_addr += 2;
        else if (dst_ctrl == DMA_DEC) *dst_addr -= 2;
        if (src_ctrl == DMA_INC) *src_addr += 2;
        else if (src_ctrl == DMA_DEC) *src_addr -= 2;
    }
}

void gba_dma_transfer_words(uint32_t dst_ctrl, uint32_t src_ctrl, uint32_t *dst_addr, uint32_t *src_addr, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        uint32_t value = memory_read_word(*src_addr);
        memory_write_word(*dst_addr, value);
        if (dst_ctrl == DMA_INC || dst_ctrl == DMA_RELOAD) *dst_addr += 4;
        else if (dst_ctrl == DMA_DEC) *dst_addr -= 4;
        if (src_ctrl == DMA_INC) *src_addr += 4;
        else if (src_ctrl == DMA_DEC) *src_addr -= 4;
    }
}

void gba_dma_update(void) {
    for (int ch = 0; ch < 4; ch++) {
        uint32_t dmacnt, *dst_addr, *src_addr;
        switch (ch) {
            case 0: dmacnt = io_dma0cnt_l | io_dma0cnt_h << 16; dst_addr = &io_dma0dad; src_addr = &io_dma0sad; break;
            case 1: dmacnt = io_dma1cnt_l | io_dma1cnt_h << 16; dst_addr = &io_dma1dad; src_addr = &io_dma1sad; break;
            case 2: dmacnt = io_dma2cnt_l | io_dma2cnt_h << 16; dst_addr = &io_dma2dad; src_addr = &io_dma2sad; break;
            case 3: dmacnt = io_dma3cnt_l | io_dma3cnt_h << 16; dst_addr = &io_dma3dad; src_addr = &io_dma3sad; break;
            default: abort();
        }

        if ((dmacnt & DMA_ENABLE) == 0) continue;

        uint32_t start_timing = (dmacnt >> 28) & 3;
        uint32_t dst_ctrl = (dmacnt >> 21) & 3;
        uint32_t src_ctrl = (dmacnt >> 23) & 3;
        bool transfer_word = (dmacnt & DMA_32) != 0;
        uint32_t count = dmacnt & 0xffff;
        if (count == 0) count = (ch == 3 ? 0x10000 : 0x4000);

        if (start_timing == DMA_AT_VBLANK && io_vcount != 160) continue;
        if (start_timing == DMA_AT_HBLANK && ppu_cycles % 1232 != 960) continue;
        if (start_timing == DMA_AT_REFRESH) {
            if (ch == 1 || ch == 2) {
                assert(*dst_addr == 0x40000a0 || *dst_addr == 0x40000a4);
                assert((dmacnt & DMA_REPEAT) != 0);
                if (*dst_addr == 0x40000a0 && !fifo_a_refill) continue;
                if (*dst_addr == 0x40000a4 && !fifo_b_refill) continue;
                dst_ctrl = DMA_FIXED;
                transfer_word = true;
                count = 4;
            } else if (ch == 3) {
                if (ppu_cycles % 1232 != 0) continue;
            } else {
                assert(false);
            }
        }

        assert(src_ctrl != DMA_RELOAD);
        assert((dmacnt & DMA_DRQ) == 0);

        uint32_t dst_addr_initial = *dst_addr;

        if (transfer_word) {
            gba_dma_transfer_words(dst_ctrl, src_ctrl, dst_addr, src_addr, count);
        } else {
            gba_dma_transfer_halfwords(dst_ctrl, src_ctrl, dst_addr, src_addr, count);
        }

        if (dst_ctrl == DMA_RELOAD) {
            *dst_addr = dst_addr_initial;
        }

        if ((dmacnt & DMA_IRQ) != 0 && io_ime == 1 && (io_ie & (1 << (8 + ch))) != 0) {
            io_if |= 1 << (8 + ch);
        }

        if ((dmacnt & DMA_REPEAT) != 0) continue;

        dmacnt &= ~DMA_ENABLE;
        io_write_word(REG_DMA0CNT_L + 12 * ch, dmacnt);
    }
}

void gba_emulate(void) {
    while (true) {
        gba_timer_update();
        gba_dma_update();
        gba_ppu_update();
        if (ppu_cycles == 0) break;

        int cpu_cycles = 1;

        if (!halted) {
            //if (r[15] == 0x00000300) single_step = true;

#ifdef DEBUG
            if (single_step && instruction_count >= start_logging_at) {
                log_instructions = true;
                log_registers = true;
            }
#endif

            if (FLAG_T()) {
                cpu_cycles = thumb_step();
            } else {
                cpu_cycles = arm_step();
            }
            assert(cpu_cycles == 1);
            instruction_count++;

#ifdef DEBUG
            if (single_step && instruction_count > start_logging_at) {
                char c = fgetc(stdin);
                if (c == EOF) exit(EXIT_SUCCESS);
            }
            /*
            if (instruction_count == end_logging_at) {
                exit(EXIT_SUCCESS);
            }
            */
#endif
        }

        if (!branch_taken && (cpsr & PSR_I) == 0 && io_if != 0) {
            arm_hardware_interrupt();
            halted = false;
        }
    }
}

uint32_t fps_ticks_last = 0;
double fps_diff = 0;

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s filename.gba\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    gba_init(argv[1]);

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow("ygba", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, RENDER_WIDTH, RENDER_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    Uint32 pixel_format = SDL_GetWindowPixelFormat(window);
    g_texture = SDL_CreateTexture(renderer, pixel_format, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    g_format = SDL_AllocFormat(pixel_format);

    bool quit = false;
    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
        }

        const Uint8 *state = SDL_GetKeyboardState(NULL);
        if (state[SDL_SCANCODE_ESCAPE]) {
            quit = true;
        }
        keys[0] = state[SDL_SCANCODE_X];          // Button A
        keys[1] = state[SDL_SCANCODE_Z];          // Button B
        keys[2] = state[SDL_SCANCODE_BACKSPACE];  // Select
        keys[3] = state[SDL_SCANCODE_RETURN];     // Start
        keys[4] = state[SDL_SCANCODE_RIGHT];      // Right
        keys[5] = state[SDL_SCANCODE_LEFT];       // Left
        keys[6] = state[SDL_SCANCODE_UP];         // Up
        keys[7] = state[SDL_SCANCODE_DOWN];       // Down
        keys[8] = state[SDL_SCANCODE_S];          // Button R
        keys[9] = state[SDL_SCANCODE_A];          // Button L
        if (keys[4] && keys[5]) {  // Disallow opposing directions
            keys[4] = false;
            keys[5] = false;
        }
        if (keys[6] && keys[7]) {
            keys[6] = false;
            keys[7] = false;
        }

        SDL_LockTexture(g_texture, NULL, (void**) &g_pixels, &g_pitch);
        gba_emulate();
        SDL_UnlockTexture(g_texture);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);
        SDL_Rect displayRect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_Rect screenRect = {0, 0, RENDER_WIDTH, RENDER_HEIGHT};
        SDL_RenderCopy(renderer, g_texture, &displayRect, &screenRect);
        SDL_RenderPresent(renderer);

        static char title[256];
        uint32_t t0 = fps_ticks_last;
        uint32_t t1 = SDL_GetTicks();
        fps_diff = fps_diff * 0.975 + (t1 - t0) * 0.025;
        double fps = 1 / (fps_diff / 1000.0);
        sprintf(title, "ygba - %s (%.1f fps)", argv[1], fps);
        SDL_SetWindowTitle(window, title);
        fps_ticks_last = t1;
    }

    SDL_FreeFormat(g_format);
    SDL_DestroyTexture(g_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
