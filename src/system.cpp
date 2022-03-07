// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#include "system.h"

#include <stdint.h>
#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <tuple>

#include <SDL.h>

#include "backup.h"
#include "cpu.h"
#include "dma.h"
#include "gpio.h"
#include "io.h"
#include "memory.h"
#include "timer.h"
#include "video.h"

SDL_GameController *game_controller;

bool skip_bios;
bool single_step;
std::string save_path;
uint32_t idle_loop_address;
uint16_t idle_loop_last_irq;

void system_reset(bool keep_save_data) {
    std::memset(cpu_ewram, 0, sizeof(cpu_ewram));
    std::memset(cpu_iwram, 0, sizeof(cpu_iwram));
    std::memset(&ioreg, 0, sizeof(ioreg));
    std::memset(palette_ram, 0, sizeof(palette_ram));
    std::memset(video_ram, 0, sizeof(video_ram));
    std::memset(object_ram, 0, sizeof(object_ram));
    if (!keep_save_data) backup_erase();
    backup_init();
    gpio_init();

    std::memset(r, 0, sizeof(uint32_t) * 16);
    arm_init_registers(skip_bios);
    branch_taken = true;

    video_cycles = 0;
    halted = false;
    dma_channel = -1;
    dma_pc = 0;

    ioreg.dispcnt.w = 0x80;
    ioreg.bg_affine[0].pa.w = 0x100;
    ioreg.bg_affine[0].pd.w = 0x100;
    ioreg.bg_affine[1].pa.w = 0x100;
    ioreg.bg_affine[1].pd.w = 0x100;
    last_bios_access = 0;

    if (skip_bios) {
        ioreg.rcnt.w = 0x8000;
        ioreg.postflg = 1;
        last_bios_access = 0xe4;
    }
}

void system_read_bios_file() {
    SDL_RWops *rw = SDL_RWFromFile("gba_bios.bin", "rb");
    if (rw == nullptr) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Missing BIOS file", "Failed to open BIOS file 'gba_bios.bin'.", nullptr);
        std::exit(EXIT_FAILURE);
    }

    SDL_RWread(rw, system_rom, sizeof(system_rom), 1);

    SDL_RWclose(rw);
}

static void system_read_rom_file(const std::string &rom_path) {
    std::memset(game_rom, 0, sizeof(game_rom));

    SDL_RWops *rw = SDL_RWFromFile(rom_path.c_str(), "rb");
    if (rw == nullptr) return;

    SDL_RWseek(rw, 0, RW_SEEK_END);
    game_rom_size = SDL_RWtell(rw);
    assert(game_rom_size != 0);
    game_rom_mask = std::bit_ceil(game_rom_size) - 1;
    SDL_RWseek(rw, 0, RW_SEEK_SET);
    if (game_rom_size <= sizeof(game_rom)) {
        SDL_RWread(rw, game_rom, game_rom_size, 1);
    }

    SDL_RWclose(rw);
}

static void system_read_save_file() {
    SDL_RWops *rw = SDL_RWFromFile(save_path.c_str(), "rb");
    if (rw == nullptr) return;

    if (has_eeprom) {
        SDL_RWread(rw, backup_eeprom, sizeof(backup_eeprom), 1);
    } else if (has_flash) {
        SDL_RWread(rw, backup_flash, sizeof(backup_flash), 1);
    } else if (has_sram) {
        SDL_RWread(rw, backup_sram, sizeof(backup_sram), 1);
    }

    SDL_RWclose(rw);
}

void system_write_save_file() {
    if (!has_eeprom && !has_flash && !has_sram) return;

    SDL_RWops *rw = SDL_RWFromFile(save_path.c_str(), "wb");
    if (rw == nullptr) return;

    if (has_eeprom) {
        SDL_RWwrite(rw, backup_eeprom, sizeof(backup_eeprom), 1);
    } else if (has_flash) {
        SDL_RWwrite(rw, backup_flash, sizeof(backup_flash), 1);
    } else if (has_sram) {
        SDL_RWwrite(rw, backup_sram, sizeof(backup_sram), 1);
    }

    SDL_RWclose(rw);
}

