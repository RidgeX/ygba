// gcc -std=c99 -Wall -Wextra -Wpedantic -O2 -o gba gba.c -lmingw32 -lSDL2main -lSDL2 && gba

#include <SDL2/SDL.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cpu.h"

bool single_step = false;
uint64_t start_logging_at = 0;
uint64_t cycles = 0;
uint32_t last_bios_access = 0xe4;

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

#define MEM_IO        0x4000000
#define MEM_VRAM      0x6000000

#define REG_DISPCNT    (MEM_IO + 0)
    #define DCNT_GB        (1 << 3)
    #define DCNT_PAGE      (1 << 4)
    #define DCNT_OAM_HBL   (1 << 5)
    #define DCNT_OBJ_1D    (1 << 6)
    #define DCNT_BLANK     (1 << 7)
    #define DCNT_BG0       (1 << 8)
    #define DCNT_BG1       (1 << 9)
    #define DCNT_BG2       (1 << 10)
    #define DCNT_BG3       (1 << 11)
    #define DCNT_OBJ       (1 << 12)
    #define DCNT_WIN0      (1 << 13)
    #define DCNT_WIN1      (1 << 14)
    #define DCNT_WINOBJ    (1 << 15)
#define REG_DISPSTAT   (MEM_IO + 4)
    #define DSTAT_IN_VBL   (1 << 0)
    #define DSTAT_IN_HBL   (1 << 1)
    #define DSTAT_IN_VCT   (1 << 2)
    #define DSTAT_VBL_IRQ  (1 << 3)
    #define DSTAT_HBL_IRQ  (1 << 4)
    #define DSTAT_VCT_IRQ  (1 << 5)
#define REG_VCOUNT     (MEM_IO + 6)
#define REG_BG0CNT     (MEM_IO + 8)
#define REG_BG1CNT     (MEM_IO + 0xa)
#define REG_BG2CNT     (MEM_IO + 0xc)
#define REG_BG3CNT     (MEM_IO + 0xe)
#define REG_BG0HOFS    (MEM_IO + 0x10)
#define REG_BG0VOFS    (MEM_IO + 0x12)
#define REG_BG1HOFS    (MEM_IO + 0x14)
#define REG_BG1VOFS    (MEM_IO + 0x16)
#define REG_BG2HOFS    (MEM_IO + 0x18)
#define REG_BG2VOFS    (MEM_IO + 0x1a)
#define REG_BG3HOFS    (MEM_IO + 0x1c)
#define REG_BG3VOFS    (MEM_IO + 0x1e)
#define IO_WIN0H       (MEM_IO+0x0040)
#define IO_WIN1H       (MEM_IO+0x0042)
#define IO_WIN0V       (MEM_IO+0x0044)
#define IO_WIN1V       (MEM_IO+0x0046)
#define IO_WININ       (MEM_IO+0x0048)
#define IO_WINOUT      (MEM_IO+0x004a)
#define IO_BLDCNT      (MEM_IO+0x0050)
#define IO_BLDALPHA    (MEM_IO+0x0052)
#define IO_BLDY        (MEM_IO+0x0054)
#define IO_SOUNDBIAS   (MEM_IO+0x0088)
#define REG_DMA0SAD    (MEM_IO + 0xb0)
#define REG_DMA0DAD    (MEM_IO + 0xb4)
#define REG_DMA0CNT_L  (MEM_IO + 0xb8)
#define REG_DMA0CNT_H  (MEM_IO + 0xba)
#define REG_DMA1SAD    (MEM_IO + 0xbc)
#define REG_DMA1DAD    (MEM_IO + 0xc0)
#define REG_DMA1CNT_L  (MEM_IO + 0xc4)
#define REG_DMA1CNT_H  (MEM_IO + 0xc6)
#define REG_DMA2SAD    (MEM_IO + 0xc8)
#define REG_DMA2DAD    (MEM_IO + 0xcc)
#define REG_DMA2CNT_L  (MEM_IO + 0xd0)
#define REG_DMA2CNT_H  (MEM_IO + 0xd2)
#define REG_DMA3SAD    (MEM_IO + 0xd4)
#define REG_DMA3DAD    (MEM_IO + 0xd8)
#define REG_DMA3CNT_L  (MEM_IO + 0xdc)
#define REG_DMA3CNT_H  (MEM_IO + 0xde)
    #define DMA_INC        0
    #define DMA_DEC        1
    #define DMA_FIXED      2
    #define DMA_RELOAD     3
    #define DMA_REPEAT     (1 << 25)
    #define DMA_32         (1 << 26)
    #define DMA_NOW        0
    #define DMA_AT_VBLANK  1
    #define DMA_AT_HBLANK  2
    #define DMA_AT_REFRESH 3
    #define DMA_IRQ        (1 << 30)
    #define DMA_ENABLE     (1 << 31)
