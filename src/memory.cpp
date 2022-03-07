// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#include "memory.h"

#include <stdint.h>

#include <fmt/core.h>

#include "backup.h"
#include "cpu.h"
#include "dma.h"
#include "gpio.h"
#include "io.h"
#include "system.h"
#include "video.h"

//#define LOG_BAD_MEMORY_ACCESS

uint8_t system_rom[0x4000];
uint32_t last_bios_access;
uint8_t cpu_ewram[0x40000];
uint8_t cpu_iwram[0x8000];
uint8_t palette_ram[0x400];
uint8_t video_ram[0x18000];
uint8_t object_ram[0x400];
uint8_t game_rom[0x2000000];
uint32_t game_rom_size;
uint32_t game_rom_mask;

uint32_t memory_open_bus() {
    if (dma_active != -1 || get_pc() - dma_pc == SIZEOF_INSTR) {
        return ioreg.dma[dma_active].value.dw;
    } else if (!FLAG_T()) {
        return arm_pipeline[1];
    } else {
        return thumb_pipeline[1] | thumb_pipeline[1] << 16;
    }
}

uint8_t rom_read_byte(uint32_t address) {
    if (address > game_rom_mask) return (uint8_t) ((uint16_t) (address >> 1) >> 8 * (address & 1));
    return game_rom[address & game_rom_mask];
}

uint16_t rom_read_halfword(uint32_t address) {
    if (address > game_rom_mask) return (uint16_t) (address >> 1);
    return *(uint16_t *) &game_rom[address & (game_rom_mask & ~1)];
}

uint32_t rom_read_word(uint32_t address) {
    if (address > game_rom_mask) return (uint16_t) ((address & ~2) >> 1) | (uint16_t) ((address | 2) >> 1) << 16;
    return *(uint32_t *) &game_rom[address & (game_rom_mask & ~3)];
}

