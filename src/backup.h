// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#ifndef BACKUP_H
#define BACKUP_H

#define MANUFACTURER_ATMEL     0x1f
#define DEVICE_AT29LV512       0x3d  // 512 Kbit

#define MANUFACTURER_PANASONIC 0x32
#define DEVICE_MN63F805MNP     0x1b  // 512 Kbit

#define MANUFACTURER_SANYO     0x62
#define DEVICE_LE26FV10N1TS    0x13  // 1 Mbit

#define MANUFACTURER_SST       0xbf
#define DEVICE_SST39VF512      0xd4  // 512 Kbit

#define MANUFACTURER_MACRONIX  0xc2
#define DEVICE_MX29L512        0x1c  // 512 Kbit
#define DEVICE_MX29L010        0x09  // 1 Mbit

extern bool has_eeprom;
extern bool has_flash;
extern bool has_sram;

extern uint8_t backup_eeprom[0x2000];
extern uint8_t backup_flash[0x20000];
extern uint8_t backup_sram[0x8000];

extern uint32_t eeprom_width;

extern uint8_t flash_manufacturer;
extern uint8_t flash_device;

void backup_erase(void);
void backup_init(void);
uint8_t backup_read_byte(uint32_t address);
void backup_write_byte(uint32_t address, uint8_t value);
uint16_t backup_read_halfword(uint32_t address);
void backup_write_halfword(uint32_t address, uint16_t value);
uint32_t backup_read_word(uint32_t address);
void backup_write_word(uint32_t address, uint32_t value);

uint16_t eeprom_read_bit(void);
void eeprom_write_bit(uint16_t value);

#endif  // BACKUP_H
