// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#include "video.h"

#include <stdint.h>
#include <cassert>
#include <cmath>

#include "cpu.h"
#include "dma.h"
#include "io.h"
#include "memory.h"
#include "system.h"

uint32_t video_cycles;
bool video_frame_drawn;

uint32_t screen_texture;
uint32_t screen_pixels[SCREEN_HEIGHT][SCREEN_WIDTH];

enum ScanlineFlags {
    EnableBlend = 1,
    SpriteBlend = 2,
    SpriteMask = 4
};

struct ScanlineInfo {
    uint16_t top;
    uint16_t bottom;
    uint8_t top_bg;
    uint8_t bottom_bg;
    uint8_t flags;
};

ScanlineInfo scanline[SCREEN_WIDTH];

bool active_compute_sprite_masks;
bool active_sprite_blend;
bool active_sprite_mask;

struct WindowInfo {
    int left;
    int top;
    int right;
    int bottom;
};

WindowInfo win0, win1;

static bool is_point_in_window(int x, int y, WindowInfo win) {
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

static double fixed1p4_to_double(int8_t x) {
    return (x >> 4) + ((x & 0xf) / 16.0);
}

static double fixed8p8_to_double(int16_t x) {
    return (x >> 8) + ((x & 0xff) / 256.0);
}

static double fixed20p8_to_double(int32_t x) {
    SIGN_EXTEND(x, 27);
    return (x >> 8) + ((x & 0xff) / 256.0);
}

bool video_in_bitmap_mode() {
    uint16_t mode = ioreg.dispcnt.w & 7;
    return (mode >= 3 && mode <= 5);
}

static uint32_t rgb555_to_rgb888(uint16_t pixel) {
    int red = BITS(pixel, 0, 4);
    int green = BITS(pixel, 5, 9);
    int blue = BITS(pixel, 10, 14);

    red = (red << 3) | (red >> 2);
    green = (green << 3) | (green >> 2);
    blue = (blue << 3) | (blue >> 2);

    return 0xff << 24 | blue << 16 | green << 8 | red;
}

static uint16_t rgb565_blend(uint16_t a, uint16_t b, double weight_a, double weight_b) {
    int red_a = BITS(a, 0, 4);
    int green_a = BITS(a, 5, 9) << 1 | BIT(a, 15);
    int blue_a = BITS(a, 10, 14);

    int red_b = BITS(b, 0, 4);
    int green_b = BITS(b, 5, 9) << 1 | BIT(b, 15);
    int blue_b = BITS(b, 10, 14);

    int red = red_a * weight_a + red_b * weight_b;
    int green = green_a * weight_a + green_b * weight_b;
    int blue = blue_a * weight_a + blue_b * weight_b;

    if (red > 31) red = 31;
    if (green > 63) green = 63;
    if (blue > 31) blue = 31;

    return BIT(green, 0) << 15 | BITS(blue, 0, 4) << 10 | BITS(green, 1, 5) << 5 | BITS(red, 0, 4);
}

static void draw_forced_blank(int y) {
    assert(y >= 0 && y < SCREEN_HEIGHT);

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        screen_pixels[y][x] = 0xffffffff;
    }
}

static void draw_backdrop(int y) {
    assert(y >= 0 && y < SCREEN_HEIGHT);

    uint16_t pixel = *(uint16_t *) &palette_ram[0];

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        scanline[x].top = pixel;
        scanline[x].bottom = pixel;
        scanline[x].top_bg = 5;
        scanline[x].bottom_bg = 5;
        scanline[x].flags = ScanlineFlags::EnableBlend;
    }
}

