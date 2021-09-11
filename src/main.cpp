// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include "imgui_memory_editor.h"

#include <SDL.h>
#include <SDL_opengl.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "algorithms.h"
#include "backup.h"
#include "cpu.h"
#include "gpio.h"
#include "io.h"
#include "main.h"
#include "memory.h"

bool single_step = false;
uint32_t ppu_cycles = 0;
bool halted = false;
int active_dma = -1;
bool skip_bios = false;
char *save_path = NULL;
char game_title[13];
char game_code[5];
uint8_t game_version = 0;
uint32_t idle_loop_address = 0;
uint16_t idle_loop_last_irq = 0;

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 160

uint32_t screen_texture;
uint32_t screen_pixels[SCREEN_HEIGHT][SCREEN_WIDTH];
int screen_scale = 3;

typedef struct {
    int left;
    int top;
    int right;
    int bottom;
} lcd_window;

lcd_window win0, win1;

bool is_point_in_window(int x, int y, lcd_window win) {
    bool x_ok = false;
    bool y_ok = false;

    if (win.left < win.right) {
        x_ok = (x >= win.left && x < win.right);
    } else if (win.left > win.right) {
        x_ok = (x >= win.left || x < win.right);
    }
    if (win.top < win.bottom) {
        y_ok = (y >= win.top && y < win.bottom);
    } else if (win.top > win.bottom) {
        y_ok = (y >= win.top || y < win.bottom);
    }

    return (x_ok && y_ok);
}

double fixed24p8_to_double(uint32_t x) {
    double result = (x >> 8) + ((x & 0xff) / 256.0);
    if (result > 524288.0) result -= 1048576.0;
    return result;
}

double cubic_interpolate(int8_t *history, double mu) {
    double A = history[3] - history[2] - history[0] + history[1];
    double B = history[0] - history[1] - A;
    double C = history[2] - history[0];
    double D = history[1];
    return A * mu * mu * mu + B * mu * mu + C * mu + D;
}

int16_t clamp_i16(int16_t x, int16_t min, int16_t max) {
    x = x < min ? min : x;
    x = x > max ? max : x;
    return x;
}

void gba_audio_callback(void *userdata, uint8_t *stream_u8, int len_u8) {
    UNUSED(userdata);
    int16_t *stream = (int16_t *) stream_u8;
    int len = len_u8 / 2;

    uint16_t a_timer = BIT(ioreg.io_soundcnt_h, 10);
    uint16_t b_timer = BIT(ioreg.io_soundcnt_h, 14);
    uint16_t a_control = ioreg.timer[a_timer].control.w;
    uint16_t b_control = ioreg.timer[b_timer].control.w;
    uint16_t a_reload = ioreg.timer[a_timer].reload.w;
    uint16_t b_reload = ioreg.timer[b_timer].reload.w;
    double a_source_rate = 16777216.0 / (65536 - a_reload);
    double b_source_rate = 16777216.0 / (65536 - b_reload);
    double target_rate = 48000.0;
    double a_ratio = a_source_rate / target_rate;
    double b_ratio = b_source_rate / target_rate;
    static double a_fraction = 0;
    static double b_fraction = 0;
    static int8_t a_history[4];
    static int8_t b_history[4];

    for (int i = 0; i < len; i += 2) {
        a_history[0] = a_history[1];
        a_history[1] = a_history[2];
        a_history[2] = a_history[3];
        a_history[3] = (BIT(a_control, 7) ? (int8_t) ioreg.fifo_a[ioreg.fifo_a_r] : 0);
        double a = cubic_interpolate(a_history, a_fraction);
        a_fraction += a_ratio;
        if (a_fraction >= 1.0) {
            a_fraction -= (int) a_fraction;  // % 1.0
            if ((ioreg.fifo_a_r + 1) % FIFO_SIZE != ioreg.fifo_a_w) {
                ioreg.fifo_a_r = (ioreg.fifo_a_r + 1) % FIFO_SIZE;
            }
        }

        b_history[0] = b_history[1];
        b_history[1] = b_history[2];
        b_history[2] = b_history[3];
        b_history[3] = (BIT(b_control, 7) ? (int8_t) ioreg.fifo_b[ioreg.fifo_b_r] : 0);
        double b = cubic_interpolate(b_history, a_fraction);
        b_fraction += b_ratio;
        if (b_fraction >= 1.0) {
            b_fraction -= (int) b_fraction;  // % 1.0
            if ((ioreg.fifo_b_r + 1) % FIFO_SIZE != ioreg.fifo_b_w) {
                ioreg.fifo_b_r = (ioreg.fifo_b_r + 1) % FIFO_SIZE;
            }
        }

        int16_t left = 0;
        int16_t right = 0;
        if (BIT(ioreg.io_soundcnt_h, 8)) right = clamp_i16(right + a, -512, 511);
        if (BIT(ioreg.io_soundcnt_h, 9)) left = clamp_i16(left + a, -512, 511);
        if (BIT(ioreg.io_soundcnt_h, 12)) right = clamp_i16(right + b, -512, 511);
        if (BIT(ioreg.io_soundcnt_h, 13)) left = clamp_i16(left + b, -512, 511);
        stream[i] = left << 7;
        stream[i + 1] = right << 7;
    }
}

void gba_audio_fifo_a(uint32_t sample) {
    *(uint32_t *)&ioreg.fifo_a[ioreg.fifo_a_w] = sample;
    ioreg.fifo_a_w = (ioreg.fifo_a_w + 4) % FIFO_SIZE;
}

