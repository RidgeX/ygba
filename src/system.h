// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <stdint.h>
#include <string>

#include <SDL.h>

#define INT_VBLANK (1 << 0)
#define INT_HBLANK (1 << 1)
#define INT_VCOUNT (1 << 2)
#define INT_TIMER0 (1 << 3)
#define INT_TIMER1 (1 << 4)
#define INT_TIMER2 (1 << 5)
#define INT_TIMER3 (1 << 6)
#define INT_COM    (1 << 7)
#define INT_DMA0   (1 << 8)
#define INT_DMA1   (1 << 9)
#define INT_DMA2   (1 << 10)
#define INT_DMA3   (1 << 11)
#define INT_BUTTON (1 << 12)
#define INT_CART   (1 << 13)

extern SDL_GameController *game_controller;

extern bool skip_bios;
extern bool single_step;
extern std::string save_path;

void system_reset(bool keep_save_data);
void system_read_bios_file();
void system_write_save_file();
void system_load_rom(const std::string &rom_path);
void system_process_input();
void system_emulate_frame();
void system_tick(uint32_t cycles);

inline void system_idle() {
    system_tick(1);
}