static void draw_pixel_if_visible(int bg, int x, int y, uint16_t pixel) {
    if (x < 0 || x >= SCREEN_WIDTH) return;
    assert(y >= 0 && y < SCREEN_HEIGHT);

    if (active_compute_sprite_masks) {
        scanline[x].flags |= ScanlineFlags::SpriteMask;
        return;
    }

    bool enable_win0 = (ioreg.dispcnt.w & DCNT_WIN0);
    bool enable_win1 = (ioreg.dispcnt.w & DCNT_WIN1);
    bool enable_winobj = (ioreg.dispcnt.w & DCNT_WINOBJ);
    bool enable_winout = (enable_win0 || enable_win1 || enable_winobj);

    bool inside_win0 = (enable_win0 && is_point_in_window(x, y, win0));
    bool inside_win1 = (enable_win1 && is_point_in_window(x, y, win1));
    bool inside_winobj = (enable_winobj && (scanline[x].flags & ScanlineFlags::SpriteMask));

    bool enable_blend = true;
    bool visible = true;

    if (inside_win0) {
        enable_blend = BIT(ioreg.winin.w, 5);
        visible = BIT(ioreg.winin.w, bg);
    } else if (inside_win1) {
        enable_blend = BIT(ioreg.winin.w, 13);
        visible = BIT(ioreg.winin.w, 8 + bg);
    } else if (inside_winobj) {
        enable_blend = BIT(ioreg.winout.w, 13);
        visible = BIT(ioreg.winout.w, 8 + bg);
    } else if (enable_winout) {
        enable_blend = BIT(ioreg.winout.w, 5);
        visible = BIT(ioreg.winout.w, bg);
    }

    if (!enable_blend) {
        scanline[x].flags &= ~ScanlineFlags::EnableBlend;
    }
    if (active_sprite_blend) {
        scanline[x].flags |= ScanlineFlags::SpriteBlend;
    } else {
        scanline[x].flags &= ~ScanlineFlags::SpriteBlend;
    }
    if (active_sprite_mask) {
        visible = false;
    }

    if (!visible) return;

    bool occluding_sprites = (bg == 4 && scanline[x].top_bg == 4);
    if (!occluding_sprites) {
        scanline[x].bottom = scanline[x].top;
        scanline[x].bottom_bg = scanline[x].top_bg;
    }

    scanline[x].top = pixel;
    scanline[x].top_bg = bg;
}

static void compose_scanline(int y) {
    assert(y >= 0 && y < SCREEN_HEIGHT);

    int blend_top_bgs = BITS(ioreg.bldcnt.w, 0, 5);
    int blend_mode_default = BITS(ioreg.bldcnt.w, 6, 7);
    int blend_bottom_bgs = BITS(ioreg.bldcnt.w, 8, 13);
    double weight_a = fixed1p4_to_double(BITS(ioreg.bldalpha.w, 0, 4));
    double weight_b = fixed1p4_to_double(BITS(ioreg.bldalpha.w, 8, 12));
    double weight_y = fixed1p4_to_double(BITS(ioreg.bldy.w, 0, 4));

    if (weight_a > 1.0) weight_a = 1.0;
    if (weight_b > 1.0) weight_b = 1.0;
    if (weight_y > 1.0) weight_y = 1.0;

    const uint16_t white = 0xffff;
    const uint16_t black = 0;

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        uint16_t top = scanline[x].top;
        uint16_t bottom = scanline[x].bottom;
        uint8_t top_bg = scanline[x].top_bg;
        uint8_t bottom_bg = scanline[x].bottom_bg;
        bool enable_blend = scanline[x].flags & ScanlineFlags::EnableBlend;
        bool sprite_blend = scanline[x].flags & ScanlineFlags::SpriteBlend;

        bool top_ok = BIT(blend_top_bgs, top_bg);
        bool bottom_ok = BIT(blend_bottom_bgs, bottom_bg);

        int blend_mode = blend_mode_default;
        bool valid_blend = enable_blend && top_ok && (bottom_ok || blend_mode != 1);

        if (sprite_blend) {
            if ((top_ok && bottom_ok) || (!top_ok && blend_bottom_bgs != 0)) {
                blend_mode = 1;
                valid_blend = true;
            }
        }

        uint16_t pixel = top;
        if (valid_blend) {
            switch (blend_mode) {
                case 1:
                    pixel = rgb565_blend(top, bottom, weight_a, weight_b);
                    break;
                case 2:
                    pixel = rgb565_blend(top, white, 1.0 - weight_y, weight_y);
                    break;
                case 3:
                    pixel = rgb565_blend(top, black, 1.0 - weight_y, weight_y);
                    break;
            }
        }
        screen_pixels[y][x] = rgb555_to_rgb888(pixel);
    }
}