void gba_audio_fifo_b(uint32_t sample) {
    *(uint32_t *)&ioreg.fifo_b[ioreg.fifo_b_w] = sample;
    ioreg.fifo_b_w = (ioreg.fifo_b_w + 4) % FIFO_SIZE;
}

SDL_AudioDeviceID gba_audio_init(void) {
    SDL_AudioSpec want;
    memset(&want, 0, sizeof(want));
    want.freq = 48000;
    want.format = AUDIO_S16;
    want.channels = 2;
    want.samples = FIFO_SIZE;
    want.callback = gba_audio_callback;
    SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (audio_device == 0) {
        SDL_Log("Failed to open audio device: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }
    SDL_PauseAudioDevice(audio_device, 0);
    return audio_device;
}

void gba_check_keypad_interrupt(void) {
    if (BIT(ioreg.keycnt.w, 14)) {
        uint16_t held = ~ioreg.keyinput.w & 0x3ff;
        uint16_t mask = ioreg.keycnt.w & 0x3ff;
        if (BIT(ioreg.keycnt.w, 15)) {
            if ((held & mask) == mask) {  // All keys held
                ioreg.irq.w |= INT_BUTTON;
            }
        } else {
            if (held & mask) {  // Any key held
                ioreg.irq.w |= INT_BUTTON;
            }
        }
    }
}

void gba_timer_reset(int i) {
    ioreg.timer[i].counter.w = ioreg.timer[i].reload.w;
    ioreg.timer[i].elapsed = 0;
}

uint32_t open_bus(void) {
    if (FLAG_T()) {
        return thumb_pipeline[1] | thumb_pipeline[1] << 16;
    } else {
        return arm_pipeline[1];
    }
}

void gba_detect_cartridge_features(void) {
    void *match;

    has_eeprom = false;
    has_flash = false;
    has_sram = false;
    has_rtc = false;
    idle_loop_address = 0;

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

    uint8_t *siirtc_v = (uint8_t *) "SIIRTC_V";
    match = boyer_moore_matcher(game_rom, game_rom_size, siirtc_v, 8);
    if (match) { has_rtc = true; }

    memcpy(game_title, game_rom + 0xa0, 12);
    game_title[12] = '\0';
    memcpy(game_code, game_rom + 0xac, 4);
    game_code[4] = '\0';
    game_version = game_rom[0xbc];

    // Pokemon - Emerald Version (USA, Europe)
    if (strcmp(game_title, "POKEMON EMER") == 0 && strcmp(game_code, "BPEE") == 0 && game_version == 0) idle_loop_address = 0x80008c6;
    // Pokemon - FireRed Version (USA)
    if (strcmp(game_title, "POKEMON FIRE") == 0 && strcmp(game_code, "BPRE") == 0 && game_version == 0) idle_loop_address = 0x80008aa;
    // Pokemon - FireRed Version (USA, Europe) (Rev 1)
    if (strcmp(game_title, "POKEMON FIRE") == 0 && strcmp(game_code, "BPRE") == 0 && game_version == 1) idle_loop_address = 0x80008be;
    // Pokemon - LeafGreen Version (USA)
    if (strcmp(game_title, "POKEMON LEAF") == 0 && strcmp(game_code, "BPGE") == 0 && game_version == 0) idle_loop_address = 0x80008aa;
    // Pokemon - LeafGreen Version (USA, Europe) (Rev 1)
    if (strcmp(game_title, "POKEMON LEAF") == 0 && strcmp(game_code, "BPGE") == 0 && game_version == 1) idle_loop_address = 0x80008be;
    // Super Monkey Ball Jr. (USA)
    if (strcmp(game_title, "MONKEYBALLJR") == 0 && strcmp(game_code, "ALUE") == 0 && game_version == 0) has_flash = has_sram = false;
}

void gba_reset(bool keep_backup) {
    memset(cpu_ewram, 0, sizeof(cpu_ewram));
    memset(cpu_iwram, 0, sizeof(cpu_iwram));
    memset(&ioreg, 0, sizeof(ioreg));
    memset(palette_ram, 0, sizeof(palette_ram));
    memset(video_ram, 0, sizeof(video_ram));
    memset(object_ram, 0, sizeof(object_ram));
    if (!keep_backup) backup_erase();
    backup_init();
    gpio_init();

    memset(r, 0, sizeof(uint32_t) * 16);
    arm_init_registers(skip_bios);
    branch_taken = true;

    ppu_cycles = 0;
    halted = false;

    ioreg.dispcnt.w = 0x80;
    ioreg.bg_affine[0].dx.w = 0x100;
    ioreg.bg_affine[0].dmy.w = 0x100;
    ioreg.bg_affine[1].dx.w = 0x100;
    ioreg.bg_affine[1].dmy.w = 0x100;
    last_bios_access = 0;

    if (skip_bios) {
        ioreg.rcnt.w = 0x8000;
        ioreg.postflg = 1;
        last_bios_access = 0xe4;
    }
}

void read_save_file(void) {
    SDL_RWops *rw = SDL_RWFromFile(save_path, "rb");
    if (rw == NULL) return;

    if (has_eeprom) {
        SDL_RWread(rw, backup_eeprom, sizeof(backup_eeprom), 1);
    } else if (has_flash) {
        SDL_RWread(rw, backup_flash, sizeof(backup_flash), 1);
    } else if (has_sram) {
        SDL_RWread(rw, backup_sram, sizeof(backup_sram), 1);
    }

    SDL_RWclose(rw);
}

void write_save_file(void) {
    if (!has_eeprom && !has_flash && !has_sram) return;

    SDL_RWops *rw = SDL_RWFromFile(save_path, "wb");
    if (rw == NULL) return;

    if (has_eeprom) {
        SDL_RWwrite(rw, backup_eeprom, sizeof(backup_eeprom), 1);
    } else if (has_flash) {
        SDL_RWwrite(rw, backup_flash, sizeof(backup_flash), 1);
    } else if (has_sram) {
        SDL_RWwrite(rw, backup_sram, sizeof(backup_sram), 1);
    }

    SDL_RWclose(rw);
}

void load_bios_file(void) {
    SDL_RWops *rw = SDL_RWFromFile("gba_bios.bin", "rb");
    if (rw == NULL) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Missing BIOS file", "Failed to open BIOS file 'gba_bios.bin'.", NULL);
        exit(EXIT_FAILURE);
    }

    SDL_RWread(rw, system_rom, sizeof(system_rom), 1);

    SDL_RWclose(rw);
}

void load_rom_file(const char *rom_path) {
    memset(game_rom, 0, sizeof(game_rom));

    SDL_RWops *rw = SDL_RWFromFile(rom_path, "rb");
    if (rw == NULL) return;

    SDL_RWseek(rw, 0, RW_SEEK_END);
    game_rom_size = SDL_RWtell(rw);
    assert(game_rom_size != 0);
    game_rom_mask = next_power_of_2(game_rom_size) - 1;
    SDL_RWseek(rw, 0, RW_SEEK_SET);
    if (game_rom_size <= sizeof(game_rom)) {
        SDL_RWread(rw, game_rom, game_rom_size, 1);
    }

    SDL_RWclose(rw);
}

void gba_load(const char *rom_path) {
    if (strstr(rom_path, ".gba") == NULL) return;

    if (save_path != NULL) {
        write_save_file();
        free(save_path);
    }
    save_path = strdup(rom_path);
    char *p_ext = strstr(save_path, ".gba");
    *p_ext = '\0';
    strcat(save_path, ".sav");

    gba_reset(false);
    load_rom_file(rom_path);
    gba_detect_cartridge_features();
    read_save_file();
}

uint32_t rgb555(uint32_t pixel) {
    uint32_t red, green, blue;
    red = pixel & 0x1f;
    green = (pixel >> 5) & 0x1f;
    blue = (pixel >> 10) & 0x1f;
    red = (red << 3) | (red >> 2);
    green = (green << 3) | (green >> 2);
    blue = (blue << 3) | (blue >> 2);
    return 0xff << 24 | blue << 16 | green << 8 | red;
}

void gba_draw_blank(int y, bool forced_blank) {
    uint16_t pixel = *(uint16_t *)&palette_ram[0];
    uint32_t clear_color = rgb555(forced_blank ? 0x7fff : pixel);

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        screen_pixels[y][x] = clear_color;
    }
}

