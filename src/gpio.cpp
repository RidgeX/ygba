// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <time.h>

#include "gpio.h"
#include "memory.h"

bool has_rtc = false;

uint16_t gpio_data = 0;
uint16_t gpio_direction = 0;
uint16_t gpio_read_enable = 0;

uint64_t rtc_rbits = 0;
uint32_t rtc_num_rbits = 0;
uint64_t rtc_wbits = 0;
uint32_t rtc_num_wbits = 0;
uint32_t rtc_state = 0;

void gpio_init(void) {
    gpio_data = 0;
    gpio_direction = 0;
    gpio_read_enable = 0;

    rtc_rbits = 0;
    rtc_num_rbits = 0;
    rtc_wbits = 0;
    rtc_num_wbits = 0;
    rtc_state = 0;
}

uint8_t bcd_to_decimal(uint8_t x) {
    uint8_t tens = x >> 4;
    uint8_t ones = x & 0xf;
    return (tens * 10) + ones;
}

uint8_t decimal_to_bcd(uint8_t x) {
    uint8_t tens = x / 10;
    uint8_t ones = x % 10;
    return (tens << 4) | ones;
}

void rtc_send(uint8_t value) {
    for (int i = 0; i < 8; i++) {
        rtc_rbits <<= 1;
        rtc_rbits |= (value >> i) & 1;
    }
    rtc_num_rbits += 8;
}

uint16_t rtc_read_bit(void) {
    if (rtc_num_rbits > 0) {
        rtc_num_rbits--;
        return (rtc_rbits >> rtc_num_rbits) & 1;
    }
    return 0;
}

void rtc_write_bit(uint16_t value) {
    time_t rawtime;
    struct tm *timeinfo;

    rtc_wbits <<= 1;
    rtc_wbits |= value & 1;
    rtc_num_wbits++;

    if (rtc_state == 0) {  // Command received
        if (rtc_num_wbits < 8) return;
        rtc_state = (uint8_t) rtc_wbits;
        rtc_rbits = 0;
        rtc_num_rbits = 0;
        rtc_wbits = 0;
        rtc_num_wbits = 0;

        switch (rtc_state) {
            case 0x60:  // Reset
            case 0x61:
                rtc_state = 0;
                break;

            case 0x62:  // Write status
                break;

            case 0x63:  // Read status
                rtc_send(STATUS_24HOUR);
                rtc_state = 0;
                break;

            case 0x64:  // Write date and time
                break;

            case 0x65:  // Read date and time
                time(&rawtime);
                timeinfo = localtime(&rawtime);
                rtc_send(decimal_to_bcd(timeinfo->tm_year % 100));
                rtc_send(decimal_to_bcd(timeinfo->tm_mon + 1));
                rtc_send(decimal_to_bcd(timeinfo->tm_mday));
                rtc_send(decimal_to_bcd(timeinfo->tm_wday));
                rtc_send(decimal_to_bcd(timeinfo->tm_hour));
                rtc_send(decimal_to_bcd(timeinfo->tm_min));
                rtc_send(decimal_to_bcd(timeinfo->tm_sec));
                rtc_state = 0;
                break;

            case 0x66:  // Write time
                break;

            case 0x67:  // Read time
                time(&rawtime);
                timeinfo = localtime(&rawtime);
                rtc_send(decimal_to_bcd(timeinfo->tm_hour));
                rtc_send(decimal_to_bcd(timeinfo->tm_min));
                rtc_send(decimal_to_bcd(timeinfo->tm_sec));
                rtc_state = 0;
                break;

            default:
                assert(false);
                break;
        }
    } else {  // Data received
        switch (rtc_state) {
            case 0x62:  // Write status
                if (rtc_num_wbits < 8) return;
                // Do nothing
                rtc_state = 0;
                rtc_wbits = 0;
                rtc_num_wbits = 0;
                break;

            case 0x64:  // Write date and time
                if (rtc_num_wbits < 56) return;
                // Do nothing
                rtc_state = 0;
                rtc_wbits = 0;
                rtc_num_wbits = 0;
                break;

            case 0x66:  // Write time
                if (rtc_num_wbits < 24) return;
                // Do nothing
                rtc_state = 0;
                rtc_wbits = 0;
                rtc_num_wbits = 0;
                break;

            default:
                assert(false);
                break;
        }
    }
}

uint16_t gpio_read_halfword(uint32_t address) {
    if (gpio_read_enable) {
        switch (address) {
            case 0xc4: return gpio_data;
            case 0xc6: return gpio_direction;
            case 0xc8: return gpio_read_enable;
        }
    }
    return rom_read_halfword(address);
}

void gpio_write_halfword(uint32_t address, uint16_t value) {
    uint16_t last_gpio_data;

    switch (address) {
        case 0xc4:
            last_gpio_data = gpio_data;
            gpio_data = value & 0xf;
            if (has_rtc && (gpio_data & RTC_CS) && (gpio_data & RTC_SCK) && !(last_gpio_data & RTC_SCK)) {
                if (gpio_direction & RTC_SIO) {
                    if (gpio_data & RTC_SIO) {
                        rtc_write_bit(1);
                    } else {
                        rtc_write_bit(0);
                    }
                } else {
                    if (rtc_read_bit()) {
                        gpio_data |= RTC_SIO;
                    } else {
                        gpio_data &= ~RTC_SIO;
                    }
                }
            }
            break;

        case 0xc6:
            gpio_direction = value & 0xf;
            break;

        case 0xc8:
            gpio_read_enable = value & 1;
            break;
    }
}