#define REG_SIODATA32  (MEM_IO + 0x120)
#define REG_SIOCNT     (MEM_IO + 0x128)
#define REG_KEYINPUT   (MEM_IO + 0x130)
#define REG_KEYCNT     (MEM_IO + 0x132)
#define REG_RCNT       (MEM_IO + 0x134)
#define REG_IE         (MEM_IO + 0x200)
#define REG_IF         (MEM_IO + 0x202)
#define REG_WAITCNT    (MEM_IO + 0x204)
#define REG_IME        (MEM_IO + 0x208)
#define REG_POSTFLG    (MEM_IO + 0x300)
#define REG_HALTCNT    (MEM_IO + 0x301)

uint16_t io_dispcnt;
uint16_t io_dispstat;
uint16_t io_vcount;
uint16_t io_bg0cnt;
uint16_t io_bg1cnt;
uint16_t io_bg2cnt;
uint16_t io_bg3cnt;
uint16_t io_bg0hofs, io_bg0vofs;
uint16_t io_bg1hofs, io_bg1vofs;
uint16_t io_bg2hofs, io_bg2vofs;
uint16_t io_win0h;
uint16_t io_win1h;
uint16_t io_win0v;
uint16_t io_win1v;
uint16_t io_winin;
uint16_t io_winout;
uint16_t io_soundbias;
uint32_t io_dma0sad, io_dma0dad, io_dma0cnt;
uint32_t io_dma1sad, io_dma1dad, io_dma1cnt;
uint32_t io_dma2sad, io_dma2dad, io_dma2cnt;
uint32_t io_dma3sad, io_dma3dad, io_dma3cnt;
uint16_t io_keyinput;
uint16_t io_keycnt;
//uint16_t io_rcnt;
uint16_t io_ie;
uint16_t io_if;
//uint16_t io_waitcnt;
uint16_t io_ime;
uint8_t io_haltcnt;

uint8_t io_read_byte(uint32_t address) {
    switch (address) {
        case REG_VCOUNT:
            return (uint8_t) io_vcount;

        default:
            printf("io_read_byte(0x%08x);\n", address);
            return 0;
    }
}

void io_write_byte(uint32_t address, uint8_t value) {
    switch (address) {
        //case IO_DISPSTAT + 1:
        //    io_dispstat = (io_dispstat & 0xff) | value << 8;
        //    printf("DISPSTAT = 0x%04x\n", io_dispstat);
        //    break;

        case REG_IF:
            io_if = (io_if & 0xff00) | value;  // FIXME? xor value
            break;

        case REG_IME:
            io_ime = (io_ime & 0xff00) | value;
            break;

        case REG_HALTCNT:
            io_haltcnt = value;
            halted = true;
            break;

        default:
            printf("io_write_byte(0x%08x, 0x%02x);\n", address, value);
            break;
    }
}

uint16_t io_read_halfword(uint32_t address) {
    switch (address) {
        case REG_DISPCNT: return io_dispcnt;
        case REG_DISPSTAT: return io_dispstat;
        case REG_VCOUNT: return io_vcount;
        case REG_BG0CNT: return io_bg0cnt;
        case REG_BG1CNT: return io_bg1cnt;
        case REG_BG2CNT: return io_bg2cnt;
        case REG_BG3CNT: return io_bg3cnt;
        case REG_BG0HOFS: return io_bg0hofs;
        case REG_BG0VOFS: return io_bg0vofs;
        case REG_BG1HOFS: return io_bg1hofs;
        case REG_BG1VOFS: return io_bg1vofs;

        case IO_SOUNDBIAS:
            return io_soundbias;

        //case IO_DMA0CNT_H:
        //    return io_dma0cnt_h;

        //case IO_DMA1CNT_H:
        //    return io_dma1cnt_h;

        //case IO_DMA2CNT_H:
        //    return io_dma2cnt_h;

        //case IO_DMA3CNT_H:
        //    return io_dma3cnt_h;

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
            printf("io_read_halfword(0x%08x);\n", address);
            return 0;
    }
}