void gba_draw_pixel_culled(int bg, int x, int y, uint32_t pixel) {
    if (x < 0 || x >= SCREEN_WIDTH) return;

    bool enable_win0 = (ioreg.dispcnt.w & DCNT_WIN0);
    bool enable_win1 = (ioreg.dispcnt.w & DCNT_WIN1);
    bool enable_winobj = (ioreg.dispcnt.w & DCNT_WINOBJ);
    bool enable_winout = (enable_win0 || enable_win1 || enable_winobj);

    bool inside_win0 = (enable_win0 && is_point_in_window(x, y, win0));
    bool inside_win1 = (enable_win1 && is_point_in_window(x, y, win1));
    bool inside_winobj = false;  // FIXME

    if (inside_win0) {
        if (!BIT(ioreg.winin.w, bg)) return;
    } else if (inside_win1) {
        if (!BIT(ioreg.winin.w, 8 + bg)) return;
    } else if (inside_winobj) {
        if (!BIT(ioreg.winout.w, 8 + bg)) return;
    } else if (enable_winout) {
        if (!BIT(ioreg.winout.w, bg)) return;
    }

    screen_pixels[y][x] = rgb555(pixel);
}

bool tile_access(uint32_t tile_address, int x, int y, bool hflip, bool vflip, bool colors_256, uint32_t palette_offset, int palette_no, uint16_t *pixel) {
    assert(x >= 0 && x < 8);
    assert(y >= 0 && y < 8);

    if (hflip) x = 7 - x;
    if (vflip) y = 7 - y;

    uint8_t *tile = &video_ram[tile_address];

    if (colors_256) {
        uint32_t tile_offset = y * 8 + x;
        uint8_t pixel_index = tile[tile_offset];
        if (pixel_index != 0) {
            *pixel = *(uint16_t *)&palette_ram[palette_offset + pixel_index * 2];
            return true;
        }
    } else {
        uint32_t tile_offset = y * 4 + x / 2;
        uint8_t pixel_indexes = tile[tile_offset];
        uint8_t pixel_index = (pixel_indexes >> (x % 2 == 1 ? 4 : 0)) & 0xf;
        if (pixel_index != 0) {
            *pixel = *(uint16_t *)&palette_ram[palette_offset + palette_no * 32 + pixel_index * 2];
            return true;
        }
    }

    return false;
}

bool obj_access(int tile_no, int x, int y, int w, int h, bool hflip, bool vflip, bool colors_256, int palette_no, int mode, uint16_t *pixel) {
    assert(x >= 0 && x < w);
    assert(y >= 0 && y < h);

    if (hflip) x = w - 1 - x;
    if (vflip) y = h - 1 - y;

    bool obj_1d = (ioreg.dispcnt.w & DCNT_OBJ_1D);
    int stride = (obj_1d ? (w / 8) : (colors_256 ? 16 : 32));
    int increment = (colors_256 ? 2 : 1);
    tile_no += (y / 8) * stride * increment + (x / 8) * increment;
    tile_no &= 0x3ff;

    bool bitmap_mode = (mode >= 3 && mode <= 5);
    if (bitmap_mode && tile_no < 512) return false;

    return tile_access(0x10000 + tile_no * 32, x % 8, y % 8, false, false, colors_256, 0x200, palette_no, pixel);
}