static bool tile_access(uint32_t tile_address, int x, int y, bool hflip, bool vflip, bool colors_256, uint32_t palette_offset, int palette_no, uint16_t *pixel) {
    assert(x >= 0 && x < 8 && y >= 0 && y < 8);

    if (hflip) x = 7 - x;
    if (vflip) y = 7 - y;

    uint8_t *tile = &video_ram[tile_address];

    if (colors_256) {
        uint32_t tile_offset = y * 8 + x;
        uint8_t pixel_index = tile[tile_offset];
        if (pixel_index != 0) {
            *pixel = *(uint16_t *) &palette_ram[palette_offset + pixel_index * 2];
            return true;
        }
    } else {
        uint32_t tile_offset = y * 4 + x / 2;
        uint8_t pixel_indexes = tile[tile_offset];
        uint8_t pixel_index = (pixel_indexes >> (x % 2 == 1 ? 4 : 0)) & 0xf;
        if (pixel_index != 0) {
            *pixel = *(uint16_t *) &palette_ram[palette_offset + palette_no * 32 + pixel_index * 2];
            return true;
        }
    }

    return false;
}

static bool bg_regular_access(int x, int y, int w, int h, uint32_t tile_base, uint32_t map_base, uint32_t screen_size, bool colors_256, uint16_t *pixel) {
    assert(x >= 0 && x < w && y >= 0 && y < h);

    int map_x = (x / 8) % (w / 8);
    int map_y = (y / 8) % (h / 8);
    int quad_x = 32 * 32;
    int quad_y = 32 * 32 * (screen_size == 3 ? 2 : 1);
    uint32_t map_index = (map_y / 32) * quad_y + (map_x / 32) * quad_x + (map_y % 32) * 32 + (map_x % 32);
    uint16_t info = *(uint16_t *) &video_ram[map_base + map_index * 2];
    int tile_no = BITS(info, 0, 9);
    bool hflip = BIT(info, 10);
    bool vflip = BIT(info, 11);
    int palette_no = BITS(info, 12, 15);

    uint32_t tile_address = tile_base + tile_no * (colors_256 ? 64 : 32);
    if (tile_address >= 0x10000) return false;
    return tile_access(tile_address, x % 8, y % 8, hflip, vflip, colors_256, 0, palette_no, pixel);
}

static bool bg_affine_access(int x, int y, int w, int h, uint32_t tile_base, uint32_t map_base, uint16_t *pixel) {
    if (x < 0 || x >= w || y < 0 || y >= h) return false;

    int map_x = (x / 8) % (w / 8);
    int map_y = (y / 8) % (h / 8);
    uint32_t map_index = map_y * (w / 8) + map_x;
    uint8_t info = video_ram[map_base + map_index];
    int tile_no = info;

    uint32_t tile_address = tile_base + tile_no * 64;
    if (tile_address >= 0x10000) return false;
    return tile_access(tile_address, x % 8, y % 8, false, false, true, 0, 0, pixel);
}

static bool sprite_access(int tile_no, int x, int y, int w, int h, bool hflip, bool vflip, bool colors_256, int palette_no, int mode, uint16_t *pixel) {
    if (x < 0 || x >= w || y < 0 || y >= h) return false;

    if (hflip) x = w - 1 - x;
    if (vflip) y = h - 1 - y;

    bool obj_1d = (ioreg.dispcnt.w & DCNT_OBJ_1D);
    int stride = (obj_1d ? (w / 8) : (colors_256 ? 16 : 32));
    int increment = (colors_256 ? 2 : 1);
    int count_y = (y / 8) * stride * increment;
    int count_x = (x / 8) * increment;

    tile_no += count_y;
    if (obj_1d) {
        tile_no += count_x;
    } else {
        tile_no = (tile_no & ~0x1f) | ((tile_no + count_x) & 0x1f);
    }
    tile_no &= 0x3ff;

    bool bitmap_mode = (mode >= 3 && mode <= 5);
    if (bitmap_mode && tile_no < 512) return false;

    uint32_t tile_address = 0x10000 + tile_no * 32;
    return tile_access(tile_address, x % 8, y % 8, false, false, colors_256, 0x200, palette_no, pixel);
}

const int sprite_width_lookup[4][4] = {{8, 16, 32, 64}, {16, 32, 32, 64}, {8, 8, 16, 32}, {8, 8, 8, 8}};
const int sprite_height_lookup[4][4] = {{8, 16, 32, 64}, {8, 8, 16, 32}, {16, 32, 32, 64}, {8, 8, 8, 8}};