void io_write_halfword(uint32_t address, uint16_t value) {
    switch (address) {
        case REG_DISPCNT:
            io_dispcnt = (io_dispcnt & DCNT_GB) | (value & ~DCNT_GB);
            break;

        case REG_DISPSTAT:
            io_dispstat = (io_dispstat & 7) | (value & ~7);
            break;

        case REG_BG0CNT: io_bg0cnt = value; break;
        case REG_BG1CNT: io_bg1cnt = value; break;
        case REG_BG2CNT: io_bg2cnt = value; break;
        case REG_BG3CNT: io_bg3cnt = value; break;
        case REG_BG0HOFS: io_bg0hofs = value; break;
        case REG_BG0VOFS: io_bg0vofs = value; break;
        case REG_BG1HOFS: io_bg1hofs = value; break;
        case REG_BG1VOFS: io_bg1vofs = value; break;
        case REG_BG2HOFS: io_bg2hofs = value; break;
        case REG_BG2VOFS: io_bg2vofs = value; break;

        case IO_WIN0H:
            io_win0h = value;
            //printf("WIN0H = 0x%04x\n", io_win0h);
            break;

        case IO_WIN1H:
            io_win1h = value;
            //printf("WIN1H = 0x%04x\n", io_win1h);
            break;

        case IO_WIN0V:
            io_win0v = value;
            //printf("WIN0V = 0x%04x\n", io_win0v);
            break;

        case IO_WIN1V:
            io_win1v = value;
            //printf("WIN1V = 0x%04x\n", io_win1v);
            break;

        case IO_WININ:
            io_winin = value;
            printf("WININ = 0x%04x\n", io_winin);
            break;

        case IO_WINOUT:
            io_winout = value;
            printf("WINOUT = 0x%04x\n", io_winout);
            break;

        case IO_SOUNDBIAS:
            io_soundbias = value;
            //printf("SOUNDBIAS = 0x%04x\n", io_soundbias);
            break;

        case REG_DMA1CNT_H:
            io_dma1cnt = (io_dma1cnt & 0xffff) | value << 16;
            break;

        case REG_DMA2CNT_H:
            io_dma2cnt = (io_dma2cnt & 0xffff) | value << 16;
            break;

        case REG_DMA3CNT_L:
            io_dma3cnt = (io_dma3cnt & 0xffff0000) | value;
            break;

        case REG_DMA3CNT_H:
            io_dma3cnt = (io_dma3cnt & 0xffff) | value << 16;
            break;

        case REG_IE: io_ie = value; break;
        case REG_IF: io_if = value; break;  // FIXME? xor value

        //case IO_WAITCNT:
        //    io_waitcnt = value;
        //    printf("WAITCNT = 0x%04x\n", io_waitcnt);
        //    break;

        case REG_IME: io_ime = value; break;

        default:
            printf("io_write_halfword(0x%08x, 0x%04x);\n", address, value);
            break;
    }
}

uint32_t io_read_word(uint32_t address) {
    switch (address) {
        case REG_DISPCNT:
            return io_dispcnt;  // FIXME green swap

        case REG_DISPSTAT:
            return io_dispstat | io_vcount << 16;

        case REG_DMA0SAD: return io_dma0sad;
        case REG_DMA0DAD: return io_dma0dad;
        case REG_DMA0CNT_L: return io_dma0cnt;
        case REG_DMA1SAD: return io_dma1sad;
        case REG_DMA1DAD: return io_dma1dad;
        case REG_DMA1CNT_L: return io_dma1cnt;
        case REG_DMA2SAD: return io_dma2sad;
        case REG_DMA2DAD: return io_dma2dad;
        case REG_DMA2CNT_L: return io_dma2cnt;
        case REG_DMA3SAD: return io_dma3sad;
        case REG_DMA3DAD: return io_dma3dad;
        case REG_DMA3CNT_L: return io_dma3cnt;

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
            printf("io_read_word(0x%08x);\n", address);
            return 0;
    }
}