void gba_draw_obj(int mode, int pri, int y) {
    for (int n = 127; n >= 0; n--) {
        uint16_t attr0 = *(uint16_t *)&object_ram[n * 8];
        uint16_t attr1 = *(uint16_t *)&object_ram[n * 8 + 2];
        uint16_t attr2 = *(uint16_t *)&object_ram[n * 8 + 4];

        int oy = BITS(attr0, 0, 7);
        int obj_mode = BITS(attr0, 8, 9);
        //int gfx_mode = BITS(attr0, 10, 11);
        //bool mosaic = BIT(attr0, 12);
        bool colors_256 = BIT(attr0, 13);
        int shape = BITS(attr0, 14, 15);

        int ox = BITS(attr1, 0, 8);
        //int aff_index = BITS(attr1, 9, 13);
        bool hflip = BIT(attr1, 12);
        bool vflip = BIT(attr1, 13);
        int size = BITS(attr1, 14, 15);

        int tile_no = BITS(attr2, 0, 9);
        int priority = BITS(attr2, 10, 11);
        int palette_no = BITS(attr2, 12, 15);

        if (obj_mode == 2 || priority != pri) continue;

        int ow = 8;
        int oh = 8;
        if (shape == 0) {
            if (size == 0) { ow = 8; oh = 8; }
            else if (size == 1) { ow = 16; oh = 16; }
            else if (size == 2) { ow = 32; oh = 32; }
            else if (size == 3) { ow = 64; oh = 64; }
        } else if (shape == 1) {
            if (size == 0) { ow = 16; oh = 8; }
            else if (size == 1) { ow = 32; oh = 8; }
            else if (size == 2) { ow = 32; oh = 16; }
            else if (size == 3) { ow = 64; oh = 32; }
        } else if (shape == 2) {
            if (size == 0) { ow = 8; oh = 16; }
            else if (size == 1) { ow = 8; oh = 32; }
            else if (size == 2) { ow = 16; oh = 32; }
            else if (size == 3) { ow = 32; oh = 64; }
        } else if (shape == 3) {
            // Fall through
        }

        if (ox + ow > 511) ox -= 512;
        if (oy + oh > 255) oy -= 256;

        if (obj_mode == 1 || obj_mode == 3) {
            hflip = false;
            vflip = false;
        }
        if (obj_mode == 3) {  // FIXME
            ox += ow / 2;
            oy += oh / 2;
        }

        if (y < oy || y >= oy + oh) continue;

        for (int i = 0; i < ow; i++) {
            uint16_t pixel;
            bool ok = obj_access(tile_no, i, y - oy, ow, oh, hflip, vflip, colors_256, palette_no, mode, &pixel);
            if (ok) gba_draw_pixel_culled(4, ox + i, y, pixel);
        }
    }
}

void gba_draw_bitmap(int mode, int y) {
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
            bool pflip = (ioreg.dispcnt.w & DCNT_PAGE);
            uint8_t pixel_index = video_ram[(pflip ? 0xa000 : 0) + y * SCREEN_WIDTH + x];
            pixel = *(uint16_t *)&palette_ram[pixel_index * 2];
        }
        screen_pixels[y][x] = rgb555(pixel);
    }

    for (int pri = 3; pri >= 0; pri--) {
        gba_draw_obj(mode, pri, y);
    }
}

