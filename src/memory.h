// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <stdint.h>

extern uint8_t system_rom[0x4000];
extern uint32_t last_bios_access;
extern uint8_t cpu_ewram[0x40000];
extern uint8_t cpu_iwram[0x8000];
extern uint8_t palette_ram[0x400];
extern uint8_t video_ram[0x18000];
extern uint8_t object_ram[0x400];
extern uint8_t game_rom[0x2000000];
extern uint32_t game_rom_size;
extern uint32_t game_rom_mask;

uint32_t memory_open_bus();

uint8_t rom_read_byte(uint32_t address);
uint16_t rom_read_halfword(uint32_t address);
uint32_t rom_read_word(uint32_t address);

uint8_t memory_read_byte(uint32_t address, bool peek = false);
void memory_write_byte(uint32_t address, uint8_t value, bool poke = false);
uint16_t memory_read_halfword(uint32_t address, bool peek = false);
void memory_write_halfword(uint32_t address, uint16_t value, bool poke = false);
uint32_t memory_read_word(uint32_t address, bool peek = false);
void memory_write_word(uint32_t address, uint32_t value, bool poke = false);

inline uint8_t memory_peek_byte(uint32_t address) {
    return memory_read_byte(address, true);
}

inline void memory_poke_byte(uint32_t address, uint8_t value) {
    memory_write_byte(address, value, true);
}

inline uint16_t memory_peek_halfword(uint32_t address) {
    return memory_read_halfword(address, true);
}

inline void memory_poke_halfword(uint32_t address, uint16_t value) {
    memory_write_halfword(address, value, true);
}

inline uint32_t memory_peek_word(uint32_t address) {
    return memory_read_word(address, true);
}

inline void memory_poke_word(uint32_t address, uint32_t value) {
    memory_write_word(address, value, true);
}
