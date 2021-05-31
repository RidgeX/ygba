#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <SDL.h>

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
// About Desktop OpenGL function loaders:
//  Modern desktop OpenGL doesn't have a standard portable header file to load OpenGL function pointers.
//  Helper libraries are often used for this purpose! Here we are supporting a few common ones (gl3w, glew, glad).
//  You may use another loader/header of your choice (glext, glLoadGen, etc.), or choose to manually implement your own.
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h>  // Initialize with gl3wInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h>  // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>  // Initialize with gladLoadGL()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD2)
#include <glad/gl.h>  // Initialize with gladLoadGL(...) or gladLoaderLoadGL()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
#define GLFW_INCLUDE_NONE  // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/Binding.h>  // Initialize with glbinding::Binding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
#define GLFW_INCLUDE_NONE  // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/glbinding.h>  // Initialize with glbinding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "algorithms.h"
#include "cpu.h"

#define UNUSED(x) (void)(x)

bool single_step = false;
uint64_t start_logging_at = 0;
//uint64_t end_logging_at = 200000;
int ppu_cycles = 0;
int timer_cycles = 0;
bool halted = false;
uint32_t last_bios_access = 0xe4;
bool skip_bios = false;
bool has_eeprom = false;
bool has_flash = false;
//bool has_rtc = false;
bool has_sram = false;

//#define LOG_BAD_MEMORY_ACCESS

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 160

uint32_t screen_texture;
uint32_t screen_pixels[SCREEN_HEIGHT][SCREEN_WIDTH];
int screen_scale = 3;

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

struct {
    uint16_t io_dispcnt;
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
    uint16_t io_bg2pa;
    uint16_t io_bg2pb;
    uint16_t io_bg2pc;
    uint16_t io_bg2pd;
    uint32_t io_bg2x;  // 32
    uint32_t io_bg2y;  // 32
    uint16_t io_bg3pa;
    uint16_t io_bg3pb;
    uint16_t io_bg3pc;
    uint16_t io_bg3pd;
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
} ioreg;

uint8_t io_read_byte(uint32_t address) {
    switch (address) {
        case REG_DISPCNT + 0: return (uint8_t)(ioreg.io_dispcnt >> 0);
        case REG_DISPCNT + 1: return (uint8_t)(ioreg.io_dispcnt >> 8);
        case 0x2: return 0xad;
        case 0x3: return 0xde;
        case REG_DISPSTAT + 0: return (uint8_t)(ioreg.io_dispstat >> 0);
        case REG_DISPSTAT + 1: return (uint8_t)(ioreg.io_dispstat >> 8);
        case REG_VCOUNT + 0: return (uint8_t)(ioreg.io_vcount >> 0);
        case REG_VCOUNT + 1: return (uint8_t)(ioreg.io_vcount >> 8);
        case REG_BG0CNT + 0: return (uint8_t)(ioreg.io_bg0cnt >> 0);
        case REG_BG0CNT + 1: return (uint8_t)(ioreg.io_bg0cnt >> 8);
        case REG_BG1CNT + 0: return (uint8_t)(ioreg.io_bg1cnt >> 0);
        case REG_BG1CNT + 1: return (uint8_t)(ioreg.io_bg1cnt >> 8);
        case REG_BG2CNT + 0: return (uint8_t)(ioreg.io_bg2cnt >> 0);
        case REG_BG2CNT + 1: return (uint8_t)(ioreg.io_bg2cnt >> 8);
        case REG_BG3CNT + 0: return (uint8_t)(ioreg.io_bg3cnt >> 0);
        case REG_BG3CNT + 1: return (uint8_t)(ioreg.io_bg3cnt >> 8);
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
        case REG_WININ + 0: return (uint8_t)(ioreg.io_winin >> 0);
        case REG_WININ + 1: return (uint8_t)(ioreg.io_winin >> 8);
        case REG_WINOUT + 0: return (uint8_t)(ioreg.io_winout >> 0);
        case REG_WINOUT + 1: return (uint8_t)(ioreg.io_winout >> 8);
        case REG_MOSAIC + 0: return 0xad;
        case REG_MOSAIC + 1: return 0xde;
        case 0x4e: return 0xad;
        case 0x4f: return 0xde;
        case REG_BLDCNT + 0: return (uint8_t)(ioreg.io_bldcnt >> 0);
        case REG_BLDCNT + 1: return (uint8_t)(ioreg.io_bldcnt >> 8);
        case REG_BLDALPHA + 0: return (uint8_t)(ioreg.io_bldalpha >> 0);
        case REG_BLDALPHA + 1: return (uint8_t)(ioreg.io_bldalpha >> 8);
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
        case 0x8c: return 0xad;
        case 0x8d: return 0xde;
        case 0x8e: return 0xad;
        case 0x8f: return 0xde;
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
        case REG_DMA0CNT_H + 0: return (uint8_t)(ioreg.io_dma0cnt_h >> 0);
        case REG_DMA0CNT_H + 1: return (uint8_t)(ioreg.io_dma0cnt_h >> 8);
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
        case REG_DMA1CNT_H + 0: return (uint8_t)(ioreg.io_dma1cnt_h >> 0);
        case REG_DMA1CNT_H + 1: return (uint8_t)(ioreg.io_dma1cnt_h >> 8);
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
        case REG_DMA2CNT_H + 0: return (uint8_t)(ioreg.io_dma2cnt_h >> 0);
        case REG_DMA2CNT_H + 1: return (uint8_t)(ioreg.io_dma2cnt_h >> 8);
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
        case REG_DMA3CNT_H + 0: return (uint8_t)(ioreg.io_dma3cnt_h >> 0);
        case REG_DMA3CNT_H + 1: return (uint8_t)(ioreg.io_dma3cnt_h >> 8);
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

        case REG_TM0CNT_L + 0: return (uint8_t) ioreg.timer_0_counter;
        case REG_TM0CNT_L + 1: return (uint8_t)(ioreg.timer_0_counter >> 8);
        case REG_TM1CNT_L + 0: return (uint8_t) ioreg.timer_1_counter;
        case REG_TM1CNT_L + 1: return (uint8_t)(ioreg.timer_1_counter >> 8);
        case REG_TM2CNT_L + 0: return (uint8_t) ioreg.timer_2_counter;
        case REG_TM2CNT_L + 1: return (uint8_t)(ioreg.timer_2_counter >> 8);
        case REG_TM3CNT_L + 0: return (uint8_t) ioreg.timer_3_counter;
        case REG_TM3CNT_L + 1: return (uint8_t)(ioreg.timer_3_counter >> 8);

        default:
#ifdef LOG_BAD_MEMORY_ACCESS
            printf("io_read_byte(0x%08x);\n", address);
#endif
            return 0;
    }
}