void gba_draw_tiled_bg(int mode, int bg, int y) {
    if (mode == 1 && bg == 3) return;
    if (mode == 2 && (bg == 0 || bg == 1)) return;

    uint32_t bgcnt = ioreg.bgcnt[bg].w;
    int hofs = ioreg.bg_text[bg].x.w;
    int vofs = ioreg.bg_text[bg].y.w;

    uint32_t tile_base = BITS(bgcnt, 2, 3) * 0x4000;
    uint32_t map_base = BITS(bgcnt, 8, 12) * 0x800;
    bool overflow_wraps = BIT(bgcnt, 13);
    uint32_t screen_size = BITS(bgcnt, 14, 15);
    bool colors_256 = BIT(bgcnt, 7);

    bool is_affine = ((mode == 1 && bg == 2) || (mode == 2 && (bg == 2 || bg == 3)));
    if (is_affine) colors_256 = true;

    int width_in_tiles = 32;
    int height_in_tiles = 32;
    if (is_affine) {
        switch (screen_size) {
            case 0: width_in_tiles = 16; height_in_tiles = 16; break;
            case 1: width_in_tiles = 32; height_in_tiles = 32; break;
            case 2: width_in_tiles = 64; height_in_tiles = 64; break;
            case 3: width_in_tiles = 128; height_in_tiles = 128; break;
        }
    } else {
        switch (screen_size) {
            case 0: width_in_tiles = 32; height_in_tiles = 32; break;
            case 1: width_in_tiles = 64; height_in_tiles = 32; break;
            case 2: width_in_tiles = 32; height_in_tiles = 64; break;
            case 3: width_in_tiles = 64; height_in_tiles = 64; break;
        }
    }

    for (int x = 0; x < 31 * 8; x += 8) {
        int tile_no;
        bool hflip, vflip;
        int palette_no;

        if (is_affine) {
            int aff_x = (int) fixed24p8_to_double(ioreg.bg_affine[bg - 2].x.dw);
            int aff_y = (int) fixed24p8_to_double(ioreg.bg_affine[bg - 2].y.dw);
            hofs = aff_x;
            vofs = aff_y;
            if (hofs < 0) hofs += width_in_tiles * 8;
            if (vofs < 0) vofs += height_in_tiles * 8;
            int map_x = ((x + hofs) / 8) % width_in_tiles;
            int map_y = ((y + vofs) / 8) % height_in_tiles;
            bool in_range = ((x + aff_x) >= 0 && (y + aff_y) >= 0 && (x + aff_x) < (width_in_tiles * 8) && (y + aff_y) < (height_in_tiles * 8));
            if (overflow_wraps || in_range) {
                uint32_t map_index = map_y * width_in_tiles + map_x;
                uint8_t info = video_ram[map_base + map_index];
                tile_no = info;
            } else {
                tile_no = -1;
            }
            hflip = false;
            vflip = false;
            palette_no = 0;
        } else {
            int map_x = ((x + hofs) / 8) % width_in_tiles;
            int map_y = ((y + vofs) / 8) % height_in_tiles;
            int quad_x = 32 * 32;
            int quad_y = 32 * 32 * (screen_size == 3 ? 2 : 1);
            uint32_t map_index = (map_y / 32) * quad_y + (map_x / 32) * quad_x + (map_y % 32) * 32 + (map_x % 32);
            uint16_t info = *(uint16_t *)&video_ram[map_base + map_index * 2];
            tile_no = BITS(info, 0, 9);
            hflip = BIT(info, 10);
            vflip = BIT(info, 11);
            palette_no = BITS(info, 12, 15);
        }

        if (tile_no == -1) continue;
        uint32_t tile_address = tile_base + tile_no * (colors_256 ? 64 : 32);
        if (tile_address >= 0x10000) continue;

        uint32_t xh = x - (hofs % 8);
        uint32_t yv = y + (vofs % 8);
        int j = yv % 8;
        for (int i = 0; i < 8; i++) {
            uint16_t pixel;
            bool ok = tile_access(tile_address, i, j, hflip, vflip, colors_256, 0, palette_no, &pixel);
            if (ok) gba_draw_pixel_culled(bg, xh + i, y, pixel);
        }
    }
}

void gba_draw_tiled(int mode, int y) {
    for (int pri = 3; pri >= 0; pri--) {
        for (int bg = 3; bg >= 0; bg--) {
            bool bg_visible = BIT(ioreg.dispcnt.w, 8 + bg);
            if (!bg_visible) continue;
            uint16_t priority = BITS(ioreg.bgcnt[bg].w, 0, 1);
            if (priority != pri) continue;
            gba_draw_tiled_bg(mode, bg, y);
        }
        bool obj_visible = (ioreg.dispcnt.w & DCNT_OBJ);
        if (!obj_visible) continue;
        gba_draw_obj(mode, pri, y);
    }
}

void gba_draw_scanline(void) {
    win0.right = ioreg.winh[0].b.b0;
    win0.left = ioreg.winh[0].b.b1;
    win0.bottom = ioreg.winv[0].b.b0;
    win0.top = ioreg.winv[0].b.b1;
    win1.right = ioreg.winh[1].b.b0;
    win1.left = ioreg.winh[1].b.b1;
    win1.bottom = ioreg.winv[1].b.b0;
    win1.top = ioreg.winv[1].b.b1;

    bool forced_blank = (ioreg.dispcnt.w & DCNT_BLANK);
    gba_draw_blank(ioreg.vcount.w, forced_blank);
    if (forced_blank) return;

    int mode = BITS(ioreg.dispcnt.w, 0, 2);
    switch (mode) {
        case 0:
        case 1:
        case 2:
        //case 6:
        //case 7:
            gba_draw_tiled(mode, ioreg.vcount.w);
            break;

        case 3:
        case 4:
        case 5:
            gba_draw_bitmap(mode, ioreg.vcount.w);
            break;

        default:
            assert(false);
            break;
    }
}

void gba_ppu_update(void) {
    if (ppu_cycles % 1232 == 0) {
        ioreg.dispstat.w &= ~DSTAT_IN_HBL;
        if (ioreg.vcount.w < SCREEN_HEIGHT) {
            gba_draw_scanline();
        }
        ioreg.vcount.w = (ioreg.vcount.w + 1) % 228;
        if (ioreg.vcount.w == 227) {
            ioreg.dispstat.w &= ~DSTAT_IN_VBL;
        } else if (ioreg.vcount.w == 161) {  // FIXME
            if (ioreg.dispstat.w & DSTAT_VBL_IRQ) {
                ioreg.irq.w |= INT_VBLANK;
            }
        } else if (ioreg.vcount.w == 160) {
            if (!(ioreg.dispstat.w & DSTAT_IN_VBL)) {
                ioreg.dispstat.w |= DSTAT_IN_VBL;
                gba_dma_update(DMA_AT_VBLANK);
            }
        }
        if (ioreg.vcount.w == ioreg.dispstat.b.b1) {
            if (!(ioreg.dispstat.w & DSTAT_IN_VCT)) {
                ioreg.dispstat.w |= DSTAT_IN_VCT;
                if (ioreg.dispstat.w & DSTAT_VCT_IRQ) {
                    ioreg.irq.w |= INT_VCOUNT;
                }
            }
        } else {
            ioreg.dispstat.w &= ~DSTAT_IN_VCT;
        }
    }
    ppu_cycles = (ppu_cycles + 1) % 280896;
    if (ppu_cycles % 1232 == 1006) {
        if (!(ioreg.dispstat.w & DSTAT_IN_HBL)) {
            ioreg.dispstat.w |= DSTAT_IN_HBL;
            if (ioreg.dispstat.w & DSTAT_HBL_IRQ) {
                ioreg.irq.w |= INT_HBLANK;
            }
            if (ppu_cycles < 197120) {
                gba_dma_update(DMA_AT_HBLANK);
            }
        }
    }
}