void io_write_word(uint32_t address, uint32_t value) {
    switch (address) {
        case REG_DISPCNT:
            io_dispcnt = (io_dispcnt & DCNT_GB) | ((uint16_t) value & ~DCNT_GB);  // FIXME green swap
            break;

        case REG_BG0CNT:
            io_bg0cnt = (uint16_t) value;
            io_bg1cnt = (uint16_t)(value >> 16);
            break;

        case REG_BG0HOFS:
            io_bg0hofs = (uint16_t) value;
            io_bg0vofs = (uint16_t)(value >> 16);
            break;

        case REG_DMA0SAD: io_dma0sad = value; break;
        case REG_DMA0DAD: io_dma0dad = value; break;
        case REG_DMA0CNT_L: io_dma0cnt = value; break;
        case REG_DMA1SAD: io_dma1sad = value; break;
        case REG_DMA1DAD: io_dma1dad = value; break;
        case REG_DMA1CNT_L: io_dma1cnt = value; break;
        case REG_DMA2SAD: io_dma2sad = value; break;
        case REG_DMA2DAD: io_dma2dad = value; break;
        case REG_DMA2CNT_L: io_dma2cnt = value; break;
        case REG_DMA3SAD: io_dma3sad = value; break;
        case REG_DMA3DAD: io_dma3dad = value; break;
        case REG_DMA3CNT_L: io_dma3cnt = value; break;

        case REG_IE:
            io_ie = (uint16_t) value;
            io_if = (uint16_t)(value >> 16);
            break;

        case REG_IME:
            io_ime = (uint16_t) value;
            break;

        default:
            printf("io_write_word(0x%08x, 0x%08x);\n", address, value);
            break;
    }
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
        return io_read_byte(address);
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
        return game_rom[address & 0x1ffffff];
    }
    printf("memory_read_byte(0x%08x);\n", address);
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
        io_write_byte(address, value);
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
    printf("memory_write_byte(0x%08x, 0x%02x);\n", address, value);
}

uint16_t memory_read_halfword(uint32_t address) {
    assert((address & 1) == 0);
    if (address < 0x4000) {
        if (r[15] < 0x4000) last_bios_access = address;
        return *(uint16_t *)&system_rom[last_bios_access];
    }
    if (address >= 0x02000000 && address < 0x03000000) {
        return *(uint16_t *)&cpu_ewram[address & 0x3ffff];
    }
    if (address >= 0x03000000 && address < 0x04000000) {
        return *(uint16_t *)&cpu_iwram[address & 0x7fff];
    }
    if (address >= 0x04000000 && address < 0x05000000) {
        return io_read_halfword(address);
    }
    if (address >= 0x05000000 && address < 0x06000000) {
        return *(uint16_t *)&palette_ram[address & 0x3ff];
    }
    if (address >= 0x06000000 && address < 0x07000000) {
        address &= 0x1ffff;
        if (address >= 0x18000) address -= 0x18000;
        return *(uint16_t *)&video_ram[address];
    }
    if (address >= 0x07000000 && address < 0x08000000) {
        return *(uint16_t *)&object_ram[address & 0x3ff];
    }
    if (address >= 0x08000000 && address < 0x0e000000) {
        return *(uint16_t *)&game_rom[address & 0x1ffffff];
    }
    printf("memory_read_halfword(0x%08x);\n", address);
    return 0;
}

void memory_write_halfword(uint32_t address, uint16_t value) {
    address &= ~1;
    if (address < 0x4000) {
        //return;  // Read only
    }
    if (address >= 0x02000000 && address < 0x03000000) {
        *(uint16_t *)&cpu_ewram[address & 0x3ffff] = value;
        return;
    }
    if (address >= 0x03000000 && address < 0x04000000) {
        *(uint16_t *)&cpu_iwram[address & 0x7fff] = value;
        return;
    }
    if (address >= 0x04000000 && address < 0x05000000) {
        io_write_halfword(address, value);
        return;
    }
    if (address >= 0x05000000 && address < 0x06000000) {
        *(uint16_t *)&palette_ram[address & 0x3ff] = value;
        return;
    }
    if (address >= 0x06000000 && address < 0x07000000) {
        address &= 0x1ffff;
        if (address >= 0x18000) address -= 0x18000;
        *(uint16_t *)&video_ram[address] = value;
        return;
    }
    if (address >= 0x07000000 && address < 0x08000000) {
        *(uint16_t *)&object_ram[address & 0x3ff] = value;
        return;
    }
    if (address >= 0x08000000 && address < 0x0e000000) {
        //return;  // Read only
    }
    printf("memory_write_halfword(0x%08x, 0x%04x);\n", address, value);
}