static void draw_sprites(int mode, int pri, int y) {
    for (int n = 127; n >= 0; n--) {
        uint16_t attr0 = *(uint16_t *) &object_ram[n * 8];
        uint16_t attr1 = *(uint16_t *) &object_ram[n * 8 + 2];
        uint16_t attr2 = *(uint16_t *) &object_ram[n * 8 + 4];

        int sprite_y = BITS(attr0, 0, 7);
        int obj_mode = BITS(attr0, 8, 9);
        int gfx_mode = BITS(attr0, 10, 11);
        //bool mosaic = BIT(attr0, 12);
        bool colors_256 = BIT(attr0, 13);
        int shape = BITS(attr0, 14, 15);

        if (active_compute_sprite_masks && gfx_mode != 2) continue;
        assert(gfx_mode != 3);

        int sprite_x = BITS(attr1, 0, 8);
        int affine_index = BITS(attr1, 9, 13);
        bool hflip = BIT(attr1, 12);
        bool vflip = BIT(attr1, 13);
        int size = BITS(attr1, 14, 15);

        int tile_no = BITS(attr2, 0, 9);
        int priority = BITS(attr2, 10, 11);
        int palette_no = BITS(attr2, 12, 15);

        if (obj_mode == 2 || priority != pri) continue;

        bool is_affine = (obj_mode == 1 || obj_mode == 3);
        int bbox_scale = (obj_mode == 3 ? 2 : 1);

        int sprite_width = sprite_width_lookup[shape][size];
        int sprite_height = sprite_height_lookup[shape][size];
        int bbox_width = sprite_width * bbox_scale;
        int bbox_height = sprite_height * bbox_scale;

        if (sprite_x + bbox_width >= 512) sprite_x -= 512;
        if (sprite_y + bbox_height >= 256) sprite_y -= 256;

        if (y < sprite_y || y >= sprite_y + bbox_height) continue;

        int sprite_cx = sprite_width / 2;
        int sprite_cy = sprite_height / 2;
        int bbox_cx = bbox_width / 2;
        int bbox_cy = bbox_height / 2;

        double pa, pb, pc, pd;
        if (is_affine) {
            pa = fixed8p8_to_double(*(uint16_t *) &object_ram[affine_index * 32 + 6]);
            pb = fixed8p8_to_double(*(uint16_t *) &object_ram[affine_index * 32 + 14]);
            pc = fixed8p8_to_double(*(uint16_t *) &object_ram[affine_index * 32 + 22]);
            pd = fixed8p8_to_double(*(uint16_t *) &object_ram[affine_index * 32 + 30]);
            hflip = false;
            vflip = false;
        } else {
            pa = pd = 1.0;
            pb = pc = 0.0;
        }

        active_sprite_blend = (gfx_mode == 1);
        active_sprite_mask = (gfx_mode == 2);

        int j = y - sprite_y;
        for (int i = 0; i < bbox_width; i++) {
            int texture_x = sprite_cx + std::floor(pa * (i - bbox_cx) + pb * (j - bbox_cy));
            int texture_y = sprite_cy + std::floor(pc * (i - bbox_cx) + pd * (j - bbox_cy));
            uint16_t pixel;
            bool ok = sprite_access(tile_no, texture_x, texture_y, sprite_width, sprite_height, hflip, vflip, colors_256, palette_no, mode, &pixel);
            if (ok) draw_pixel_if_visible(4, sprite_x + i, y, pixel);
        }

        active_sprite_blend = false;
        active_sprite_mask = false;
    }
}

static void compute_sprite_masks(int mode, int y) {
    active_compute_sprite_masks = true;

    for (int pri = 3; pri >= 0; pri--) {
        draw_sprites(mode, pri, y);
    }

    active_compute_sprite_masks = false;
}

const int bg_width_lookup[2][4] = {{256, 512, 256, 512}, {128, 256, 512, 1024}};
const int bg_height_lookup[2][4] = {{256, 256, 512, 512}, {128, 256, 512, 1024}};

