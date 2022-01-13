// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#include "memory.h"

#include <stdint.h>

#include <fmt/core.h>

#include "backup.h"
#include "cpu.h"
#include "gpio.h"
#include "io.h"
#include "main.h"

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

uint8_t memory_read_byte(uint32_t address) {
    if (address < 0x4000) {
        if (get_pc() < 0x4000) last_bios_access = address;
        return system_rom[last_bios_access];
    }
    if (address >= 0x02000000 && address < 0x03000000) {
        return cpu_ewram[address & 0x3ffff];
    }
    if (address >= 0x03000000 && address < 0x04000000) {
        return cpu_iwram[address & 0x7fff];
    }
    if (address >= 0x04000000 && address < 0x05000000) {
        return io_read_byte(address & 0x3ffffff);
    }
    if (address >= 0x05000000 && address < 0x06000000) {
        return palette_ram[address & 0x3ff];
    }
    if (address >= 0x06000000 && address < 0x07000000) {
        uint16_t mode = ioreg.dispcnt.w & 7;
        bool bitmap_mode = (mode >= 3 && mode <= 5);

        address &= 0x1ffff;
        if (bitmap_mode && address >= 0x18000) return 0;  // No VRAM OBJ mirror in bitmap mode
        if (address >= 0x18000) address -= 0x8000;
        return video_ram[address];
    }
    if (address >= 0x07000000 && address < 0x08000000) {
        return object_ram[address & 0x3ff];
    }
    if (address >= 0x08000000 && address < 0x0e000000) {
        return rom_read_byte(address & 0x1ffffff);
    }
    if (address >= 0x0e000000 && address < 0x10000000) {
        return backup_read_byte(address & 0xffff);
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    fmt::print("memory_read_byte(0x{:08x});\n", address);
#endif
    return (uint8_t) (gba_open_bus() >> 8 * (address & 3));
}

void memory_write_byte(uint32_t address, uint8_t value) {
    if (address < 0x4000) {
        return;  // Read only
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
        io_write_byte(address & 0x3ffffff, value);
        return;
    }
    if (address >= 0x05000000 && address < 0x06000000) {
        *(uint16_t *) &palette_ram[address & 0x3fe] = value | value << 8;
        return;
    }
    if (address >= 0x06000000 && address < 0x07000000) {
        uint16_t mode = ioreg.dispcnt.w & 7;
        bool bitmap_mode = (mode >= 3 && mode <= 5);

        address &= 0x1fffe;
        if (bitmap_mode && address >= 0x18000) return;             // No VRAM OBJ mirror in bitmap mode
        if (address >= (bitmap_mode ? 0x14000 : 0x10000)) return;  // VRAM OBJ 8-bit write ignored
        *(uint16_t *) &video_ram[address] = value | value << 8;
        return;
    }
    if (address >= 0x07000000 && address < 0x08000000) {
        return;  // OAM 8-bit write ignored
    }
    if (address >= 0x08000000 && address < 0x0e000000) {
        return;  // Read only
    }
    if (address >= 0x0e000000 && address < 0x10000000) {
        backup_write_byte(address & 0xffff, value);
        return;
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    fmt::print("memory_write_byte(0x{:08x}, 0x{:02x});\n", address, value);
#endif
}

uint16_t memory_read_halfword(uint32_t address) {
    if (address < 0x4000) {
        if (get_pc() < 0x4000) last_bios_access = address & 0x3ffe;
        return *(uint16_t *) &system_rom[last_bios_access];
    }
    if (address >= 0x02000000 && address < 0x03000000) {
        return *(uint16_t *) &cpu_ewram[address & 0x3fffe];
    }
    if (address >= 0x03000000 && address < 0x04000000) {
        return *(uint16_t *) &cpu_iwram[address & 0x7ffe];
    }
    if (address >= 0x04000000 && address < 0x05000000) {
        return io_read_halfword(address & 0x3fffffe);
    }
    if (address >= 0x05000000 && address < 0x06000000) {
        return *(uint16_t *) &palette_ram[address & 0x3fe];
    }
    if (address >= 0x06000000 && address < 0x07000000) {
        uint16_t mode = ioreg.dispcnt.w & 7;
        bool bitmap_mode = (mode >= 3 && mode <= 5);

        address &= 0x1fffe;
        if (bitmap_mode && address >= 0x18000) return 0;  // No VRAM OBJ mirror in bitmap mode
        if (address >= 0x18000) address -= 0x8000;
        return *(uint16_t *) &video_ram[address];
    }
    if (address >= 0x07000000 && address < 0x08000000) {
        return *(uint16_t *) &object_ram[address & 0x3fe];
    }
    if (address >= 0x08000000 && address < 0x0e000000) {
        if (has_eeprom && game_rom_size <= 0x1000000 && (address >= 0x0d000000 && address < 0x0e000000)) {
            return eeprom_read_bit();
        }
        if (has_rtc && (address >= 0x080000c4 && address < 0x080000ca)) {
            return gpio_read_halfword(address & 0x1fffffe);
        }
        return rom_read_halfword(address & 0x1fffffe);
    }
    if (address >= 0x0e000000 && address < 0x10000000) {
        return backup_read_halfword(address & 0xffff);
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    fmt::print("memory_read_halfword(0x{:08x});\n", address);
#endif
    return (uint16_t) (gba_open_bus() >> 8 * (address & 2));
}

void memory_write_halfword(uint32_t address, uint16_t value) {
    if (address < 0x4000) {
        return;  // Read only
    }
    if (address >= 0x02000000 && address < 0x03000000) {
        *(uint16_t *) &cpu_ewram[address & 0x3fffe] = value;
        return;
    }
    if (address >= 0x03000000 && address < 0x04000000) {
        *(uint16_t *) &cpu_iwram[address & 0x7ffe] = value;
        return;
    }
    if (address >= 0x04000000 && address < 0x05000000) {
        io_write_halfword(address & 0x3fffffe, value);
        return;
    }
    if (address >= 0x05000000 && address < 0x06000000) {
        *(uint16_t *) &palette_ram[address & 0x3fe] = value;
        return;
    }
    if (address >= 0x06000000 && address < 0x07000000) {
        uint16_t mode = ioreg.dispcnt.w & 7;
        bool bitmap_mode = (mode >= 3 && mode <= 5);

        address &= 0x1fffe;
        if (bitmap_mode && address >= 0x18000) return;  // No VRAM OBJ mirror in bitmap mode
        if (address >= 0x18000) address -= 0x8000;
        *(uint16_t *) &video_ram[address] = value;
        return;
    }
    if (address >= 0x07000000 && address < 0x08000000) {
        *(uint16_t *) &object_ram[address & 0x3fe] = value;
        return;
    }
    if (address >= 0x08000000 && address < 0x0e000000) {
        if (has_eeprom && game_rom_size <= 0x1000000 && (address >= 0x0d000000 && address < 0x0e000000)) {
            eeprom_write_bit(value);
            return;
        }
        if (has_rtc && (address >= 0x080000c4 && address < 0x080000ca)) {
            gpio_write_halfword(address & 0x1fffffe, value);
            return;
        }
        return;  // Read only
    }
    if (address >= 0x0e000000 && address < 0x10000000) {
        backup_write_halfword(address & 0xffff, value);
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    fmt::print("memory_write_halfword(0x{:08x}, 0x{:04x});\n", address, value);
#endif
}

uint32_t memory_read_word(uint32_t address) {
    if (address < 0x4000) {
        if (get_pc() < 0x4000) last_bios_access = address & 0x3ffc;
        return *(uint32_t *) &system_rom[last_bios_access];
    }
    if (address >= 0x02000000 && address < 0x03000000) {
        return *(uint32_t *) &cpu_ewram[address & 0x3fffc];
    }
    if (address >= 0x03000000 && address < 0x04000000) {
        return *(uint32_t *) &cpu_iwram[address & 0x7ffc];
    }
    if (address >= 0x04000000 && address < 0x05000000) {
        return io_read_word(address & 0x3fffffc);
    }
    if (address >= 0x05000000 && address < 0x06000000) {
        return *(uint32_t *) &palette_ram[address & 0x3fc];
    }
    if (address >= 0x06000000 && address < 0x07000000) {
        uint16_t mode = ioreg.dispcnt.w & 7;
        bool bitmap_mode = (mode >= 3 && mode <= 5);

        address &= 0x1fffc;
        if (bitmap_mode && address >= 0x18000) return 0;  // No VRAM OBJ mirror in bitmap mode
        if (address >= 0x18000) address -= 0x8000;
        return *(uint32_t *) &video_ram[address];
    }
    if (address >= 0x07000000 && address < 0x08000000) {
        return *(uint32_t *) &object_ram[address & 0x3fc];
    }
    if (address >= 0x08000000 && address < 0x0e000000) {
        return rom_read_word(address & 0x1fffffc);
    }
    if (address >= 0x0e000000 && address < 0x10000000) {
        return backup_read_word(address & 0xffff);
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    fmt::print("memory_read_word(0x{:08x});\n", address);
#endif
    return gba_open_bus();
}

void memory_write_word(uint32_t address, uint32_t value) {
    if (address < 0x4000) {
        return;  // Read only
    }
    if (address >= 0x02000000 && address < 0x03000000) {
        *(uint32_t *) &cpu_ewram[address & 0x3fffc] = value;
        return;
    }
    if (address >= 0x03000000 && address < 0x04000000) {
        *(uint32_t *) &cpu_iwram[address & 0x7ffc] = value;
        return;
    }
    if (address >= 0x04000000 && address < 0x05000000) {
        io_write_word(address & 0x3fffffc, value);
        return;
    }
    if (address >= 0x05000000 && address < 0x06000000) {
        *(uint32_t *) &palette_ram[address & 0x3fc] = value;
        return;
    }
    if (address >= 0x06000000 && address < 0x07000000) {
        uint16_t mode = ioreg.dispcnt.w & 7;
        bool bitmap_mode = (mode >= 3 && mode <= 5);

        address &= 0x1fffc;
        if (bitmap_mode && address >= 0x18000) return;  // No VRAM OBJ mirror in bitmap mode
        if (address >= 0x18000) address -= 0x8000;
        *(uint32_t *) &video_ram[address] = value;
        return;
    }
    if (address >= 0x07000000 && address < 0x08000000) {
        *(uint32_t *) &object_ram[address & 0x3fc] = value;
        return;
    }
    if (address >= 0x08000000 && address < 0x0e000000) {
        return;  // Read only
    }
    if (address >= 0x0e000000 && address < 0x10000000) {
        backup_write_word(address & 0xffff, value);
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    fmt::print("memory_write_word(0x{:08x}, 0x{:08x});\n", address, value);
#endif
}