void gba_timer_update(uint32_t cycles) {
    bool overflow = false;

    for (int i = 0; i < 4; i++) {
        uint16_t *counter = &ioreg.timer[i].counter.w;
        uint16_t *reload = &ioreg.timer[i].reload.w;
        uint16_t *control = &ioreg.timer[i].control.w;
        uint32_t *elapsed = &ioreg.timer[i].elapsed;

        if (!(*control & TM_ENABLE)) {
            overflow = false;
            continue;
        }

        uint32_t increment = 0;
        if (*control & TM_CASCADE) {
            increment = (overflow ? 1 : 0);
        } else {
            *elapsed += cycles;
            uint32_t freq = 1;
            switch (*control & TM_FREQ_MASK) {
                case TM_FREQ_1: freq = 1; break;
                case TM_FREQ_64: freq = 64; break;
                case TM_FREQ_256: freq = 256; break;
                case TM_FREQ_1024: freq = 1024; break;
            }
            if (*elapsed >= freq) {
                increment = *elapsed / freq;
                *elapsed = *elapsed % freq;
            }
        }

        uint16_t last_counter = *counter;
        *counter += increment;
        overflow = *counter < last_counter;
        if (overflow) {
            *counter += *reload;
            bool fifo_a_tick = BIT(ioreg.io_soundcnt_h, 10) == i;
            bool fifo_b_tick = BIT(ioreg.io_soundcnt_h, 14) == i;
            if (fifo_a_tick) {
                ioreg.fifo_a_ticks = (ioreg.fifo_a_ticks + 1) % 16;
                if (ioreg.fifo_a_ticks == 0) ioreg.fifo_a_refill = true;
            }
            if (fifo_b_tick) {
                ioreg.fifo_b_ticks = (ioreg.fifo_b_ticks + 1) % 16;
                if (ioreg.fifo_b_ticks == 0) ioreg.fifo_b_refill = true;
            }
            if (ioreg.fifo_a_refill || ioreg.fifo_b_refill) {
                gba_dma_update(DMA_AT_REFRESH);
                ioreg.fifo_a_refill = false;
                ioreg.fifo_b_refill = false;
            }
            if (*control & TM_IRQ) {
                ioreg.irq.w |= 1 << (3 + i);
            }
        }
    }
}

void gba_dma_transfer(int ch, uint32_t dst_ctrl, uint32_t src_ctrl, uint32_t *dst_addr, uint32_t *src_addr, uint32_t size, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        bool bad_src_addr = !(*src_addr >= 0x02000000 && *src_addr < 0x10000000);
        bad_src_addr |= (ch == 0 && *src_addr >= 0x08000000 && *src_addr < 0x0e000000);

        bool bad_dst_addr = !(*dst_addr >= 0x02000000 && *dst_addr < 0x10000000);
        bad_dst_addr |= (ch != 3 && *dst_addr >= 0x08000000);

        if (size == 4) {
            uint32_t value = ioreg.dma_value.dw;
            if (!bad_src_addr) value = memory_read_word(*src_addr & ~3);
            ioreg.dma_value.dw = value;
            if (!bad_dst_addr) memory_write_word(*dst_addr & ~3, value);
        } else {
            uint16_t value = ioreg.dma_value.w.w0;
            if (!bad_src_addr) value = memory_read_halfword(*src_addr & ~1);
            ioreg.dma_value.w.w0 = value;
            if (!bad_dst_addr) memory_write_halfword(*dst_addr & ~1, value);
        }

        if (dst_ctrl == DMA_INC || dst_ctrl == DMA_RELOAD) *dst_addr += size;
        else if (dst_ctrl == DMA_DEC) *dst_addr -= size;
        if (src_ctrl == DMA_INC) *src_addr += size;
        else if (src_ctrl == DMA_DEC) *src_addr -= size;
    }
}

void gba_dma_reset(int ch) {
    uint32_t sad = ioreg.dma[ch].sad.dw;
    uint32_t dad = ioreg.dma[ch].dad.dw;
    uint32_t cnt = ioreg.dma[ch].cnt.dw;

    ioreg.dma[ch].src_addr = sad;
    ioreg.dma[ch].dst_addr = dad;
    ioreg.dma[ch].count = (uint16_t) cnt;
    if (ioreg.dma[ch].count == 0) ioreg.dma[ch].count = (ch == 3 ? 0x10000 : 0x4000);
}