uint8_t memory_read_byte(uint32_t address, bool peek) {
    UNUSED(peek);
    uint8_t region = address >> 24;
    switch (region) {
        case 0:
            if (address >= 0x4000) break;
            if (get_pc() < 0x4000) last_bios_access = address;
            return system_rom[last_bios_access];
        case 2:
            return cpu_ewram[address & 0x3ffff];
        case 3:
            return cpu_iwram[address & 0x7fff];
        case 4:
            return io_read_byte(address & 0x3ffffff);
        case 5:
            return palette_ram[address & 0x3ff];
        case 6:
            address &= 0x1ffff;
            if (video_in_bitmap_mode() && address >= 0x18000) return 0;  // No VRAM OBJ mirror in bitmap mode
            if (address >= 0x18000) address -= 0x8000;
            return video_ram[address];
        case 7:
            return object_ram[address & 0x3ff];
        case 8:
        case 9:
        case 0xa:
        case 0xb:
        case 0xc:
        case 0xd:
            return rom_read_byte(address & 0x1ffffff);
        case 0xe:
        case 0xf:
            return backup_read_byte(address & 0xffff);
        default:
            break;
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    fmt::print("memory_read_byte(0x{:08x});\n", address);
#endif
    return (uint8_t) (memory_open_bus() >> 8 * (address & 3));
}

void memory_write_byte(uint32_t address, uint8_t value, bool poke) {
    UNUSED(poke);
    uint8_t region = address >> 24;
    switch (region) {
        case 0:
            if (address >= 0x4000) break;
            return;  // Read only
        case 2:
            cpu_ewram[address & 0x3ffff] = value;
            return;
        case 3:
            cpu_iwram[address & 0x7fff] = value;
            return;
        case 4:
            io_write_byte(address & 0x3ffffff, value);
            return;
        case 5:
            *(uint16_t *) &palette_ram[address & 0x3fe] = value | value << 8;
            return;
        case 6:
            address &= 0x1fffe;
            if (video_in_bitmap_mode() && address >= 0x18000) return;             // No VRAM OBJ mirror in bitmap mode
            if (address >= (video_in_bitmap_mode() ? 0x14000 : 0x10000)) return;  // VRAM OBJ 8-bit write ignored
            *(uint16_t *) &video_ram[address] = value | value << 8;
            return;
        case 7:
            return;  // OAM 8-bit write ignored
        case 8:
        case 9:
        case 0xa:
        case 0xb:
        case 0xc:
        case 0xd:
            return;  // Read only
        case 0xe:
        case 0xf:
            backup_write_byte(address & 0xffff, value);
            return;
        default:
            break;
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    fmt::print("memory_write_byte(0x{:08x}, 0x{:02x});\n", address, value);
#endif
}

uint16_t memory_read_halfword(uint32_t address, bool peek) {
    UNUSED(peek);
    uint8_t region = address >> 24;
    switch (region) {
        case 0:
            if (address >= 0x4000) break;
            if (get_pc() < 0x4000) last_bios_access = address & 0x3ffe;
            return *(uint16_t *) &system_rom[last_bios_access];
        case 2:
            return *(uint16_t *) &cpu_ewram[address & 0x3fffe];
        case 3:
            return *(uint16_t *) &cpu_iwram[address & 0x7ffe];
        case 4:
            return io_read_halfword(address & 0x3fffffe);
        case 5:
            return *(uint16_t *) &palette_ram[address & 0x3fe];
        case 6:
            address &= 0x1fffe;
            if (video_in_bitmap_mode() && address >= 0x18000) return 0;  // No VRAM OBJ mirror in bitmap mode
            if (address >= 0x18000) address -= 0x8000;
            return *(uint16_t *) &video_ram[address];
        case 7:
            return *(uint16_t *) &object_ram[address & 0x3fe];
        case 8:
        case 9:
        case 0xa:
        case 0xb:
        case 0xc:
        case 0xd:
            if (has_eeprom && address >= (game_rom_size <= 0x1000000 ? 0x0d000000 : 0x0dffff00)) {
                return eeprom_read_bit();
            }
            if (has_rtc && (address >= 0x080000c4 && address < 0x080000ca)) {
                return gpio_read_halfword(address & 0x1fffffe);
            }
            return rom_read_halfword(address & 0x1fffffe);
        case 0xe:
        case 0xf:
            return backup_read_halfword(address & 0xffff);
        default:
            break;
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    fmt::print("memory_read_halfword(0x{:08x});\n", address);
#endif
    return (uint16_t) (memory_open_bus() >> 8 * (address & 2));
}

void memory_write_halfword(uint32_t address, uint16_t value, bool poke) {
    UNUSED(poke);
    uint8_t region = address >> 24;
    switch (region) {
        case 0:
            if (address >= 0x4000) break;
            return;  // Read only
        case 2:
            *(uint16_t *) &cpu_ewram[address & 0x3fffe] = value;
            return;
        case 3:
            *(uint16_t *) &cpu_iwram[address & 0x7ffe] = value;
            return;
        case 4:
            io_write_halfword(address & 0x3fffffe, value);
            return;
        case 5:
            *(uint16_t *) &palette_ram[address & 0x3fe] = value;
            return;
        case 6:
            address &= 0x1fffe;
            if (video_in_bitmap_mode() && address >= 0x18000) return;  // No VRAM OBJ mirror in bitmap mode
            if (address >= 0x18000) address -= 0x8000;
            *(uint16_t *) &video_ram[address] = value;
            return;
        case 7:
            *(uint16_t *) &object_ram[address & 0x3fe] = value;
            return;
        case 8:
        case 9:
        case 0xa:
        case 0xb:
        case 0xc:
        case 0xd:
            if (has_eeprom && address >= (game_rom_size <= 0x1000000 ? 0x0d000000 : 0x0dffff00)) {
                eeprom_write_bit(value);
                return;
            }
            if (has_rtc && (address >= 0x080000c4 && address < 0x080000ca)) {
                gpio_write_halfword(address & 0x1fffffe, value);
                return;
            }
            return;  // Read only
        case 0xe:
        case 0xf:
            backup_write_halfword(address & 0xffff, value);
        default:
            break;
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    fmt::print("memory_write_halfword(0x{:08x}, 0x{:04x});\n", address, value);
#endif
}

uint32_t memory_read_word(uint32_t address, bool peek) {
    UNUSED(peek);
    uint8_t region = address >> 24;
    switch (region) {
        case 0:
            if (address >= 0x4000) break;
            if (get_pc() < 0x4000) last_bios_access = address & 0x3ffc;
            return *(uint32_t *) &system_rom[last_bios_access];
        case 2:
            return *(uint32_t *) &cpu_ewram[address & 0x3fffc];
        case 3:
            return *(uint32_t *) &cpu_iwram[address & 0x7ffc];
        case 4:
            return io_read_word(address & 0x3fffffc);
        case 5:
            return *(uint32_t *) &palette_ram[address & 0x3fc];
        case 6:
            address &= 0x1fffc;
            if (video_in_bitmap_mode() && address >= 0x18000) return 0;  // No VRAM OBJ mirror in bitmap mode
            if (address >= 0x18000) address -= 0x8000;
            return *(uint32_t *) &video_ram[address];
        case 7:
            return *(uint32_t *) &object_ram[address & 0x3fc];
        case 8:
        case 9:
        case 0xa:
        case 0xb:
        case 0xc:
        case 0xd:
            return rom_read_word(address & 0x1fffffc);
        case 0xe:
        case 0xf:
            return backup_read_word(address & 0xffff);
        default:
            break;
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    fmt::print("memory_read_word(0x{:08x});\n", address);
#endif
    return memory_open_bus();
}

void memory_write_word(uint32_t address, uint32_t value, bool poke) {
    UNUSED(poke);
    uint8_t region = address >> 24;
    switch (region) {
        case 0:
            if (address >= 0x4000) break;
            return;  // Read only
        case 2:
            *(uint32_t *) &cpu_ewram[address & 0x3fffc] = value;
            return;
        case 3:
            *(uint32_t *) &cpu_iwram[address & 0x7ffc] = value;
            return;
        case 4:
            io_write_word(address & 0x3fffffc, value);
            return;
        case 5:
            *(uint32_t *) &palette_ram[address & 0x3fc] = value;
            return;
        case 6:
            address &= 0x1fffc;
            if (video_in_bitmap_mode() && address >= 0x18000) return;  // No VRAM OBJ mirror in bitmap mode
            if (address >= 0x18000) address -= 0x8000;
            *(uint32_t *) &video_ram[address] = value;
            return;
        case 7:
            *(uint32_t *) &object_ram[address & 0x3fc] = value;
            return;
        case 8:
        case 9:
        case 0xa:
        case 0xb:
        case 0xc:
        case 0xd:
            return;  // Read only
        case 0xe:
        case 0xf:
            backup_write_word(address & 0xffff, value);
        default:
            break;
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    fmt::print("memory_write_word(0x{:08x}, 0x{:08x});\n", address, value);
#endif
}
