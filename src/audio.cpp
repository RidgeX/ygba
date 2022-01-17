// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#include "audio.h"

#include <stdint.h>
#include <cstdlib>
#include <cstring>

#include <SDL.h>

#include "cpu.h"
#include "io.h"

static double cubic_interpolate(int8_t *history, double mu) {
    double A = history[3] - history[2] - history[0] + history[1];
    double B = history[0] - history[1] - A;
    double C = history[2] - history[0];
    double D = history[1];
    return A * mu * mu * mu + B * mu * mu + C * mu + D;
}

static int16_t clamp_i16(int32_t x, int16_t min, int16_t max) {
    x = x < min ? min : x;
    x = x > max ? max : x;
    return x;
}

static void audio_callback(void *userdata, uint8_t *stream_u8, int len_u8) {
    UNUSED(userdata);
    int16_t *stream = (int16_t *) stream_u8;
    int len = len_u8 / 2;

    uint16_t a_timer = BIT(ioreg.soundcnt_h.w, 10);
    uint16_t b_timer = BIT(ioreg.soundcnt_h.w, 14);
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
        if (BIT(ioreg.soundcnt_h.w, 8)) right = clamp_i16(right + a, -512, 511);
        if (BIT(ioreg.soundcnt_h.w, 9)) left = clamp_i16(left + a, -512, 511);
        if (BIT(ioreg.soundcnt_h.w, 12)) right = clamp_i16(right + b, -512, 511);
        if (BIT(ioreg.soundcnt_h.w, 13)) left = clamp_i16(left + b, -512, 511);
        stream[i] = left << 7;
        stream[i + 1] = right << 7;
    }
}

void audio_fifo_a(uint32_t sample) {
    *(uint32_t *) &ioreg.fifo_a[ioreg.fifo_a_w] = sample;
    ioreg.fifo_a_w = (ioreg.fifo_a_w + 4) % FIFO_SIZE;
}

void audio_fifo_b(uint32_t sample) {
    *(uint32_t *) &ioreg.fifo_b[ioreg.fifo_b_w] = sample;
    ioreg.fifo_b_w = (ioreg.fifo_b_w + 4) % FIFO_SIZE;
}

SDL_AudioDeviceID audio_init() {
    SDL_AudioSpec want;
    std::memset(&want, 0, sizeof(want));
    want.freq = 48000;
    want.format = AUDIO_S16;
    want.channels = 2;
    want.samples = FIFO_SIZE;
    want.callback = audio_callback;
    SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
    if (audio_device == 0) {
        SDL_Log("Failed to open audio device: %s", SDL_GetError());
        std::exit(EXIT_FAILURE);
    }
    SDL_PauseAudioDevice(audio_device, 0);
    return audio_device;
}
