// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#ifndef MEMORY_H
#define MEMORY_H

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

uint8_t rom_read_byte(uint32_t address);
uint16_t rom_read_halfword(uint32_t address);
uint32_t rom_read_word(uint32_t address);

uint8_t memory_read_byte(uint32_t address);
void memory_write_byte(uint32_t address, uint8_t value);
uint16_t memory_read_halfword(uint32_t address);
void memory_write_halfword(uint32_t address, uint16_t value);
uint32_t memory_read_word(uint32_t address);
void memory_write_word(uint32_t address, uint32_t value);

#endif  // MEMORY_H
