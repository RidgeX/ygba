// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <stdint.h>

#include <SDL.h>

void audio_fifo_a(uint32_t sample);
void audio_fifo_b(uint32_t sample);
SDL_AudioDeviceID audio_init();