void io_write_byte(uint32_t address, uint8_t value) {
    switch (address) {
        case REG_DISPCNT + 0: ioreg.io_dispcnt = (ioreg.io_dispcnt & 0xff08) | ((value << 0) & 0x00f7); break;
        case REG_DISPCNT + 1: ioreg.io_dispcnt = (ioreg.io_dispcnt & 0x00ff) | ((value << 8) & 0xff00); break;
        case 0x2: break;
        case 0x3: break;
        case REG_DISPSTAT + 0: ioreg.io_dispstat = (ioreg.io_dispstat & 0xff07) | ((value << 0) & 0x0038); break;
        case REG_DISPSTAT + 1: ioreg.io_dispstat = (ioreg.io_dispstat & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_VCOUNT + 0: break;
        case REG_VCOUNT + 1: break;
        case REG_BG0CNT + 0: ioreg.io_bg0cnt = (ioreg.io_bg0cnt & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG0CNT + 1: ioreg.io_bg0cnt = (ioreg.io_bg0cnt & 0x00ff) | ((value << 8) & 0xdf00); break;
        case REG_BG1CNT + 0: ioreg.io_bg1cnt = (ioreg.io_bg1cnt & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG1CNT + 1: ioreg.io_bg1cnt = (ioreg.io_bg1cnt & 0x00ff) | ((value << 8) & 0xdf00); break;
        case REG_BG2CNT + 0: ioreg.io_bg2cnt = (ioreg.io_bg2cnt & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG2CNT + 1: ioreg.io_bg2cnt = (ioreg.io_bg2cnt & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_BG3CNT + 0: ioreg.io_bg3cnt = (ioreg.io_bg3cnt & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG3CNT + 1: ioreg.io_bg3cnt = (ioreg.io_bg3cnt & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_BG0HOFS + 0: ioreg.io_bg0hofs = (ioreg.io_bg0hofs & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG0HOFS + 1: ioreg.io_bg0hofs = (ioreg.io_bg0hofs & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_BG0VOFS + 0: ioreg.io_bg0vofs = (ioreg.io_bg0vofs & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG0VOFS + 1: ioreg.io_bg0vofs = (ioreg.io_bg0vofs & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_BG1HOFS + 0: ioreg.io_bg1hofs = (ioreg.io_bg1hofs & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG1HOFS + 1: ioreg.io_bg1hofs = (ioreg.io_bg1hofs & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_BG1VOFS + 0: ioreg.io_bg1vofs = (ioreg.io_bg1vofs & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG1VOFS + 1: ioreg.io_bg1vofs = (ioreg.io_bg1vofs & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_BG2HOFS + 0: ioreg.io_bg2hofs = (ioreg.io_bg2hofs & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG2HOFS + 1: ioreg.io_bg2hofs = (ioreg.io_bg2hofs & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_BG2VOFS + 0: ioreg.io_bg2vofs = (ioreg.io_bg2vofs & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG2VOFS + 1: ioreg.io_bg2vofs = (ioreg.io_bg2vofs & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_BG3HOFS + 0: ioreg.io_bg3hofs = (ioreg.io_bg3hofs & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG3HOFS + 1: ioreg.io_bg3hofs = (ioreg.io_bg3hofs & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_BG3VOFS + 0: ioreg.io_bg3vofs = (ioreg.io_bg3vofs & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG3VOFS + 1: ioreg.io_bg3vofs = (ioreg.io_bg3vofs & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_BG2PA + 0: ioreg.io_bg2pa = (ioreg.io_bg2pa & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG2PA + 1: ioreg.io_bg2pa = (ioreg.io_bg2pa & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_BG2PB + 0: ioreg.io_bg2pb = (ioreg.io_bg2pb & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG2PB + 1: ioreg.io_bg2pb = (ioreg.io_bg2pb & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_BG2PC + 0: ioreg.io_bg2pc = (ioreg.io_bg2pc & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG2PC + 1: ioreg.io_bg2pc = (ioreg.io_bg2pc & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_BG2PD + 0: ioreg.io_bg2pd = (ioreg.io_bg2pd & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG2PD + 1: ioreg.io_bg2pd = (ioreg.io_bg2pd & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_BG2X_L + 0: ioreg.io_bg2x = (ioreg.io_bg2x & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_BG2X_L + 1: ioreg.io_bg2x = (ioreg.io_bg2x & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_BG2X_H + 0: ioreg.io_bg2x = (ioreg.io_bg2x & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_BG2X_H + 1: ioreg.io_bg2x = (ioreg.io_bg2x & 0x00ffffff) | ((value << 24) & 0xff000000); break;
        case REG_BG2Y_L + 0: ioreg.io_bg2y = (ioreg.io_bg2y & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_BG2Y_L + 1: ioreg.io_bg2y = (ioreg.io_bg2y & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_BG2Y_H + 0: ioreg.io_bg2y = (ioreg.io_bg2y & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_BG2Y_H + 1: ioreg.io_bg2y = (ioreg.io_bg2y & 0x00ffffff) | ((value << 24) & 0xff000000); break;
        case REG_BG3PA + 0: ioreg.io_bg3pa = (ioreg.io_bg3pa & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG3PA + 1: ioreg.io_bg3pa = (ioreg.io_bg3pa & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_BG3PB + 0: ioreg.io_bg3pb = (ioreg.io_bg3pb & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG3PB + 1: ioreg.io_bg3pb = (ioreg.io_bg3pb & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_BG3PC + 0: ioreg.io_bg3pc = (ioreg.io_bg3pc & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG3PC + 1: ioreg.io_bg3pc = (ioreg.io_bg3pc & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_BG3PD + 0: ioreg.io_bg3pd = (ioreg.io_bg3pd & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BG3PD + 1: ioreg.io_bg3pd = (ioreg.io_bg3pd & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_BG3X_L + 0: ioreg.io_bg3x = (ioreg.io_bg3x & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_BG3X_L + 1: ioreg.io_bg3x = (ioreg.io_bg3x & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_BG3X_H + 0: ioreg.io_bg3x = (ioreg.io_bg3x & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_BG3X_H + 1: ioreg.io_bg3x = (ioreg.io_bg3x & 0x00ffffff) | ((value << 24) & 0xff000000); break;
        case REG_BG3Y_L + 0: ioreg.io_bg3y = (ioreg.io_bg3y & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_BG3Y_L + 1: ioreg.io_bg3y = (ioreg.io_bg3y & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_BG3Y_H + 0: ioreg.io_bg3y = (ioreg.io_bg3y & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_BG3Y_H + 1: ioreg.io_bg3y = (ioreg.io_bg3y & 0x00ffffff) | ((value << 24) & 0xff000000); break;
        case REG_WIN0H + 0: ioreg.io_win0h = (ioreg.io_win0h & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_WIN0H + 1: ioreg.io_win0h = (ioreg.io_win0h & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_WIN1H + 0: ioreg.io_win1h = (ioreg.io_win1h & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_WIN1H + 1: ioreg.io_win1h = (ioreg.io_win1h & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_WIN0V + 0: ioreg.io_win0v = (ioreg.io_win0v & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_WIN0V + 1: ioreg.io_win0v = (ioreg.io_win0v & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_WIN1V + 0: ioreg.io_win1v = (ioreg.io_win1v & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_WIN1V + 1: ioreg.io_win1v = (ioreg.io_win1v & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_WININ + 0: ioreg.io_winin = (ioreg.io_winin & 0xff00) | ((value << 0) & 0x003f); break;
        case REG_WININ + 1: ioreg.io_winin = (ioreg.io_winin & 0x00ff) | ((value << 8) & 0x3f00); break;
        case REG_WINOUT + 0: ioreg.io_winout = (ioreg.io_winout & 0xff00) | ((value << 0) & 0x003f); break;
        case REG_WINOUT + 1: ioreg.io_winout = (ioreg.io_winout & 0x00ff) | ((value << 8) & 0x3f00); break;
        case REG_MOSAIC + 0: ioreg.io_mosaic = (ioreg.io_mosaic & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_MOSAIC + 1: ioreg.io_mosaic = (ioreg.io_mosaic & 0x00ff) | ((value << 8) & 0xff00); break;
        case 0x4e: break;
        case 0x4f: break;
        case REG_BLDCNT + 0: ioreg.io_bldcnt = (ioreg.io_bldcnt & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_BLDCNT + 1: ioreg.io_bldcnt = (ioreg.io_bldcnt & 0x00ff) | ((value << 8) & 0x3f00); break;
        case REG_BLDALPHA + 0: ioreg.io_bldalpha = (ioreg.io_bldalpha & 0xff00) | ((value << 0) & 0x001f); break;
        case REG_BLDALPHA + 1: ioreg.io_bldalpha = (ioreg.io_bldalpha & 0x00ff) | ((value << 8) & 0x1f00); break;
        case REG_BLDY + 0: ioreg.io_bldy = (ioreg.io_bldy & 0xff00) | ((value << 0) & 0x001f); break;
        case REG_BLDY + 1: ioreg.io_bldy = (ioreg.io_bldy & 0x00ff) | ((value << 8) & 0x0000); break;
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
        case REG_SOUND1CNT_L + 0: ioreg.io_sound1cnt_l = (ioreg.io_sound1cnt_l & 0xff00) | ((value << 0) & 0x007f); break;
        case REG_SOUND1CNT_L + 1: ioreg.io_sound1cnt_l = (ioreg.io_sound1cnt_l & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_SOUND1CNT_H + 0: ioreg.io_sound1cnt_h = (ioreg.io_sound1cnt_h & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUND1CNT_H + 1: ioreg.io_sound1cnt_h = (ioreg.io_sound1cnt_h & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_SOUND1CNT_X + 0: ioreg.io_sound1cnt_x = (ioreg.io_sound1cnt_x & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUND1CNT_X + 1: ioreg.io_sound1cnt_x = (ioreg.io_sound1cnt_x & 0x00ff) | ((value << 8) & 0xc700); break;
        case 0x66: break;
        case 0x67: break;
        case REG_SOUND2CNT_L + 0: ioreg.io_sound2cnt_l = (ioreg.io_sound2cnt_l & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUND2CNT_L + 1: ioreg.io_sound2cnt_l = (ioreg.io_sound2cnt_l & 0x00ff) | ((value << 8) & 0xff00); break;
        case 0x6a: break;
        case 0x6b: break;
        case REG_SOUND2CNT_H + 0: ioreg.io_sound2cnt_h = (ioreg.io_sound2cnt_h & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUND2CNT_H + 1: ioreg.io_sound2cnt_h = (ioreg.io_sound2cnt_h & 0x00ff) | ((value << 8) & 0xc700); break;
        case 0x6e: break;
        case 0x6f: break;
        case REG_SOUND3CNT_L + 0: ioreg.io_sound3cnt_l = (ioreg.io_sound3cnt_l & 0xff00) | ((value << 0) & 0x00e0); break;
        case REG_SOUND3CNT_L + 1: ioreg.io_sound3cnt_l = (ioreg.io_sound3cnt_l & 0x00ff) | ((value << 8) & 0x0000); break;
        case REG_SOUND3CNT_H + 0: ioreg.io_sound3cnt_h = (ioreg.io_sound3cnt_h & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUND3CNT_H + 1: ioreg.io_sound3cnt_h = (ioreg.io_sound3cnt_h & 0x00ff) | ((value << 8) & 0xe000); break;
        case REG_SOUND3CNT_X + 0: ioreg.io_sound3cnt_x = (ioreg.io_sound3cnt_x & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUND3CNT_X + 1: ioreg.io_sound3cnt_x = (ioreg.io_sound3cnt_x & 0x00ff) | ((value << 8) & 0xc700); break;
        case 0x76: break;
        case 0x77: break;
        case REG_SOUND4CNT_L + 0: ioreg.io_sound4cnt_l = (ioreg.io_sound4cnt_l & 0xff00) | ((value << 0) & 0x003f); break;
        case REG_SOUND4CNT_L + 1: ioreg.io_sound4cnt_l = (ioreg.io_sound4cnt_l & 0x00ff) | ((value << 8) & 0xff00); break;
        case 0x7a: break;
        case 0x7b: break;
        case REG_SOUND4CNT_H + 0: ioreg.io_sound4cnt_h = (ioreg.io_sound4cnt_h & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUND4CNT_H + 1: ioreg.io_sound4cnt_h = (ioreg.io_sound4cnt_h & 0x00ff) | ((value << 8) & 0xc000); break;
        case 0x7e: break;
        case 0x7f: break;
        case REG_SOUNDCNT_L + 0: ioreg.io_soundcnt_l = (ioreg.io_soundcnt_l & 0xff00) | ((value << 0) & 0x0077); break;
        case REG_SOUNDCNT_L + 1: ioreg.io_soundcnt_l = (ioreg.io_soundcnt_l & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_SOUNDCNT_H + 0: ioreg.io_soundcnt_h = (ioreg.io_soundcnt_h & 0xff00) | ((value << 0) & 0x000f); break;
        case REG_SOUNDCNT_H + 1: ioreg.io_soundcnt_h = (ioreg.io_soundcnt_h & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_SOUNDCNT_X + 0: ioreg.io_soundcnt_x = (ioreg.io_soundcnt_x & 0xff00) | ((value << 0) & 0x008f); break;
        case REG_SOUNDCNT_X + 1: ioreg.io_soundcnt_x = (ioreg.io_soundcnt_x & 0x00ff) | ((value << 8) & 0x0000); break;
        case 0x86: break;
        case 0x87: break;
        case REG_SOUNDBIAS + 0: ioreg.io_soundbias = (ioreg.io_soundbias & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_SOUNDBIAS + 1: ioreg.io_soundbias = (ioreg.io_soundbias & 0x00ff) | ((value << 8) & 0xc300); break;
        case 0x8a: break;
        case 0x8b: break;
        case 0x8c: break;
        case 0x8d: break;
        case 0x8e: break;
        case 0x8f: break;
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
        case REG_FIFO_A_L + 0: ioreg.io_fifo_a = (ioreg.io_fifo_a & 0xffffff00) | ((value << 0) & 0x000000ff); break;  // FIXME
        case REG_FIFO_A_L + 1: ioreg.io_fifo_a = (ioreg.io_fifo_a & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_FIFO_A_H + 0: ioreg.io_fifo_a = (ioreg.io_fifo_a & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_FIFO_A_H + 1: ioreg.io_fifo_a = (ioreg.io_fifo_a & 0x00ffffff) | ((value << 24) & 0xff000000); break;
        case REG_FIFO_B_L + 0: ioreg.io_fifo_b = (ioreg.io_fifo_b & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_FIFO_B_L + 1: ioreg.io_fifo_b = (ioreg.io_fifo_b & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_FIFO_B_H + 0: ioreg.io_fifo_b = (ioreg.io_fifo_b & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_FIFO_B_H + 1: ioreg.io_fifo_b = (ioreg.io_fifo_b & 0x00ffffff) | ((value << 24) & 0xff000000); break;
        case 0xa8: break;
        case 0xa9: break;
        case 0xaa: break;
        case 0xab: break;
        case 0xac: break;
        case 0xad: break;
        case 0xae: break;
        case 0xaf: break;
        case REG_DMA0SAD_L + 0: ioreg.io_dma0sad = (ioreg.io_dma0sad & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_DMA0SAD_L + 1: ioreg.io_dma0sad = (ioreg.io_dma0sad & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_DMA0SAD_H + 0: ioreg.io_dma0sad = (ioreg.io_dma0sad & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_DMA0SAD_H + 1: ioreg.io_dma0sad = (ioreg.io_dma0sad & 0x00ffffff) | ((value << 24) & 0x07000000); break;
        case REG_DMA0DAD_L + 0: ioreg.io_dma0dad = (ioreg.io_dma0dad & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_DMA0DAD_L + 1: ioreg.io_dma0dad = (ioreg.io_dma0dad & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_DMA0DAD_H + 0: ioreg.io_dma0dad = (ioreg.io_dma0dad & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_DMA0DAD_H + 1: ioreg.io_dma0dad = (ioreg.io_dma0dad & 0x00ffffff) | ((value << 24) & 0x07000000); break;
        case REG_DMA0CNT_L + 0: ioreg.io_dma0cnt_l = (ioreg.io_dma0cnt_l & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_DMA0CNT_L + 1: ioreg.io_dma0cnt_l = (ioreg.io_dma0cnt_l & 0x00ff) | ((value << 8) & 0x3f00); break;
        case REG_DMA0CNT_H + 0: ioreg.io_dma0cnt_h = (ioreg.io_dma0cnt_h & 0xff00) | ((value << 0) & 0x00e0); break;
        case REG_DMA0CNT_H + 1: ioreg.io_dma0cnt_h = (ioreg.io_dma0cnt_h & 0x00ff) | ((value << 8) & 0xf700); break;
        case REG_DMA1SAD_L + 0: ioreg.io_dma1sad = (ioreg.io_dma1sad & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_DMA1SAD_L + 1: ioreg.io_dma1sad = (ioreg.io_dma1sad & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_DMA1SAD_H + 0: ioreg.io_dma1sad = (ioreg.io_dma1sad & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_DMA1SAD_H + 1: ioreg.io_dma1sad = (ioreg.io_dma1sad & 0x00ffffff) | ((value << 24) & 0x0f000000); break;
        case REG_DMA1DAD_L + 0: ioreg.io_dma1dad = (ioreg.io_dma1dad & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_DMA1DAD_L + 1: ioreg.io_dma1dad = (ioreg.io_dma1dad & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_DMA1DAD_H + 0: ioreg.io_dma1dad = (ioreg.io_dma1dad & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_DMA1DAD_H + 1: ioreg.io_dma1dad = (ioreg.io_dma1dad & 0x00ffffff) | ((value << 24) & 0x07000000); break;
        case REG_DMA1CNT_L + 0: ioreg.io_dma1cnt_l = (ioreg.io_dma1cnt_l & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_DMA1CNT_L + 1: ioreg.io_dma1cnt_l = (ioreg.io_dma1cnt_l & 0x00ff) | ((value << 8) & 0x3f00); break;
        case REG_DMA1CNT_H + 0: ioreg.io_dma1cnt_h = (ioreg.io_dma1cnt_h & 0xff00) | ((value << 0) & 0x00e0); break;
        case REG_DMA1CNT_H + 1: ioreg.io_dma1cnt_h = (ioreg.io_dma1cnt_h & 0x00ff) | ((value << 8) & 0xf700); break;
        case REG_DMA2SAD_L + 0: ioreg.io_dma2sad = (ioreg.io_dma2sad & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_DMA2SAD_L + 1: ioreg.io_dma2sad = (ioreg.io_dma2sad & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_DMA2SAD_H + 0: ioreg.io_dma2sad = (ioreg.io_dma2sad & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_DMA2SAD_H + 1: ioreg.io_dma2sad = (ioreg.io_dma2sad & 0x00ffffff) | ((value << 24) & 0x0f000000); break;
        case REG_DMA2DAD_L + 0: ioreg.io_dma2dad = (ioreg.io_dma2dad & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_DMA2DAD_L + 1: ioreg.io_dma2dad = (ioreg.io_dma2dad & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_DMA2DAD_H + 0: ioreg.io_dma2dad = (ioreg.io_dma2dad & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_DMA2DAD_H + 1: ioreg.io_dma2dad = (ioreg.io_dma2dad & 0x00ffffff) | ((value << 24) & 0x07000000); break;
        case REG_DMA2CNT_L + 0: ioreg.io_dma2cnt_l = (ioreg.io_dma2cnt_l & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_DMA2CNT_L + 1: ioreg.io_dma2cnt_l = (ioreg.io_dma2cnt_l & 0x00ff) | ((value << 8) & 0x3f00); break;
        case REG_DMA2CNT_H + 0: ioreg.io_dma2cnt_h = (ioreg.io_dma2cnt_h & 0xff00) | ((value << 0) & 0x00e0); break;
        case REG_DMA2CNT_H + 1: ioreg.io_dma2cnt_h = (ioreg.io_dma2cnt_h & 0x00ff) | ((value << 8) & 0xf700); break;
        case REG_DMA3SAD_L + 0: ioreg.io_dma3sad = (ioreg.io_dma3sad & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_DMA3SAD_L + 1: ioreg.io_dma3sad = (ioreg.io_dma3sad & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_DMA3SAD_H + 0: ioreg.io_dma3sad = (ioreg.io_dma3sad & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_DMA3SAD_H + 1: ioreg.io_dma3sad = (ioreg.io_dma3sad & 0x00ffffff) | ((value << 24) & 0x0f000000); break;
        case REG_DMA3DAD_L + 0: ioreg.io_dma3dad = (ioreg.io_dma3dad & 0xffffff00) | ((value << 0) & 0x000000ff); break;
        case REG_DMA3DAD_L + 1: ioreg.io_dma3dad = (ioreg.io_dma3dad & 0xffff00ff) | ((value << 8) & 0x0000ff00); break;
        case REG_DMA3DAD_H + 0: ioreg.io_dma3dad = (ioreg.io_dma3dad & 0xff00ffff) | ((value << 16) & 0x00ff0000); break;
        case REG_DMA3DAD_H + 1: ioreg.io_dma3dad = (ioreg.io_dma3dad & 0x00ffffff) | ((value << 24) & 0x0f000000); break;
        case REG_DMA3CNT_L + 0: ioreg.io_dma3cnt_l = (ioreg.io_dma3cnt_l & 0xff00) | ((value << 0) & 0x00ff); break;
        case REG_DMA3CNT_L + 1: ioreg.io_dma3cnt_l = (ioreg.io_dma3cnt_l & 0x00ff) | ((value << 8) & 0xff00); break;
        case REG_DMA3CNT_H + 0: ioreg.io_dma3cnt_h = (ioreg.io_dma3cnt_h & 0xff00) | ((value << 0) & 0x00e0); break;
        case REG_DMA3CNT_H + 1: ioreg.io_dma3cnt_h = (ioreg.io_dma3cnt_h & 0x00ff) | ((value << 8) & 0xff00); break;
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
            ioreg.io_if &= ~value;
            break;

        case REG_IME:
            ioreg.io_ime = (ioreg.io_ime & 0xff00) | value;
            break;

        case REG_HALTCNT:
            ioreg.io_haltcnt = value;
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
        case REG_DISPCNT: return ioreg.io_dispcnt;
        case 0x2: return 0xdead;
        case REG_DISPSTAT: return ioreg.io_dispstat;
        case REG_VCOUNT: return ioreg.io_vcount;
        case REG_BG0CNT: return ioreg.io_bg0cnt;
        case REG_BG1CNT: return ioreg.io_bg1cnt;
        case REG_BG2CNT: return ioreg.io_bg2cnt;
        case REG_BG3CNT: return ioreg.io_bg3cnt;
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
        case REG_WININ: return ioreg.io_winin;
        case REG_WINOUT: return ioreg.io_winout;
        case REG_MOSAIC: return 0xdead;
        case 0x4e: return 0xdead;
        case REG_BLDCNT: return ioreg.io_bldcnt;
        case REG_BLDALPHA: return ioreg.io_bldalpha;
        case REG_BLDY: return 0xdead;
        case 0x56: return 0xdead;
        case 0x58: return 0xdead;
        case 0x5a: return 0xdead;
        case 0x5c: return 0xdead;
        case 0x5e: return 0xdead;
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
        case 0x8c: return 0xdead;
        case 0x8e: return 0xdead;
        case REG_WAVE_RAM0_L: return (uint16_t)(ioreg.io_wave_ram0 >> 0);
        case REG_WAVE_RAM0_H: return (uint16_t)(ioreg.io_wave_ram0 >> 16);
        case REG_WAVE_RAM1_L: return (uint16_t)(ioreg.io_wave_ram1 >> 0);
        case REG_WAVE_RAM1_H: return (uint16_t)(ioreg.io_wave_ram1 >> 16);
        case REG_WAVE_RAM2_L: return (uint16_t)(ioreg.io_wave_ram2 >> 0);
        case REG_WAVE_RAM2_H: return (uint16_t)(ioreg.io_wave_ram2 >> 16);
        case REG_WAVE_RAM3_L: return (uint16_t)(ioreg.io_wave_ram3 >> 0);
        case REG_WAVE_RAM3_H: return (uint16_t)(ioreg.io_wave_ram3 >> 16);
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
        case REG_DMA0CNT_H: return ioreg.io_dma0cnt_h;
        case REG_DMA1SAD_L: return 0xdead;
        case REG_DMA1SAD_H: return 0xdead;
        case REG_DMA1DAD_L: return 0xdead;
        case REG_DMA1DAD_H: return 0xdead;
        case REG_DMA1CNT_L: return 0;
        case REG_DMA1CNT_H: return ioreg.io_dma1cnt_h;
        case REG_DMA2SAD_L: return 0xdead;
        case REG_DMA2SAD_H: return 0xdead;
        case REG_DMA2DAD_L: return 0xdead;
        case REG_DMA2DAD_H: return 0xdead;
        case REG_DMA2CNT_L: return 0;
        case REG_DMA2CNT_H: return ioreg.io_dma2cnt_h;
        case REG_DMA3SAD_L: return 0xdead;
        case REG_DMA3SAD_H: return 0xdead;
        case REG_DMA3DAD_L: return 0xdead;
        case REG_DMA3DAD_H: return 0xdead;
        case REG_DMA3CNT_L: return 0;
        case REG_DMA3CNT_H: return ioreg.io_dma3cnt_h;
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

        case REG_TM0CNT_L: return ioreg.timer_0_counter;
        case REG_TM0CNT_H: return ioreg.timer_0_control;
        case REG_TM1CNT_L: return ioreg.timer_1_counter;
        case REG_TM1CNT_H: return ioreg.timer_1_control;
        case REG_TM2CNT_L: return ioreg.timer_2_counter;
        case REG_TM2CNT_H: return ioreg.timer_2_control;
        case REG_TM3CNT_L: return ioreg.timer_3_counter;
        case REG_TM3CNT_H: return ioreg.timer_3_control;

        case REG_SIODATA32: return 0;  // FIXME
        case REG_SIOCNT: return 0;  // FIXME

        case REG_KEYINPUT:
            ioreg.io_keyinput = 0x3ff;
            for (int i = 0; i < NUM_KEYS; i++) {
                if (keys[i]) ioreg.io_keyinput &= ~(1 << i);
            }
            return ioreg.io_keyinput;

        //case IO_RCNT:
        //    return ioreg.io_rcnt;

        case REG_IE: return ioreg.io_ie;
        case REG_IF: return ioreg.io_if;
        case REG_IME: return ioreg.io_ime;

        default:
#ifdef LOG_BAD_MEMORY_ACCESS
            printf("io_read_halfword(0x%08x);\n", address);
#endif
            return 0;
    }
}

void io_write_halfword(uint32_t address, uint16_t value) {
    switch (address) {
        case REG_DISPCNT: ioreg.io_dispcnt = (ioreg.io_dispcnt & 0x0008) | (value & 0xfff7); break;
        case 0x2: break;
        case REG_DISPSTAT: ioreg.io_dispstat = (ioreg.io_dispstat & 0x0007) | (value & 0xff38); break;
        case REG_VCOUNT: break;
        case REG_BG0CNT: ioreg.io_bg0cnt = value & 0xdfff; break;
        case REG_BG1CNT: ioreg.io_bg1cnt = value & 0xdfff; break;
        case REG_BG2CNT: ioreg.io_bg2cnt = value & 0xffff; break;
        case REG_BG3CNT: ioreg.io_bg3cnt = value & 0xffff; break;
        case REG_BG0HOFS: ioreg.io_bg0hofs = value & 0x00ff; break;
        case REG_BG0VOFS: ioreg.io_bg0vofs = value & 0x00ff; break;
        case REG_BG1HOFS: ioreg.io_bg1hofs = value & 0x00ff; break;
        case REG_BG1VOFS: ioreg.io_bg1vofs = value & 0x00ff; break;
        case REG_BG2HOFS: ioreg.io_bg2hofs = value & 0x00ff; break;
        case REG_BG2VOFS: ioreg.io_bg2vofs = value & 0x00ff; break;
        case REG_BG3HOFS: ioreg.io_bg3hofs = value & 0x00ff; break;
        case REG_BG3VOFS: ioreg.io_bg3vofs = value & 0x00ff; break;
        case REG_BG2PA: ioreg.io_bg2pa = value & 0xffff; break;
        case REG_BG2PB: ioreg.io_bg2pb = value & 0xffff; break;
        case REG_BG2PC: ioreg.io_bg2pc = value & 0xffff; break;
        case REG_BG2PD: ioreg.io_bg2pd = value & 0xffff; break;
        case REG_BG2X_L: ioreg.io_bg2x = (ioreg.io_bg2x & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_BG2X_H: ioreg.io_bg2x = (ioreg.io_bg2x & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_BG2Y_L: ioreg.io_bg2y = (ioreg.io_bg2y & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_BG2Y_H: ioreg.io_bg2y = (ioreg.io_bg2y & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_BG3PA: ioreg.io_bg3pa = value & 0xffff; break;
        case REG_BG3PB: ioreg.io_bg3pb = value & 0xffff; break;
        case REG_BG3PC: ioreg.io_bg3pc = value & 0xffff; break;
        case REG_BG3PD: ioreg.io_bg3pd = value & 0xffff; break;
        case REG_BG3X_L: ioreg.io_bg3x = (ioreg.io_bg3x & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_BG3X_H: ioreg.io_bg3x = (ioreg.io_bg3x & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_BG3Y_L: ioreg.io_bg3y = (ioreg.io_bg3y & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_BG3Y_H: ioreg.io_bg3y = (ioreg.io_bg3y & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_WIN0H: ioreg.io_win0h = value & 0xffff; break;
        case REG_WIN1H: ioreg.io_win1h = value & 0xffff; break;
        case REG_WIN0V: ioreg.io_win0v = value & 0xffff; break;
        case REG_WIN1V: ioreg.io_win1v = value & 0xffff; break;
        case REG_WININ: ioreg.io_winin = value & 0x3f3f; break;
        case REG_WINOUT: ioreg.io_winout = value & 0x3f3f; break;
        case REG_MOSAIC: ioreg.io_mosaic = value & 0xffff; break;
        case 0x4e: break;
        case REG_BLDCNT: ioreg.io_bldcnt = value & 0x3fff; break;
        case REG_BLDALPHA: ioreg.io_bldalpha = value & 0x1f1f; break;
        case REG_BLDY: ioreg.io_bldy = value & 0x001f; break;
        case 0x56: break;
        case 0x58: break;
        case 0x5a: break;
        case 0x5c: break;
        case 0x5e: break;
        case REG_SOUND1CNT_L: ioreg.io_sound1cnt_l = value & 0x007f; break;
        case REG_SOUND1CNT_H: ioreg.io_sound1cnt_h = value & 0xffff; break;
        case REG_SOUND1CNT_X: ioreg.io_sound1cnt_x = value & 0xc7ff; break;
        case 0x66: break;
        case REG_SOUND2CNT_L: ioreg.io_sound2cnt_l = value & 0xffff; break;
        case 0x6a: break;
        case REG_SOUND2CNT_H: ioreg.io_sound2cnt_h = value & 0xc7ff; break;
        case 0x6e: break;
        case REG_SOUND3CNT_L: ioreg.io_sound3cnt_l = value & 0x00e0; break;
        case REG_SOUND3CNT_H: ioreg.io_sound3cnt_h = value & 0xe0ff; break;
        case REG_SOUND3CNT_X: ioreg.io_sound3cnt_x = value & 0xc7ff; break;
        case 0x76: break;
        case REG_SOUND4CNT_L: ioreg.io_sound4cnt_l = value & 0xff3f; break;
        case 0x7a: break;
        case REG_SOUND4CNT_H: ioreg.io_sound4cnt_h = value & 0xc0ff; break;
        case 0x7e: break;
        case REG_SOUNDCNT_L: ioreg.io_soundcnt_l = value & 0xff77; break;
        case REG_SOUNDCNT_H: ioreg.io_soundcnt_h = value & 0xff0f; break;
        case REG_SOUNDCNT_X: ioreg.io_soundcnt_x = value & 0x008f; break;
        case 0x86: break;
        case REG_SOUNDBIAS: ioreg.io_soundbias = value & 0xc3ff; break;
        case 0x8a: break;
        case 0x8c: break;
        case 0x8e: break;
        case REG_WAVE_RAM0_L: ioreg.io_wave_ram0 = (ioreg.io_wave_ram0 & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_WAVE_RAM0_H: ioreg.io_wave_ram0 = (ioreg.io_wave_ram0 & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_WAVE_RAM1_L: ioreg.io_wave_ram1 = (ioreg.io_wave_ram1 & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_WAVE_RAM1_H: ioreg.io_wave_ram1 = (ioreg.io_wave_ram1 & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_WAVE_RAM2_L: ioreg.io_wave_ram2 = (ioreg.io_wave_ram2 & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_WAVE_RAM2_H: ioreg.io_wave_ram2 = (ioreg.io_wave_ram2 & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_WAVE_RAM3_L: ioreg.io_wave_ram3 = (ioreg.io_wave_ram3 & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_WAVE_RAM3_H: ioreg.io_wave_ram3 = (ioreg.io_wave_ram3 & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_FIFO_A_L: ioreg.io_fifo_a = (ioreg.io_fifo_a & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_FIFO_A_H: ioreg.io_fifo_a = (ioreg.io_fifo_a & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case REG_FIFO_B_L: ioreg.io_fifo_b = (ioreg.io_fifo_b & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_FIFO_B_H: ioreg.io_fifo_b = (ioreg.io_fifo_b & 0x0000ffff) | ((value << 16) & 0xffff0000); break;
        case 0xa8: break;
        case 0xaa: break;
        case 0xac: break;
        case 0xae: break;
        case REG_DMA0SAD_L: ioreg.io_dma0sad = (ioreg.io_dma0sad & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_DMA0SAD_H: ioreg.io_dma0sad = (ioreg.io_dma0sad & 0x0000ffff) | ((value << 16) & 0x07ff0000); break;
        case REG_DMA0DAD_L: ioreg.io_dma0dad = (ioreg.io_dma0dad & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_DMA0DAD_H: ioreg.io_dma0dad = (ioreg.io_dma0dad & 0x0000ffff) | ((value << 16) & 0x07ff0000); break;
        case REG_DMA0CNT_L: ioreg.io_dma0cnt_l = value & 0x3fff; break;
        case REG_DMA0CNT_H: ioreg.io_dma0cnt_h = value & 0xf7e0; break;
        case REG_DMA1SAD_L: ioreg.io_dma1sad = (ioreg.io_dma1sad & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_DMA1SAD_H: ioreg.io_dma1sad = (ioreg.io_dma1sad & 0x0000ffff) | ((value << 16) & 0x0fff0000); break;
        case REG_DMA1DAD_L: ioreg.io_dma1dad = (ioreg.io_dma1dad & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_DMA1DAD_H: ioreg.io_dma1dad = (ioreg.io_dma1dad & 0x0000ffff) | ((value << 16) & 0x07ff0000); break;
        case REG_DMA1CNT_L: ioreg.io_dma1cnt_l = value & 0x3fff; break;
        case REG_DMA1CNT_H: ioreg.io_dma1cnt_h = value & 0xf7e0; break;
        case REG_DMA2SAD_L: ioreg.io_dma2sad = (ioreg.io_dma2sad & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_DMA2SAD_H: ioreg.io_dma2sad = (ioreg.io_dma2sad & 0x0000ffff) | ((value << 16) & 0x0fff0000); break;
        case REG_DMA2DAD_L: ioreg.io_dma2dad = (ioreg.io_dma2dad & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_DMA2DAD_H: ioreg.io_dma2dad = (ioreg.io_dma2dad & 0x0000ffff) | ((value << 16) & 0x07ff0000); break;
        case REG_DMA2CNT_L: ioreg.io_dma2cnt_l = value & 0x3fff; break;
        case REG_DMA2CNT_H: ioreg.io_dma2cnt_h = value & 0xf7e0; break;
        case REG_DMA3SAD_L: ioreg.io_dma3sad = (ioreg.io_dma3sad & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_DMA3SAD_H: ioreg.io_dma3sad = (ioreg.io_dma3sad & 0x0000ffff) | ((value << 16) & 0x0fff0000); break;
        case REG_DMA3DAD_L: ioreg.io_dma3dad = (ioreg.io_dma3dad & 0xffff0000) | ((value << 0) & 0x0000ffff); break;
        case REG_DMA3DAD_H: ioreg.io_dma3dad = (ioreg.io_dma3dad & 0x0000ffff) | ((value << 16) & 0x0fff0000); break;
        case REG_DMA3CNT_L: ioreg.io_dma3cnt_l = value & 0xffff; break;
        case REG_DMA3CNT_H: ioreg.io_dma3cnt_h = value & 0xffe0; break;
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
            ioreg.timer_0_reload = value;
            break;

        case REG_TM0CNT_H:
            ioreg.timer_0_control = value;
            if (ioreg.timer_0_control & 0x80) ioreg.timer_0_counter = ioreg.timer_0_reload;
            break;

        case REG_TM1CNT_L:
            ioreg.timer_1_reload = value;
            break;

        case REG_TM1CNT_H:
            ioreg.timer_1_control = value;
            if (ioreg.timer_1_control & 0x80) ioreg.timer_1_counter = ioreg.timer_1_reload;
            break;

        case REG_TM2CNT_L:
            ioreg.timer_2_reload = value;
            break;

        case REG_TM2CNT_H:
            ioreg.timer_2_control = value;
            if (ioreg.timer_2_control & 0x80) ioreg.timer_2_counter = ioreg.timer_2_reload;
            break;

        case REG_TM3CNT_L:
            ioreg.timer_3_reload = value;
            break;

        case REG_TM3CNT_H:
            ioreg.timer_3_control = value;
            if (ioreg.timer_3_control & 0x80) ioreg.timer_3_counter = ioreg.timer_3_reload;
            break;

        case REG_IE: ioreg.io_ie = value; break;
        case REG_IF: ioreg.io_if &= ~value; break;

        //case IO_WAITCNT:
        //    ioreg.io_waitcnt = value;
        //    printf("WAITCNT = 0x%04x\n", ioreg.io_waitcnt);
        //    break;

        case REG_IME: ioreg.io_ime = value; break;

        default:
#ifdef LOG_BAD_MEMORY_ACCESS
            printf("io_write_halfword(0x%08x, 0x%04x);\n", address, value);
#endif
            break;
    }
}

uint32_t io_read_word(uint32_t address) {
    switch (address) {
        case REG_DISPCNT: return ioreg.io_dispcnt | 0xdead << 16;
        case REG_DISPSTAT: return ioreg.io_dispstat | ioreg.io_vcount << 16;
        case REG_BG0CNT: return ioreg.io_bg0cnt | ioreg.io_bg1cnt << 16;
        case REG_BG2CNT: return ioreg.io_bg2cnt | ioreg.io_bg3cnt << 16;
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
        case REG_WININ: return ioreg.io_winin | ioreg.io_winout << 16;
        case REG_MOSAIC: return 0xdeaddead;
        case REG_BLDCNT: return ioreg.io_bldcnt | ioreg.io_bldalpha << 16;
        case REG_BLDY: return ioreg.io_bldy | 0xdead << 16;
        case 0x58: return 0xdeaddead;
        case 0x5c: return 0xdeaddead;
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
        case 0x8c: return 0xdeaddead;
        case REG_WAVE_RAM0_L: return ioreg.io_wave_ram0;
        case REG_WAVE_RAM1_L: return ioreg.io_wave_ram1;
        case REG_WAVE_RAM2_L: return ioreg.io_wave_ram2;
        case REG_WAVE_RAM3_L: return ioreg.io_wave_ram3;
        case REG_FIFO_A_L: return 0xdeaddead;
        case REG_FIFO_B_L: return 0xdeaddead;
        case 0xa8: return 0xdeaddead;
        case 0xac: return 0xdeaddead;
        case REG_DMA0SAD_L: return 0xdeaddead;
        case REG_DMA0DAD_L: return 0xdeaddead;
        case REG_DMA0CNT_L: return ioreg.io_dma0cnt_h << 16;
        case REG_DMA1SAD_L: return 0xdeaddead;
        case REG_DMA1DAD_L: return 0xdeaddead;
        case REG_DMA1CNT_L: return ioreg.io_dma1cnt_h << 16;
        case REG_DMA2SAD_L: return 0xdeaddead;
        case REG_DMA2DAD_L: return 0xdeaddead;
        case REG_DMA2CNT_L: return ioreg.io_dma2cnt_h << 16;
        case REG_DMA3SAD_L: return 0xdeaddead;
        case REG_DMA3DAD_L: return 0xdeaddead;
        case REG_DMA3CNT_L: return ioreg.io_dma3cnt_h << 16;
        case 0xe0: return 0xdeaddead;
        case 0xe4: return 0xdeaddead;
        case 0xe8: return 0xdeaddead;
        case 0xec: return 0xdeaddead;
        case 0xf0: return 0xdeaddead;
        case 0xf4: return 0xdeaddead;
        case 0xf8: return 0xdeaddead;
        case 0xfc: return 0xdeaddead;
        case 0x100c: return 0xdeaddead;

        case REG_TM0CNT_L: return ioreg.timer_0_counter | ioreg.timer_0_control << 16;
        case REG_TM1CNT_L: return ioreg.timer_1_counter | ioreg.timer_1_control << 16;
        case REG_TM2CNT_L: return ioreg.timer_2_counter | ioreg.timer_2_control << 16;
        case REG_TM3CNT_L: return ioreg.timer_3_counter | ioreg.timer_3_control << 16;

        //case IO_KEYINPUT:
        //    ioreg.io_keyinput = 0x3ff;
        //    for (int i = 0; i < NUM_KEYS; i++) {
        //        if (keys[i]) ioreg.io_keyinput &= ~(1 << i);
        //    }
        //    return ioreg.io_keyinput | ioreg.io_keycnt << 16;

        case REG_IE:
            return ioreg.io_ie | ioreg.io_if << 16;

        case REG_IME:
            return ioreg.io_ime;

        default:
#ifdef LOG_BAD_MEMORY_ACCESS
            printf("io_read_word(0x%08x);\n", address);
#endif
            return 0;
    }
}

void io_write_word(uint32_t address, uint32_t value) {
    switch (address) {
        case REG_DISPCNT: ioreg.io_dispcnt = (ioreg.io_dispcnt & 0x0008) | (value & 0xfff7); break;
        case REG_DISPSTAT: ioreg.io_dispstat = (ioreg.io_dispstat & 0x0007) | (value & 0xff38); break;
        case REG_BG0CNT: ioreg.io_bg0cnt = value & 0xdfff; ioreg.io_bg1cnt = (value >> 16) & 0xdfff; break;
        case REG_BG2CNT: ioreg.io_bg2cnt = value & 0xffff; ioreg.io_bg3cnt = (value >> 16) & 0xffff; break;
        case REG_BG0HOFS: ioreg.io_bg0hofs = value & 0x00ff; ioreg.io_bg0vofs = (value >> 16) & 0x00ff; break;
        case REG_BG1HOFS: ioreg.io_bg1hofs = value & 0x00ff; ioreg.io_bg1vofs = (value >> 16) & 0x00ff; break;
        case REG_BG2HOFS: ioreg.io_bg2hofs = value & 0x00ff; ioreg.io_bg2vofs = (value >> 16) & 0x00ff; break;
        case REG_BG3HOFS: ioreg.io_bg3hofs = value & 0x00ff; ioreg.io_bg3vofs = (value >> 16) & 0x00ff; break;
        case REG_BG2PA: ioreg.io_bg2pa = value & 0xffff; ioreg.io_bg2pb = (value >> 16) & 0xffff; break;
        case REG_BG2PC: ioreg.io_bg2pc = value & 0xffff; ioreg.io_bg2pd = (value >> 16) & 0xffff; break;
        case REG_BG2X_L: ioreg.io_bg2x = value & 0xffffffff; break;
        case REG_BG2Y_L: ioreg.io_bg2y = value & 0xffffffff; break;
        case REG_BG3PA: ioreg.io_bg3pa = value & 0xffff; ioreg.io_bg3pb = (value >> 16) & 0xffff; break;
        case REG_BG3PC: ioreg.io_bg3pc = value & 0xffff; ioreg.io_bg3pd = (value >> 16) & 0xffff; break;
        case REG_BG3X_L: ioreg.io_bg3x = value & 0xffffffff; break;
        case REG_BG3Y_L: ioreg.io_bg3y = value & 0xffffffff; break;
        case REG_WIN0H: ioreg.io_win0h = value & 0xffff; ioreg.io_win1h = (value >> 16) & 0xffff; break;
        case REG_WIN0V: ioreg.io_win0v = value & 0xffff; ioreg.io_win1v = (value >> 16) & 0xffff; break;
        case REG_WININ: ioreg.io_winin = value & 0x3f3f; ioreg.io_winout = (value >> 16) & 0x3f3f; break;
        case REG_MOSAIC: ioreg.io_mosaic = value & 0xffff; break;
        case REG_BLDCNT: ioreg.io_bldcnt = value & 0x3fff; ioreg.io_bldalpha = (value >> 16) & 0x1f1f; break;
        case REG_BLDY: ioreg.io_bldy = value & 0x001f; break;
        case 0x58: break;
        case 0x5c: break;
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
        case 0x8c: break;
        case REG_WAVE_RAM0_L: ioreg.io_wave_ram0 = value; break;
        case REG_WAVE_RAM1_L: ioreg.io_wave_ram1 = value; break;
        case REG_WAVE_RAM2_L: ioreg.io_wave_ram2 = value; break;
        case REG_WAVE_RAM3_L: ioreg.io_wave_ram3 = value; break;
        case REG_FIFO_A_L: ioreg.io_fifo_a = value; break;
        case REG_FIFO_B_L: ioreg.io_fifo_b = value; break;
        case 0xa8: break;
        case 0xac: break;
        case REG_DMA0SAD_L: ioreg.io_dma0sad = value & 0x07ffffff; break;
        case REG_DMA0DAD_L: ioreg.io_dma0dad = value & 0x07ffffff; break;
        case REG_DMA0CNT_L: ioreg.io_dma0cnt_l = value & 0x3fff; ioreg.io_dma0cnt_h = (value >> 16) & 0xf7e0; break;
        case REG_DMA1SAD_L: ioreg.io_dma1sad = value & 0x0fffffff; break;
        case REG_DMA1DAD_L: ioreg.io_dma1dad = value & 0x07ffffff; break;
        case REG_DMA1CNT_L: ioreg.io_dma1cnt_l = value & 0x3fff; ioreg.io_dma1cnt_h = (value >> 16) & 0xf7e0; break;
        case REG_DMA2SAD_L: ioreg.io_dma2sad = value & 0x0fffffff; break;
        case REG_DMA2DAD_L: ioreg.io_dma2dad = value & 0x07ffffff; break;
        case REG_DMA2CNT_L: ioreg.io_dma2cnt_l = value & 0x3fff; ioreg.io_dma2cnt_h = (value >> 16) & 0xf7e0; break;
        case REG_DMA3SAD_L: ioreg.io_dma3sad = value & 0x0fffffff; break;
        case REG_DMA3DAD_L: ioreg.io_dma3dad = value & 0x0fffffff; break;
        case REG_DMA3CNT_L: ioreg.io_dma3cnt_l = value & 0xffff; ioreg.io_dma3cnt_h = (value >> 16) & 0xffe0; break;
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
            ioreg.timer_0_reload = (uint16_t) value;
            ioreg.timer_0_control = (uint16_t)(value >> 16);
            if (ioreg.timer_0_control & 0x80) ioreg.timer_0_counter = ioreg.timer_0_reload;
            break;

        case REG_TM1CNT_L:
            ioreg.timer_1_reload = (uint16_t) value;
            ioreg.timer_1_control = (uint16_t)(value >> 16);
            if (ioreg.timer_1_control & 0x80) ioreg.timer_1_counter = ioreg.timer_1_reload;
            break;

        case REG_TM2CNT_L:
            ioreg.timer_2_reload = (uint16_t) value;
            ioreg.timer_2_control = (uint16_t)(value >> 16);
            if (ioreg.timer_2_control & 0x80) ioreg.timer_2_counter = ioreg.timer_2_reload;
            break;

        case REG_TM3CNT_L:
            ioreg.timer_3_reload = (uint16_t) value;
            ioreg.timer_3_control = (uint16_t)(value >> 16);
            if (ioreg.timer_3_control & 0x80) ioreg.timer_3_counter = ioreg.timer_3_reload;
            break;

        case REG_IE:
            ioreg.io_ie = (uint16_t) value;
            ioreg.io_if &= ~(uint16_t)(value >> 16);
            break;

        case REG_WAITCNT:
            ioreg.io_waitcnt = (uint16_t) value;
            break;

        case REG_IME:
            ioreg.io_ime = (uint16_t) value;
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
    if (match) { has_sram = true; }

    /*
    printf("has_eeprom = %s\n", (has_eeprom ? "true" : "false"));
    printf("has_flash = %s\n", (has_flash ? "true" : "false"));
    printf("has_sram = %s\n", (has_sram ? "true" : "false"));
    */
}

void gba_reset(void) {
    memset(cpu_ewram, 0, sizeof(uint8_t) * sizeof(cpu_ewram));
    memset(cpu_iwram, 0, sizeof(uint8_t) * sizeof(cpu_iwram));
    memset(&ioreg, 0, sizeof(uint8_t) * sizeof(ioreg));
    memset(palette_ram, 0, sizeof(uint8_t) * sizeof(palette_ram));
    memset(video_ram, 0, sizeof(uint8_t) * sizeof(video_ram));
    memset(object_ram, 0, sizeof(uint8_t) * sizeof(object_ram));
    memset(backup_flash, 0xff, sizeof(uint8_t) * sizeof(backup_flash));
    memset(backup_sram, 0xff, sizeof(uint8_t) * sizeof(backup_sram));

    memset(r, 0, sizeof(uint32_t) * 16);
    arm_init_registers(skip_bios);
    branch_taken = true;

    ppu_cycles = 0;
    timer_cycles = 0;
    halted = false;
    last_bios_access = 0xe4;

    if (skip_bios) {
        ioreg.io_dispcnt = 0x80;
        ioreg.io_bg2pa = 0x100;
        ioreg.io_bg2pd = 0x100;
        ioreg.io_bg3pa = 0x100;
        ioreg.io_bg3pd = 0x100;
    }
}

void gba_load(const char *filename) {
    gba_reset();

    memset(game_rom, 0, sizeof(uint8_t) * sizeof(game_rom));

    FILE *fp = fopen(filename, "rb");
    assert(fp != NULL);
    fseek(fp, 0, SEEK_END);
    game_rom_size = ftell(fp);
    assert(game_rom_size > 0);
    game_rom_mask = next_power_of_2(game_rom_size) - 1;
    fseek(fp, 0, SEEK_SET);
    fread(game_rom, sizeof(uint8_t), game_rom_size, fp);
    fclose(fp);

    gba_detect_cartridge_features();
}

Uint32 rgb555(uint32_t pixel) {
    uint32_t r, g, b;
    r = pixel & 0x1f;
    g = (pixel >> 5) & 0x1f;
    b = (pixel >> 10) & 0x1f;
    r = (r << 3) | (r >> 2);
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);
    return 0xff << 24 | b << 16 | g << 8 | r;
}

void gba_draw_blank(int y) {
    Uint32 clear_color = rgb555(0);

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        screen_pixels[y][x] = clear_color;
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
            bool pflip = (ioreg.io_dispcnt & DCNT_PAGE) != 0;
            uint8_t pixel_index = video_ram[(pflip ? 0xa000 : 0) + y * SCREEN_WIDTH + x];
            pixel = *(uint16_t *)&palette_ram[pixel_index * 2];
        }
        screen_pixels[y][x] = rgb555(pixel);
    }
}

void gba_draw_tiled_cull(int x, int y, int h, uint32_t pixel) {
    x -= h;
    if (x < 0 || x >= SCREEN_WIDTH) return;
    screen_pixels[y][x] = rgb555(pixel);
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
            bool visible = (ioreg.io_dispcnt & (1 << (8 + bg))) != 0;
            if (!visible) continue;
            uint16_t bgcnt = io_read_halfword(REG_BG0CNT + 2 * bg);
            uint16_t priority = bgcnt & 3;
            if (priority == pri) {
                uint16_t hofs, vofs;
                switch (bg) {
                    case 0: hofs = ioreg.io_bg0hofs; vofs = ioreg.io_bg0vofs; break;
                    case 1: hofs = ioreg.io_bg1hofs; vofs = ioreg.io_bg1vofs; break;
                    case 2: hofs = ioreg.io_bg2hofs; vofs = ioreg.io_bg2vofs; break;
                    case 3: hofs = ioreg.io_bg3hofs; vofs = ioreg.io_bg3vofs; break;
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
    gba_draw_blank(ioreg.io_vcount);  // FIXME forced blank

    uint32_t mode = ioreg.io_dispcnt & 7;
    switch (mode) {
        case 0:
        case 1:
        case 2:
        //case 6:
        //case 7:
            gba_draw_tiled(mode, ioreg.io_vcount);
            break;

        case 3:
        case 4:
        case 5:
            gba_draw_bitmap(mode, ioreg.io_vcount);
            break;

        default:
            assert(false);
            break;
    }
}

void gba_ppu_update(void) {
    if (ppu_cycles % 1232 == 0) {
        ioreg.io_dispstat &= ~DSTAT_IN_HBL;
        if (ioreg.io_vcount < SCREEN_HEIGHT) {
            gba_draw_scanline();
        }
        ioreg.io_vcount = (ioreg.io_vcount + 1) % 228;
        if (ioreg.io_vcount == 0) {
            ioreg.io_dispstat &= ~DSTAT_IN_VBL;
        } else if (ioreg.io_vcount == 160) {
            if ((ioreg.io_dispstat & DSTAT_IN_VBL) == 0) {
                ioreg.io_dispstat |= DSTAT_IN_VBL;
                if ((ioreg.io_dispstat & DSTAT_VBL_IRQ) != 0 && ioreg.io_ime == 1 && (ioreg.io_ie & INT_VBLANK) != 0) {
                    ioreg.io_if |= INT_VBLANK;
                }
            }
        }
        if (ioreg.io_vcount == (uint8_t)(ioreg.io_dispstat >> 8)) {
            if ((ioreg.io_dispstat & DSTAT_IN_VCT) == 0) {
                ioreg.io_dispstat |= DSTAT_IN_VCT;
                if ((ioreg.io_dispstat & DSTAT_VCT_IRQ) != 0 && ioreg.io_ime == 1 && (ioreg.io_ie & INT_VCOUNT) != 0) {
                    ioreg.io_if |= INT_VCOUNT;
                }
            }
        } else {
            ioreg.io_dispstat &= ~DSTAT_IN_VCT;
        }
    }
    if (ppu_cycles % 1232 == 960) {
        if ((ioreg.io_dispstat & DSTAT_IN_HBL) == 0) {
            ioreg.io_dispstat |= DSTAT_IN_HBL;
            if ((ioreg.io_dispstat & DSTAT_HBL_IRQ) != 0 && ioreg.io_ime == 1 && (ioreg.io_ie & INT_HBLANK) != 0) {
                ioreg.io_if |= INT_HBLANK;
            }
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
            case 0: counter = &ioreg.timer_0_counter; reload = &ioreg.timer_0_reload; control = &ioreg.timer_0_control; break;
            case 1: counter = &ioreg.timer_1_counter; reload = &ioreg.timer_1_reload; control = &ioreg.timer_1_control; break;
            case 2: counter = &ioreg.timer_2_counter; reload = &ioreg.timer_2_reload; control = &ioreg.timer_2_control; break;
            case 3: counter = &ioreg.timer_3_counter; reload = &ioreg.timer_3_reload; control = &ioreg.timer_3_control; break;
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
                    if ((*control & (1 << 6)) != 0 && ioreg.io_ime == 1 && (ioreg.io_ie & (1 << (3 + i))) != 0) {
                        ioreg.io_if |= 1 << (3 + i);
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
            case 0: dmacnt = ioreg.io_dma0cnt_l | ioreg.io_dma0cnt_h << 16; dst_addr = &ioreg.io_dma0dad; src_addr = &ioreg.io_dma0sad; break;
            case 1: dmacnt = ioreg.io_dma1cnt_l | ioreg.io_dma1cnt_h << 16; dst_addr = &ioreg.io_dma1dad; src_addr = &ioreg.io_dma1sad; break;
            case 2: dmacnt = ioreg.io_dma2cnt_l | ioreg.io_dma2cnt_h << 16; dst_addr = &ioreg.io_dma2dad; src_addr = &ioreg.io_dma2sad; break;
            case 3: dmacnt = ioreg.io_dma3cnt_l | ioreg.io_dma3cnt_h << 16; dst_addr = &ioreg.io_dma3dad; src_addr = &ioreg.io_dma3sad; break;
            default: abort();
        }

        if ((dmacnt & DMA_ENABLE) == 0) continue;

        uint32_t start_timing = (dmacnt >> 28) & 3;
        uint32_t dst_ctrl = (dmacnt >> 21) & 3;
        uint32_t src_ctrl = (dmacnt >> 23) & 3;
        bool transfer_word = (dmacnt & DMA_32) != 0;
        uint32_t count = dmacnt & 0xffff;
        if (count == 0) count = (ch == 3 ? 0x10000 : 0x4000);

        if (start_timing == DMA_AT_VBLANK && ioreg.io_vcount != 160) continue;
        if (start_timing == DMA_AT_HBLANK && ppu_cycles % 1232 != 960) continue;
        if (start_timing == DMA_AT_REFRESH) {
            if (ch == 1 || ch == 2) {
                assert(*dst_addr == 0x40000a0 || *dst_addr == 0x40000a4);
                assert((dmacnt & DMA_REPEAT) != 0);
                if (*dst_addr == 0x40000a0 && !ioreg.fifo_a_refill) continue;
                if (*dst_addr == 0x40000a4 && !ioreg.fifo_b_refill) continue;
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

        if ((dmacnt & DMA_IRQ) != 0 && ioreg.io_ime == 1 && (ioreg.io_ie & (1 << (8 + ch))) != 0) {
            ioreg.io_if |= 1 << (8 + ch);
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

        if (!branch_taken && (cpsr & PSR_I) == 0 && ioreg.io_if != 0) {
            arm_hardware_interrupt();
            halted = false;
        }
    }
}

// Main code
int main(int argc, char **argv) {
    UNUSED(argc);
    UNUSED(argv);

    arm_init_lookup();
    thumb_init_lookup();

    FILE *fp = fopen("system_rom.bin", "rb");
    assert(fp != NULL);
    fread(system_rom, sizeof(uint8_t), 0x4000, fp);
    fclose(fp);

    gba_reset();

    // Setup SDL
    // (Some versions of SDL before 2.0.10 appear to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled... updating to latest version of SDL is recommended!)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char *glsl_version = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char *glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);  // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window *window = SDL_CreateWindow("ygba", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    if (window == NULL) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);  // Enable vsync

    // Enable drag and drop
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    // Initialize OpenGL loader
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
    bool err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
    bool err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
    bool err = gladLoadGL() == 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD2)
    bool err = gladLoadGL((GLADloadfunc) SDL_GL_GetProcAddress) == 0;  // glad2 recommend using the windowing library loader instead of the (optionally) bundled one.
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
    bool err = false;
    glbinding::Binding::initialize();
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
    bool err = false;
    glbinding::initialize([](const char *name) { return (glbinding::ProcAddress) SDL_GL_GetProcAddress(name); });
#else
    bool err = false;  // If you use IMGUI_IMPL_OPENGL_LOADER_CUSTOM, your loader is likely to require some form of initialization.
#endif
    if (err) {
        SDL_Log("Failed to initialize OpenGL loader");
        exit(EXIT_FAILURE);
    }
    SDL_Log("OpenGL version: %s", (char *) glGetString(GL_VERSION));

    // Create textures
    glGenTextures(1, &screen_texture);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("lib/imgui/misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("lib/imgui/misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("lib/imgui/misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("lib/imgui/misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont *font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    // Our state
    bool show_demo_window = false;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    while (!done) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                done = true;
            } else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) {
                done = true;
            } else if (event.type == SDL_DROPFILE) {
                char *dropped_file = event.drop.file;
                gba_load(dropped_file);
                SDL_free(dropped_file);
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window) {
            ImGui::ShowDemoWindow(&show_demo_window);
        }

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");  // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");  // Display some text (you can use format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);  // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);  // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float *) &clear_color);  // Edit 3 floats representing a color

            if (ImGui::Button("Button")) {  // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            }
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window) {
            ImGui::Begin("Another Window", &show_another_window);  // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me")) {
                show_another_window = false;
            }
            ImGui::End();
        }

        const Uint8 *state = SDL_GetKeyboardState(NULL);
        if (state[SDL_SCANCODE_ESCAPE]) {
            done = true;
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

        gba_emulate();

        glBindTexture(GL_TEXTURE_2D, screen_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, screen_pixels);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Screen
        ImGui::Begin("Screen");
        ImGui::SliderInt("Scale", &screen_scale, 1, 5);
        ImVec2 screen_size = ImVec2((float) SCREEN_WIDTH * screen_scale, (float) SCREEN_HEIGHT * screen_scale);
        ImVec2 uv0 = ImVec2(0.0f, 0.0f);
        ImVec2 uv1 = ImVec2(1.0f, 1.0f);
        ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        ImVec4 border_col = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        ImGui::Image((void *)(intptr_t) screen_texture, screen_size, uv0, uv1, tint_col, border_col);
        ImGui::End();

        // Settings
        ImGui::Begin("Settings");
        ImGui::Checkbox("Has EEPROM", &has_eeprom);
        ImGui::Checkbox("Has Flash", &has_flash);
        ImGui::Checkbox("Has SRAM", &has_sram);
        ImGui::Checkbox("Skip BIOS", &skip_bios);
        if (ImGui::Button("Reset")) {
            gba_reset();
        }
        ImGui::End();

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int) io.DisplaySize.x, (int) io.DisplaySize.y);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    glDeleteTextures(1, &screen_texture);

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