uint32_t memory_read_word(uint32_t address) {
    address &= ~3;
    if (address < 0x4000) {
        if (r[15] < 0x4000) last_bios_access = address;
        return *(uint32_t *)&system_rom[last_bios_access];
    }
    if (address < 0x02000000) {
        //return 0;  // Unmapped
    }
    if (address >= 0x02000000 && address < 0x03000000) {
        return *(uint32_t *)&cpu_ewram[address & 0x3ffff];
    }
    if (address >= 0x03000000 && address < 0x04000000) {
        return *(uint32_t *)&cpu_iwram[address & 0x7fff];
    }
    if (address >= 0x04000000 && address < 0x05000000) {
        return io_read_word(address);
    }
    if (address >= 0x05000000 && address < 0x06000000) {
        return *(uint32_t*)&palette_ram[address & 0x3ff];
    }
    if (address >= 0x06000000 && address < 0x07000000) {
        address &= 0x1ffff;
        if (address >= 0x18000) address -= 0x18000;
        return *(uint32_t*)&video_ram[address];
    }
    if (address >= 0x07000000 && address < 0x08000000) {
        return *(uint32_t *)&object_ram[address & 0x3ff];
    }
    if (address >= 0x08000000 && address < 0x0e000000) {
        return *(uint32_t *)&game_rom[address & 0x1ffffff];
    }
    if (address >= 0x0e000000 && address < 0x0e010000) {
        // TODO
    }
    printf("memory_read_word(0x%08x);\n", address);
    return 0;
}

void memory_write_word(uint32_t address, uint32_t value) {
    address &= ~3;
    if (address < 0x4000) {
        //return;  // Read only
    }
    if (address < 0x02000000) {
        //return;  // Unmapped
    }
    if (address >= 0x02000000 && address < 0x03000000) {
        *(uint32_t *)&cpu_ewram[address & 0x3ffff] = value;
        return;
    }
    if (address >= 0x03000000 && address < 0x04000000) {
        *(uint32_t *)&cpu_iwram[address & 0x7fff] = value;
        return;
    }
    if (address >= 0x04000000 && address < 0x05000000) {
        io_write_word(address, value);
        return;
    }
    if (address >= 0x05000000 && address < 0x06000000) {
        *(uint32_t *)&palette_ram[address & 0x3ff] = value;
        return;
    }
    if (address >= MEM_VRAM && address < 0x07000000) {
        address &= 0x1ffff;
        if (address >= 0x18000) address -= 0x18000;
        *(uint32_t *)&video_ram[address] = value;
        return;
    }
    if (address >= 0x07000000 && address < 0x08000000) {
        *(uint32_t *)&object_ram[address & 0x3ff] = value;
        return;
    }
    if (address >= 0x08000000 && address < 0x0e000000) {
        //return;  // Read only
    }
    printf("memory_write_word(0x%08x, 0x%08x);\n", address, value);
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
    uint32_t game_rom_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fread(game_rom, sizeof(uint8_t), game_rom_size, fp);
    fclose(fp);

    memset(r, 0, sizeof(uint32_t) * 16);

    arm_init();

    branch_taken = true;
    io_vcount = 227;
}

SDL_Texture *g_texture;
SDL_PixelFormat *g_format;

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
    Uint32 clear_color = rgb555(0x7fff);

    Uint32 *pixels;
    int pitch;
    SDL_LockTexture(g_texture, NULL, (void**) &pixels, &pitch);
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        pixels[y * (pitch / 4) + x] = clear_color;
    }
    SDL_UnlockTexture(g_texture);
}

void gba_draw_bitmap(uint32_t mode, int y) {
    int width = (mode == 5 ? 160 : SCREEN_WIDTH);

    Uint32 *pixels;
    int pitch;
    SDL_LockTexture(g_texture, NULL, (void**) &pixels, &pitch);
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
        pixels[y * (pitch / 4) + x] = rgb555(pixel);
    }
    SDL_UnlockTexture(g_texture);
}