static void draw_tiled_bg(int mode, int bg, int y) {
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
    double affine_x, affine_y;
    double pa, pc;
    if (is_affine) {
        affine_x = ioreg.bg_affine[bg - 2].x;
        affine_y = ioreg.bg_affine[bg - 2].y;
        pa = fixed8p8_to_double(ioreg.bg_affine[bg - 2].pa.w);
        pc = fixed8p8_to_double(ioreg.bg_affine[bg - 2].pc.w);
    } else {
        affine_x = 0.0;
        affine_y = 0.0;
        pa = 0.0;
        pc = 0.0;
    }

    int bg_width = bg_width_lookup[is_affine][screen_size];
    int bg_height = bg_height_lookup[is_affine][screen_size];

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        int i = (is_affine ? std::floor(affine_x) : x + hofs);
        int j = (is_affine ? std::floor(affine_y) : y + vofs);
        if (!is_affine || overflow_wraps) {
            i &= bg_width - 1;
            j &= bg_height - 1;
        }
        uint16_t pixel;
        bool ok;
        if (is_affine) {
            ok = bg_affine_access(i, j, bg_width, bg_height, tile_base, map_base, &pixel);
        } else {
            ok = bg_regular_access(i, j, bg_width, bg_height, tile_base, map_base, screen_size, colors_256, &pixel);
        }
        if (ok) draw_pixel_if_visible(bg, x, y, pixel);
        affine_x += pa;
        affine_y += pc;
    }
}

static void draw_tiled(int mode, int y) {
    compute_sprite_masks(mode, y);

    for (int pri = 3; pri >= 0; pri--) {
        for (int bg = 3; bg >= 0; bg--) {
            bool bg_visible = BIT(ioreg.dispcnt.w, 8 + bg);
            uint16_t priority = BITS(ioreg.bgcnt[bg].w, 0, 1);

            if (bg_visible && priority == pri) {
                draw_tiled_bg(mode, bg, y);
            }
        }

        bool obj_visible = (ioreg.dispcnt.w & DCNT_OBJ);

        if (obj_visible) {
            draw_sprites(mode, pri, y);
        }
    }
}

static bool bitmap_access(int x, int y, int mode, uint16_t *pixel) {
    int w = (mode == 5 ? 160 : SCREEN_WIDTH);
    int h = (mode == 5 ? 128 : SCREEN_HEIGHT);

    if (x < 0 || x >= w || y < 0 || y >= h) return false;

    if (mode == 4) {
        bool page_flip = (ioreg.dispcnt.w & DCNT_PAGE);
        uint8_t pixel_index = video_ram[(page_flip ? 0xa000 : 0) + y * w + x];
        if (pixel_index != 0) {
            *pixel = *(uint16_t *) &palette_ram[pixel_index * 2];
            return true;
        }
    } else {
        *pixel = *(uint16_t *) &video_ram[(y * w + x) * 2];
        return true;
    }

    return false;
}

static void draw_bitmap(int mode, int y) {
    compute_sprite_masks(mode, y);

    for (int pri = 3; pri >= 0; pri--) {
        const int bg = 2;

        bool bg_visible = BIT(ioreg.dispcnt.w, 8 + bg);
        uint16_t priority = BITS(ioreg.bgcnt[bg].w, 0, 1);

        if (bg_visible && priority == pri) {
            double affine_x = ioreg.bg_affine[bg - 2].x;
            double affine_y = ioreg.bg_affine[bg - 2].y;
            double pa = fixed8p8_to_double(ioreg.bg_affine[bg - 2].pa.w);
            double pc = fixed8p8_to_double(ioreg.bg_affine[bg - 2].pc.w);

            for (int x = 0; x < SCREEN_WIDTH; x++) {
                int i = std::floor(affine_x);
                int j = std::floor(affine_y);
                uint16_t pixel;
                bool ok = bitmap_access(i, j, mode, &pixel);
                if (ok) draw_pixel_if_visible(bg, x, y, pixel);
                affine_x += pa;
                affine_y += pc;
            }
        }

        bool obj_visible = (ioreg.dispcnt.w & DCNT_OBJ);

        if (obj_visible) {
            draw_sprites(mode, pri, y);
        }
    }
}

