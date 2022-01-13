// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <stdint.h>

// Real-time clock (Seiko S-3511A)
#define RTC_SCK       1
#define RTC_SIO       2
#define RTC_CS        4

#define STATUS_INTFE  2     // Frequency interrupt enable
#define STATUS_INTME  8     // Per-minute interrupt enable
#define STATUS_INTAE  0x20  // Alarm interrupt enable
#define STATUS_24HOUR 0x40  // 0: 12-hour mode, 1: 24-hour mode
#define STATUS_POWER  0x80  // Power on or power failure occurred

#define TEST_MODE     0x80  // Flag in the "second" byte

#define ALARM_AM      0
#define ALARM_PM      0x80

extern bool has_rtc;

void gpio_init();
uint16_t gpio_read_halfword(uint32_t address);
void gpio_write_halfword(uint32_t address, uint16_t value);