void gba_draw_tiled_final(Uint32 *pixels, int pitch, int x, int y/*, int h, int v*/, uint32_t pixel) {
    //x -= h;
    //y -= v;
    //if (x < 0 || x >= SCREEN_WIDTH) return;
    //if (y < 0 || y >= SCREEN_HEIGHT) return;
    pixels[y * (pitch / 4) + x] = rgb555(pixel);
}

void gba_draw_tiled_bg(uint32_t mode, int bg, int y, uint32_t tile_base, uint32_t map_base) {
    assert(mode == 0);

    int h = io_read_halfword(REG_BG0HOFS + 4 * bg);
    int v = io_read_halfword(REG_BG0VOFS + 4 * bg);
    if (h & 0x8000) h |= ~0xffff;
    if (v & 0x8000) v |= ~0xffff;

    Uint32 *pixels;
    int pitch;
    SDL_LockTexture(g_texture, NULL, (void**) &pixels, &pitch);
    for (int x = 0; x < SCREEN_WIDTH; x += 8) {
        uint32_t map_x = x / 8;
        uint32_t map_y = y / 8;
        uint32_t map_address = map_base + (map_y * 32 + map_x) * 2;
        uint16_t info = *(uint16_t *)&video_ram[map_address];
        uint16_t tile_no = info & 0x3ff;
        bool hflip = (info & (1 << 10)) != 0;
        bool vflip = (info & (1 << 11)) != 0;
        uint16_t palette_no = (info >> 12) & 0xf;

        uint32_t tile_address = tile_base + tile_no * 32;
        uint8_t *tile = &video_ram[tile_address];
        for (int i = 0; i < 8; i += 2) {
            uint32_t offset = (vflip ? 7 - (y % 8) : (y % 8)) * 4 + ((hflip ? 7 - i : i) / 2);
            uint8_t pixel_indexes = tile[offset];
            uint8_t pixel_index_0 = (pixel_indexes >> (hflip ? 4 : 0)) & 0xf;
            uint8_t pixel_index_1 = (pixel_indexes >> (hflip ? 0 : 4)) & 0xf;
            if (true) { //pixel_index_0 != 0) {
                uint16_t pixel_0 = *(uint16_t *)&palette_ram[palette_no * 32 + pixel_index_0 * 2];
                gba_draw_tiled_final(pixels, pitch, (x / 8) * 8 + i, y/*, h, v*/, pixel_0);
            }
            if (true) { //pixel_index_1 != 0) {
                uint16_t pixel_1 = *(uint16_t *)&palette_ram[palette_no * 32 + pixel_index_1 * 2];
                gba_draw_tiled_final(pixels, pitch, (x / 8) * 8 + i + 1, y/*, h, v*/, pixel_1);
            }
        }
    }
    SDL_UnlockTexture(g_texture);
}