static bool rom_contains_string(const std::string &s) {
    const uint8_t *text_begin = game_rom;
    const uint8_t *text_end = text_begin + game_rom_size;
    const uint8_t *pattern_begin = (const uint8_t *) s.c_str();
    const uint8_t *pattern_end = pattern_begin + s.length();
    const auto it = std::search(text_begin, text_end, std::boyer_moore_searcher(pattern_begin, pattern_end));
    return it != text_end;
}

const std::map<std::tuple<std::string, std::string, uint8_t>, uint32_t> idle_loop_address_map{
    {{"ADVANCEWARS", "AWRE", 0}, 0x80387ec},   // Advance Wars (USA)
    {{"ADVANCEWARS", "AWRE", 1}, 0x8038818},   // Advance Wars (USA) (Rev 1)
    {{"ADVANCEWARS2", "AW2E", 0}, 0x8036e0c},  // Advance Wars 2 - Black Hole Rising (USA, Australia)
    {{"DRILL DOZER", "V49E", 0}, 0x80006ba},   // Drill Dozer (USA)
    {{"FFTA_USVER.", "AFXE", 0}, 0x8000418},   // Final Fantasy Tactics Advance (USA, Australia)
    {{"KURURIN", "AKRP", 0}, 0x800041a},       // Kurukuru Kururin (Europe)
    {{"POKEMON EMER", "BPEE", 0}, 0x80008c6},  // Pokemon - Emerald Version (USA, Europe)
    {{"POKEMON FIRE", "BPRE", 0}, 0x80008aa},  // Pokemon - FireRed Version (USA)
    {{"POKEMON FIRE", "BPRE", 1}, 0x80008be},  // Pokemon - FireRed Version (USA, Europe) (Rev 1)
    {{"POKEMON LEAF", "BPGE", 0}, 0x80008aa},  // Pokemon - LeafGreen Version (USA)
    {{"POKEMON LEAF", "BPGE", 1}, 0x80008be},  // Pokemon - LeafGreen Version (USA, Europe) (Rev 1)
};

static void system_detect_cartridge_features() {
    has_eeprom = false;
    has_flash = false;
    has_sram = false;
    has_rtc = false;
    idle_loop_address = 0;

    if (rom_contains_string("EEPROM_V")) {
        has_eeprom = true;
    }
    if (rom_contains_string("FLASH_V") || rom_contains_string("FLASH512_V")) {
        has_flash = true;
        flash_manufacturer = MANUFACTURER_PANASONIC;
        flash_device = DEVICE_MN63F805MNP;
    }
    if (rom_contains_string("FLASH1M_V")) {
        has_flash = true;
        flash_manufacturer = MANUFACTURER_SANYO;
        flash_device = DEVICE_LE26FV10N1TS;
    }
    if (rom_contains_string("SRAM_V") || rom_contains_string("SRAM_F_V")) {
        has_sram = true;
    }
    if (rom_contains_string("SIIRTC_V")) {
        has_rtc = true;
    }

    std::string game_title((char *) &game_rom[0xa0], 12);
    if (const auto it = std::find(game_title.begin(), game_title.end(), '\0'); it != game_title.end()) {
        game_title.erase(it, game_title.end());
    }
    std::string game_code((char *) &game_rom[0xac], 4);
    if (const auto it = std::find(game_code.begin(), game_code.end(), '\0'); it != game_code.end()) {
        game_code.erase(it, game_code.end());
    }
    uint8_t game_version = game_rom[0xbc];

    auto key = std::make_tuple(game_title, game_code, game_version);

    if (idle_loop_address_map.contains(key)) {
        idle_loop_address = idle_loop_address_map.at(key);
    }
    if (key == std::make_tuple("MONKEYBALLJR", "ALUE", 0)) {  // Super Monkey Ball Jr. (USA)
        has_eeprom = true;
        has_flash = has_sram = false;
    }
}