void gba_dma_update(uint32_t current_timing) {
    for (int ch = 0; ch < 4; ch++) {
        uint32_t dad = ioreg.dma[ch].dad.dw;
        uint32_t cnt = ioreg.dma[ch].cnt.dw;
        uint32_t start_timing = BITS(cnt, 28, 29);

        if (!(cnt & DMA_ENABLE)) continue;
        if (start_timing != current_timing) continue;

        uint32_t *dst_addr = &ioreg.dma[ch].dst_addr;
        uint32_t *src_addr = &ioreg.dma[ch].src_addr;
        uint16_t count = ioreg.dma[ch].count;

        uint32_t dst_ctrl = BITS(cnt, 21, 22);
        uint32_t src_ctrl = BITS(cnt, 23, 24);
        if (*src_addr >= 0x08000000 && *src_addr < 0x0e000000) src_ctrl = DMA_INC;
        bool word_size = (cnt & DMA_32);

        if (start_timing == DMA_AT_VBLANK) assert(ioreg.vcount.w == 160);
        if (start_timing == DMA_AT_HBLANK) assert(ppu_cycles < 197120 && ppu_cycles % 1232 == 1006);
        if (start_timing == DMA_AT_REFRESH) {
            if (ch == 0) {
                continue;
            } else if (ch == 1 || ch == 2) {
                if (!(*dst_addr == 0x40000a0 || *dst_addr == 0x40000a4)) continue;
                assert(cnt & DMA_REPEAT);
                if (*dst_addr == 0x40000a0 && !ioreg.fifo_a_refill) continue;
                if (*dst_addr == 0x40000a4 && !ioreg.fifo_b_refill) continue;
                dst_ctrl = DMA_FIXED;
                word_size = true;
                count = 4;
            } else if (ch == 3) {
                continue;  // FIXME
            }
        }

        assert(src_ctrl != DMA_RELOAD);
        assert(!(cnt & DMA_DRQ));

        // EEPROM size autodetect
        if (has_eeprom && game_rom_size <= 0x1000000 && (*dst_addr >= 0x0d000000 && *dst_addr < 0x0e000000)) {
            if (count == 9 || count == 73) {
                eeprom_width = 6;
            } else if (count == 17 || count == 81) {
                eeprom_width = 14;
            }
        }

        active_dma = ch;
        gba_dma_transfer(ch, dst_ctrl, src_ctrl, dst_addr, src_addr, word_size ? 4 : 2, count);
        active_dma = -1;

        if (cnt & DMA_IRQ) {
            ioreg.irq.w |= 1 << (8 + ch);
        }

        if (cnt & DMA_REPEAT) {
            if (dst_ctrl == DMA_RELOAD) ioreg.dma[ch].dst_addr = dad;
            ioreg.dma[ch].count = (uint16_t) cnt;
            if (ioreg.dma[ch].count == 0) ioreg.dma[ch].count = (ch == 3 ? 0x10000 : 0x4000);
        } else {
            ioreg.dma[ch].cnt.dw &= ~DMA_ENABLE;
        }
    }
}

