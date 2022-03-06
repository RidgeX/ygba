// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <stdint.h>

#define SCREEN_WIDTH    240
#define SCREEN_HEIGHT   160

#define NUM_SCANLINES   (SCREEN_HEIGHT + 68)

#define CYCLES_HDRAW    1006
#define CYCLES_HBLANK   226
#define CYCLES_SCANLINE (CYCLES_HDRAW + CYCLES_HBLANK)
#define CYCLES_VDRAW    (CYCLES_SCANLINE * SCREEN_HEIGHT)
#define CYCLES_VBLANK   (CYCLES_SCANLINE * 68)
#define CYCLES_FRAME    (CYCLES_VDRAW + CYCLES_VBLANK)

extern uint32_t screen_texture;
extern uint32_t screen_pixels[SCREEN_HEIGHT][SCREEN_WIDTH];

#define DCNT_GB      (1 << 3)
#define DCNT_PAGE    (1 << 4)
#define DCNT_OAM_HBL (1 << 5)
#define DCNT_OBJ_1D  (1 << 6)
#define DCNT_BLANK   (1 << 7)
#define DCNT_BG0     (1 << 8)
#define DCNT_BG1     (1 << 9)
#define DCNT_BG2     (1 << 10)
#define DCNT_BG3     (1 << 11)
#define DCNT_OBJ     (1 << 12)
#define DCNT_WIN0    (1 << 13)
#define DCNT_WIN1    (1 << 14)
#define DCNT_WINOBJ  (1 << 15)

void video_draw_scanline();
void video_affine_reset(int i);
void video_affine_update();