void system_load_rom(const std::string &rom_path) {
    const std::string rom_ext{".gba"};
    if (!rom_path.ends_with(rom_ext)) return;

    if (!save_path.empty()) {
        system_write_save_file();
    }
    save_path = rom_path;
    std::string::size_type n = save_path.rfind(rom_ext);
    save_path.replace(n, rom_ext.length(), ".sav");

    system_reset(false);
    system_read_rom_file(rom_path);
    system_detect_cartridge_features();
    system_read_save_file();
}

void system_process_input() {
    const Uint8 *key_state = SDL_GetKeyboardState(nullptr);
    static bool keys[10];
    std::memset(keys, 0, sizeof(keys));
    keys[0] |= (bool) key_state[SDL_SCANCODE_X];          // Button A
    keys[1] |= (bool) key_state[SDL_SCANCODE_Z];          // Button B
    keys[2] |= (bool) key_state[SDL_SCANCODE_BACKSPACE];  // Select
    keys[3] |= (bool) key_state[SDL_SCANCODE_RETURN];     // Start
    keys[4] |= (bool) key_state[SDL_SCANCODE_RIGHT];      // Right
    keys[5] |= (bool) key_state[SDL_SCANCODE_LEFT];       // Left
    keys[6] |= (bool) key_state[SDL_SCANCODE_UP];         // Up
    keys[7] |= (bool) key_state[SDL_SCANCODE_DOWN];       // Down
    keys[8] |= (bool) key_state[SDL_SCANCODE_S];          // Button R
    keys[9] |= (bool) key_state[SDL_SCANCODE_A];          // Button L
    if (game_controller != nullptr) {
        keys[0] |= (bool) SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_A);              // Button A
        keys[1] |= (bool) SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_B);              // Button B
        keys[2] |= (bool) SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_BACK);           // Select
        keys[3] |= (bool) SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_START);          // Start
        keys[4] |= (bool) SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);     // Right
        keys[5] |= (bool) SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT);      // Left
        keys[6] |= (bool) SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_DPAD_UP);        // Up
        keys[7] |= (bool) SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN);      // Down
        keys[8] |= (bool) SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);  // Button R
        keys[9] |= (bool) SDL_GameControllerGetButton(game_controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);   // Button L
    }
    if (keys[4] && keys[5]) keys[4] = keys[5] = false;  // Disallow opposing directions
    if (keys[6] && keys[7]) keys[6] = keys[7] = false;
    ioreg.keyinput.w = 0x3ff;
    for (int i = 0; i < 10; i++) {
        if (keys[i]) {
            ioreg.keyinput.w &= ~(1 << i);
        }
    }
}

void system_emulate_frame() {
    video_frame_drawn = false;

    while (true) {
        if (FLAG_T()) {
            if (branch_taken) thumb_fill_pipeline();
            if (!halted) thumb_step();
        } else {
            if (branch_taken) arm_fill_pipeline();
            if (!halted) arm_step();
        }

        // Idle loop optimization
        if (idle_loop_address != 0 && idle_loop_address == get_pc()) {
            bool vblank_raised = (idle_loop_last_irq & INT_VBLANK);
            bool vblank_handled = !(ioreg.irq.w & INT_VBLANK);
            if (!(vblank_raised && vblank_handled)) {
                halted = true;
            }
            idle_loop_last_irq = ioreg.irq.w;
        }

        if (ioreg.irq.w & ioreg.ie.w) {
            halted = false;
            if (!branch_taken && !(cpsr & PSR_I) && ioreg.ime.w) {
                arm_hardware_interrupt();
            }
        }

        system_tick(1);  // FIXME Implement timings
        if (video_frame_drawn || (single_step && !halted)) break;
    }
}

void system_tick(uint32_t cycles) {
    timer_update(cycles);
    video_update(cycles);
}