void gba_emulate(void) {
    while (true) {
        int cpu_cycles = 0;

        if (FLAG_T()) {
            if (branch_taken) thumb_fill_pipeline();
            if (!halted) cpu_cycles = thumb_step();
        } else {
            if (branch_taken) arm_fill_pipeline();
            if (!halted) cpu_cycles = arm_step();
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

        if (!branch_taken && !(cpsr & PSR_I) && ioreg.ime.w && (ioreg.irq.w & ioreg.ie.w)) {
            arm_hardware_interrupt();
            halted = false;
        }

        gba_timer_update(1);  // FIXME
        gba_ppu_update();
        if (ppu_cycles == 0 || (single_step && cpu_cycles > 0)) break;
    }
}

// Main code
int main(int argc, char **argv) {
    arm_init_lookup();
    thumb_init_lookup();

    load_bios_file();
    gba_reset(false);

    if (argc == 2) {
        skip_bios = true;
        gba_load(argv[1]);
    }

    // Setup SDL
    // (Some versions of SDL before 2.0.10 appear to have performance/stalling issues on a minority of Windows systems,
    // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled... updating to latest version of SDL is recommended!)
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
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
    SDL_Window *window = SDL_CreateWindow("ygba", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 800, window_flags);
    if (window == NULL) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);  // Enable vsync

    // Enable drag and drop
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    // Initalize audio
    SDL_AudioDeviceID audio_device = gba_audio_init();

    // Initialize gamepad
    SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt");
    SDL_GameController *game_controller = NULL;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            game_controller = SDL_GameControllerOpen(i);
            if (game_controller != NULL) break;
            SDL_Log("Failed to open game controller %d: %s", i, SDL_GetError());
        }
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
    bool show_debugger_window = true;
    bool show_memory_window = false;
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
        ImGui_ImplSDL2_NewFrame();
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
            ImGui::Checkbox("Debugger Window", &show_debugger_window);
            ImGui::SameLine();
            ImGui::Checkbox("Memory Window", &show_memory_window);

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

        // Input
        const Uint8 *key_state = SDL_GetKeyboardState(NULL);
        done |= (bool) key_state[SDL_SCANCODE_ESCAPE];
        static bool keys[10];
        memset(keys, 0, sizeof(keys));
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
        if (game_controller != NULL) {
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
        if (keys[4] && keys[5]) { keys[4] = false; keys[5] = false; }  // Disallow opposing directions
        if (keys[6] && keys[7]) { keys[6] = false; keys[7] = false; }
        ioreg.keyinput.w = 0x3ff;
        for (int i = 0; i < 10; i++) {
            if (keys[i]) {
                ioreg.keyinput.w &= ~(1 << i);
            }
        }

        static bool paused = false;
        if (!paused) {
            gba_emulate();
            if (single_step) paused = true;
        }

        glBindTexture(GL_TEXTURE_2D, screen_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, screen_pixels);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Screen
        ImGui::Begin("Screen");
        ImGui::SliderInt("Scale", &screen_scale, 1, 5);
        ImVec2 screen_size = ImVec2((float) SCREEN_WIDTH * screen_scale, (float) SCREEN_HEIGHT * screen_scale);
        ImGui::Image((void *)(intptr_t) screen_texture, screen_size);
        ImGui::End();

        // Debugger
        static char cpsr_flag_text[8];
        static char cpsr_mode_text[11];
        static char disasm_text[256];
        if (show_debugger_window) {
            ImGui::Begin("Debugger", &show_debugger_window);
            ImGui::Text(" r0: %08X   r1: %08X   r2: %08X   r3: %08X", r[0], r[1], r[2], r[3]);
            ImGui::Text(" r4: %08X   r5: %08X   r6: %08X   r7: %08X", r[4], r[5], r[6], r[7]);
            ImGui::Text(" r8: %08X   r9: %08X  r10: %08X  r11: %08X", r[8], r[9], r[10], r[11]);
            ImGui::Text("r12: %08X  r13: %08X  r14: %08X  r15: %08X", r[12], r[13], r[14], get_pc());
            cpsr_flag_text[0] = (cpsr & PSR_N ? 'N' : '-');
            cpsr_flag_text[1] = (cpsr & PSR_Z ? 'Z' : '-');
            cpsr_flag_text[2] = (cpsr & PSR_C ? 'C' : '-');
            cpsr_flag_text[3] = (cpsr & PSR_V ? 'V' : '-');
            cpsr_flag_text[4] = (cpsr & PSR_I ? 'I' : '-');
            cpsr_flag_text[5] = (cpsr & PSR_F ? 'F' : '-');
            cpsr_flag_text[6] = (cpsr & PSR_T ? 'T' : '-');
            cpsr_flag_text[7] = '\0';
            switch (cpsr & PSR_MODE) {
                case PSR_MODE_USR: strcpy(cpsr_mode_text, "User"); break;
                case PSR_MODE_FIQ: strcpy(cpsr_mode_text, "FIQ"); break;
                case PSR_MODE_IRQ: strcpy(cpsr_mode_text, "IRQ"); break;
                case PSR_MODE_SVC: strcpy(cpsr_mode_text, "Supervisor"); break;
                case PSR_MODE_ABT: strcpy(cpsr_mode_text, "Abort"); break;
                case PSR_MODE_UND: strcpy(cpsr_mode_text, "Undefined"); break;
                case PSR_MODE_SYS: strcpy(cpsr_mode_text, "System"); break;
                default: strcpy(cpsr_mode_text, "Illegal"); break;
            }
            ImGui::Text("cpsr: %08X [%s] %s", cpsr, cpsr_flag_text, cpsr_mode_text);
            if (ImGui::Button("Run")) {
                paused = false;
                single_step = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Step")) {
                paused = false;
                single_step = true;
            }
            if (ImGui::BeginTable("disassembly", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable)) {
                uint32_t pc = get_pc();
                for (int i = 0; i < 10; i++) {
                    uint32_t address = pc + i * SIZEOF_INSTR;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%08X", address);
                    if (FLAG_T()) {
                        uint16_t op = memory_read_halfword(address);
                        ImGui::TableNextColumn();
                        ImGui::Text("%04X", op);
                        ImGui::TableNextColumn();
                        thumb_disasm(address, op, disasm_text);
                    } else {
                        uint32_t op = memory_read_word(address);
                        ImGui::TableNextColumn();
                        ImGui::Text("%08X", op);
                        ImGui::TableNextColumn();
                        arm_disasm(address, op, disasm_text);
                    }
                    ImGui::Text("%s", disasm_text);
                }
                ImGui::EndTable();
            }
            ImGui::End();
        }

        // Memory
        static MemoryEditor mem_edit;
        mem_edit.ReadFn = [](const uint8_t *data, size_t off) { UNUSED(data); return memory_read_byte(off); };
        mem_edit.WriteFn = [](uint8_t *data, size_t off, uint8_t d) { UNUSED(data); memory_write_byte(off, d); };
        if (show_memory_window) {
            ImGui::Begin("Memory", &show_memory_window);
            mem_edit.DrawContents(NULL, 0x10000000);
            ImGui::End();
        }

        // Settings
        ImGui::Begin("Settings");
        ImGui::Checkbox("Has EEPROM", &has_eeprom);
        ImGui::Checkbox("Has Flash", &has_flash);
        ImGui::Checkbox("Has SRAM", &has_sram);
        ImGui::Checkbox("Has RTC", &has_rtc);
        ImGui::Checkbox("Skip BIOS", &skip_bios);

        static bool sync_to_video = true;
        ImGui::Checkbox("Sync to video", &sync_to_video);
        SDL_GL_SetSwapInterval(sync_to_video ? 1 : 0);

        static bool mute_audio = false;
        ImGui::Checkbox("Mute audio", &mute_audio);
        SDL_PauseAudioDevice(audio_device, mute_audio ? 1 : 0);

        ImGui::Text("DMA1SAD: %08X", ioreg.dma[1].src_addr);
        ImGui::Text("DMA2SAD: %08X", ioreg.dma[2].src_addr);
        ImGui::Text("fifo_a_r: %d", ioreg.fifo_a_r);
        ImGui::Text("fifo_a_w: %d", ioreg.fifo_a_w);
        ImGui::Text("fifo_b_r: %d", ioreg.fifo_b_r);
        ImGui::Text("fifo_b_w: %d", ioreg.fifo_b_w);

        if (ImGui::Button("Reset")) {
            gba_reset(true);
        }
        if (ImGui::Button("Manual save")) {
            write_save_file();
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

    if (save_path != NULL) {
        write_save_file();
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    glDeleteTextures(1, &screen_texture);

    if (game_controller != NULL) {
        SDL_GameControllerClose(game_controller);
    }
    if (audio_device != 0) {
        SDL_CloseAudioDevice(audio_device);
    }
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