static void video_draw_scanline() {
    win0.right = ioreg.winh[0].b.b0;
    win0.left = ioreg.winh[0].b.b1;
    win0.bottom = ioreg.winv[0].b.b0;
    win0.top = ioreg.winv[0].b.b1;
    win1.right = ioreg.winh[1].b.b0;
    win1.left = ioreg.winh[1].b.b1;
    win1.bottom = ioreg.winv[1].b.b0;
    win1.top = ioreg.winv[1].b.b1;

    bool forced_blank = (ioreg.dispcnt.w & DCNT_BLANK);
    int y = ioreg.vcount.w;

    if (forced_blank) {
        draw_forced_blank(y);
        return;
    }

    draw_backdrop(y);

    int mode = BITS(ioreg.dispcnt.w, 0, 2);
    switch (mode) {
        case 0:
        case 1:
        case 2:
            //case 6:
            //case 7:
            draw_tiled(mode, y);
            break;

        case 3:
        case 4:
        case 5:
            draw_bitmap(mode, y);
            break;

        default:
            assert(false);
            break;
    }

    compose_scanline(y);
}

void video_bg_affine_reset(int i) {
    ioreg.bg_affine[i].x = fixed20p8_to_double(ioreg.bg_affine[i].x0.dw);
    ioreg.bg_affine[i].y = fixed20p8_to_double(ioreg.bg_affine[i].y0.dw);
}

static void video_bg_affine_update() {
    for (int i = 0; i < 2; i++) {
        ioreg.bg_affine[i].x += fixed8p8_to_double(ioreg.bg_affine[i].pb.w);
        ioreg.bg_affine[i].y += fixed8p8_to_double(ioreg.bg_affine[i].pd.w);
    }
}

void video_update(uint32_t cycles) {
    uint32_t last_frame_cycles = video_cycles;
    video_cycles = (video_cycles + cycles) % CYCLES_FRAME;
    uint32_t frame_cycles = video_cycles;

    uint32_t last_line_cycles = last_frame_cycles % CYCLES_SCANLINE;
    uint32_t line_cycles = frame_cycles % CYCLES_SCANLINE;

    if (line_cycles >= CYCLES_HDRAW && last_line_cycles < CYCLES_HDRAW) {
        if (ioreg.vcount.w < SCREEN_HEIGHT) {
            video_draw_scanline();
            video_bg_affine_update();
        }
        ioreg.dispstat.w |= DSTAT_IN_HBL;  // Enter HBlank
        if (ioreg.dispstat.w & DSTAT_HBL_IRQ) {
            ioreg.irq.w |= INT_HBLANK;
        }
        if (ioreg.vcount.w < SCREEN_HEIGHT) {
            dma_update(DMA_AT_HBLANK);
        }
    }

    if (line_cycles < last_line_cycles) {
        ioreg.dispstat.w &= ~DSTAT_IN_HBL;  // Leave HBlank
        ioreg.vcount.w = (ioreg.vcount.w + 1) % NUM_SCANLINES;
        if (ioreg.vcount.w == 0) {
            video_bg_affine_reset(0);
            video_bg_affine_reset(1);
        } else if (ioreg.vcount.w == SCREEN_HEIGHT) {
            ioreg.dispstat.w |= DSTAT_IN_VBL;  // Enter VBlank
            dma_update(DMA_AT_VBLANK);
        } else if (ioreg.vcount.w == SCREEN_HEIGHT + 1) {
            // FIXME Implement proper IRQ delay
            if (ioreg.dispstat.w & DSTAT_VBL_IRQ) {
                ioreg.irq.w |= INT_VBLANK;
            }
        } else if (ioreg.vcount.w == NUM_SCANLINES - 1) {
            ioreg.dispstat.w &= ~DSTAT_IN_VBL;  // Leave VBlank
        }
        if (ioreg.vcount.w == ioreg.dispstat.b.b1) {
            ioreg.dispstat.w |= DSTAT_IN_VCT;  // Enter VCount
            if (ioreg.dispstat.w & DSTAT_VCT_IRQ) {
                ioreg.irq.w |= INT_VCOUNT;
            }
        } else {
            ioreg.dispstat.w &= ~DSTAT_IN_VCT;  // Leave VCount
        }
    }

    if (frame_cycles < last_frame_cycles) {
        video_frame_drawn = true;
    }
}