void gba_draw_tiled(uint32_t mode, int y) {
    for (int pri = 3; pri >= 0; pri--) {  // FIXME? reverse order
        for (int bg = 0; bg < 4; bg++) {
            bool visible = (io_dispcnt & (1 << (8 + bg))) != 0;
            if (!visible) continue;
            uint16_t bgcnt = io_read_halfword(REG_BG0CNT + 2 * bg);
            uint16_t priority = bgcnt & 3;
            if (priority == pri) {
                uint32_t tile_base = ((bgcnt >> 2) & 3) * 16384;
                uint32_t map_base = ((bgcnt >> 8) & 0x1f) * 2048;
                gba_draw_tiled_bg(mode, bg, y, tile_base, map_base);
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
    bool interrupts_enabled = io_ime == 1;  // (cpsr & PSR_I) == 0 FIXME?
    if (cycles % 1232 == 0) {
        io_dispstat &= ~DSTAT_IN_HBL;
        if (io_vcount < 160) {
            gba_draw_scanline();
        }
        io_vcount = (io_vcount + 1) % 228;
        if (io_vcount == 0) {
            io_dispstat &= ~DSTAT_IN_VBL;
        } else if (io_vcount == 160) {
            io_dispstat |= DSTAT_IN_VBL;
            if (interrupts_enabled && (io_dispstat & DSTAT_VBL_IRQ) != 0 && (io_ie & (1 << 0)) != 0) {
                io_if |= 1 << 0;
                arm_hardware_interrupt();
            }
        }
        if (io_vcount == (uint8_t)(io_dispstat >> 8)) {
            io_dispstat |= DSTAT_IN_VCT;
            if (interrupts_enabled && (io_dispstat & DSTAT_VCT_IRQ) != 0 && (io_ie & (1 << 2)) != 0) {
                //io_if |= 1 << 2;
                //arm_hardware_interrupt();
            }
        } else {
            io_dispstat &= ~DSTAT_IN_VCT;
        }
    }
    if (cycles % 1232 == 960) {
        io_dispstat |= DSTAT_IN_HBL;
        if (interrupts_enabled && (io_dispstat & DSTAT_HBL_IRQ) != 0 && (io_ie & (1 << 1)) != 0) {
            //io_if |= 1 << 1;
            //arm_hardware_interrupt();
        }
    }
    cycles = (cycles + 1) % 280896;
}

void gba_dma_transfer_halfwords(uint32_t dst_ctrl, uint32_t src_ctrl, uint32_t dst_addr, uint32_t src_addr, uint32_t len) {
    uint32_t dst_off = 0;
    uint32_t src_off = 0;
    for (uint32_t i = 0; i < len; i++) {
        uint16_t value = memory_read_halfword(src_addr + src_off);
        memory_write_halfword(dst_addr + dst_off, value);
        if (dst_ctrl == DMA_INC) dst_off += 2;
        if (dst_ctrl == DMA_DEC) dst_off -= 2;
        if (src_ctrl == DMA_INC) src_off += 2;
        if (src_ctrl == DMA_DEC) src_off -= 2;
    }
}

void gba_dma_transfer_words(uint32_t dst_ctrl, uint32_t src_ctrl, uint32_t dst_addr, uint32_t src_addr, uint32_t len) {
    uint32_t dst_off = 0;
    uint32_t src_off = 0;
    for (uint32_t i = 0; i < len; i++) {
        uint32_t value = memory_read_word(src_addr + src_off);
        memory_write_word(dst_addr + dst_off, value);
        if (dst_ctrl == DMA_INC) dst_off += 4;
        if (dst_ctrl == DMA_DEC) dst_off -= 4;
        if (src_ctrl == DMA_INC) src_off += 4;
        if (src_ctrl == DMA_DEC) src_off -= 4;
    }
}

void gba_dma_update(void) {
    for (int ch = 0; ch < 4; ch++) {
        uint32_t dmacnt = io_read_word(REG_DMA0CNT_L + 12 * ch);

        if ((dmacnt & DMA_ENABLE) != 0) {
            // FIXME check dma start condition

            uint32_t dst_ctrl = (dmacnt >> 21) & 3;
            uint32_t src_ctrl = (dmacnt >> 23) & 3;
            uint32_t dst_addr = io_read_word(REG_DMA0DAD + 12 * ch);
            uint32_t src_addr = io_read_word(REG_DMA0SAD + 12 * ch);
            uint32_t len = (ch == 3 ? dmacnt & 0xffff : dmacnt & 0x3fff);
            if (len == 0) len = (ch == 3 ? 0x10000 : 0x4000);

            assert(dst_ctrl != DMA_REPEAT);
            assert(src_ctrl != DMA_REPEAT);

            if ((dmacnt & DMA_32) != 0) {
                gba_dma_transfer_words(dst_ctrl, src_ctrl, dst_addr, src_addr, len);
            } else {
                gba_dma_transfer_halfwords(dst_ctrl, src_ctrl, dst_addr, src_addr, len);
            }

            if ((dmacnt & DMA_REPEAT) == 0) {
                dmacnt &= ~DMA_ENABLE;
                io_write_word(REG_DMA0CNT_L + 12 * ch, dmacnt);
            }
        }
    }
}

void gba_emulate(void) {
    while (true) {
        gba_dma_update();
        gba_ppu_update();
        if (cycles == 0) break;

        if (!halted) {
            //if (r[15] == 0x00000300) single_step = true;

#ifdef DEBUG
            if (single_step && instruction_count >= start_logging_at) {
                log_instructions = true;
                log_registers = true;
            }
#endif

            if (cpsr & PSR_T) {
                thumb_step();
            } else {
                arm_step();
            }
            instruction_count++;

#ifdef DEBUG
            if (single_step && instruction_count > start_logging_at) {
                char c = fgetc(stdin);
                if (c == EOF) exit(EXIT_SUCCESS);
            }
#endif
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

        gba_emulate();

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
