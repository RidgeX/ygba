// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#include "backup.h"

#include <stdint.h>
#include <cassert>
#include <cstring>

#include <fmt/core.h>

#include "dma.h"

//#define LOG_BAD_MEMORY_ACCESS

bool has_eeprom;
bool has_flash;
bool has_sram;

uint8_t backup_eeprom[0x2000];
uint8_t backup_flash[0x20000];
uint8_t backup_sram[0x8000];

uint32_t eeprom_addr;
uint64_t eeprom_rbits;
uint32_t eeprom_num_rbits;
uint64_t eeprom_wbits;
uint32_t eeprom_num_wbits;
uint32_t eeprom_state;
uint32_t eeprom_width;

uint32_t flash_bank;
uint32_t flash_state;
bool flash_id;
uint8_t flash_manufacturer;
uint8_t flash_device;

void backup_erase() {
    std::memset(backup_eeprom, 0xff, sizeof(backup_eeprom));
    std::memset(backup_flash, 0xff, sizeof(backup_flash));
    std::memset(backup_sram, 0xff, sizeof(backup_sram));
}

void backup_init() {
    eeprom_addr = 0;
    eeprom_rbits = 0;
    eeprom_num_rbits = 0;
    eeprom_wbits = 0;
    eeprom_num_wbits = 0;
    eeprom_state = 0;
    eeprom_width = 0;

    flash_bank = 0;
    flash_state = 0;
    flash_id = false;
}

uint8_t backup_read_byte(uint32_t address) {
    if (has_flash) {
        flash_state &= ~7;
        if (flash_id) {
            if ((address & 3) == 0) return flash_manufacturer;
            if ((address & 3) == 1) return flash_device;
            return 0;
        }
        return backup_flash[flash_bank * 0x10000 + address];
    } else if (has_sram) {
        if (dma_active_ch == 0) return 0;
        return backup_sram[address & 0x7fff];
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    fmt::print("backup_read_byte(0x{:08x});\n", address);
#endif
    return 0xff;
}

void backup_write_byte(uint32_t address, uint8_t value) {
    if (has_flash) {
        switch (flash_state) {
            case 0:
            case 4:
            case 8:
                if (address == 0x5555 && value == 0xaa) {
                    flash_state++;
                    break;
                }
                if (value != 0xf0) assert(false);
                flash_state &= ~7;
                break;

            case 1:
            case 5:
            case 9:
                if (address == 0x2aaa && value == 0x55) {
                    flash_state++;
                    break;
                }
                assert(false);
                flash_state &= ~7;
                break;

            case 2:
            case 6:
            case 10:
                if ((flash_state & ~3) == 0) {  // Normal mode
                    if (address == 0x5555 && value == 0x80) {
                        flash_state = 4;
                        break;
                    }
                    if (address == 0x5555 && value == 0x90) {
                        flash_state = 8;
                        flash_id = true;
                        break;
                    }
                    if (address == 0x5555 && value == 0xa0) {
                        flash_state = 3;
                        break;
                    }
                    if (address == 0x5555 && value == 0xb0) {
                        flash_state = 7;
                        break;
                    }
                    assert(false);
                }
                if (flash_state & 4) {  // Erase mode
                    if (address == 0x5555 && value == 0x10) {
                        std::memset(backup_flash, 0xff, sizeof(backup_flash));  // Chip erase
                        break;
                    }
                    if (value == 0x30) {
                        uint32_t sector = address >> 12;
                        std::memset(&backup_flash[flash_bank * 0x10000 + sector * 0x1000], 0xff, 0x1000);  // Sector erase
                        break;
                    }
                    assert(false);
                }
                if (flash_state & 8) {  // Software ID mode
                    if (address == 0x5555 && value == 0xf0) {
                        flash_state = 0;
                        flash_id = false;
                        break;
                    }
                    assert(false);
                }
                assert(false);
                flash_state &= ~7;
                break;

            case 3:  // Byte program
                backup_flash[flash_bank * 0x10000 + address] = value;
                flash_state = 0;
                break;

            case 7:  // Bank switch
                assert(address == 0);
                assert(value == 0 || value == 1);
                flash_bank = value & 1;
                flash_state = 0;
                break;

            default:
                assert(false);
                break;
        }
        return;
    } else if (has_sram) {
        backup_sram[address & 0x7fff] = value;
        return;
    }
#ifdef LOG_BAD_MEMORY_ACCESS
    fmt::print("backup_write_byte(0x{:08x}, 0x{:02x});\n", address, value);
#endif
}

uint16_t backup_read_halfword(uint32_t address) {
    uint16_t value = backup_read_byte(address);
    return value | value << 8;
}

void backup_write_halfword(uint32_t address, uint16_t value) {
    backup_write_byte(address, (uint8_t) (value >> 8 * (address & 1)));
}

uint32_t backup_read_word(uint32_t address) {
    uint32_t value = backup_read_byte(address);
    return value | value << 8 | value << 16 | value << 24;
}

void backup_write_word(uint32_t address, uint32_t value) {
    backup_write_byte(address, (uint8_t) (value >> 8 * (address & 3)));
}

uint16_t eeprom_read_bit() {
    if (eeprom_num_rbits > 64) {
        eeprom_num_rbits--;
        return 1;
    }
    if (eeprom_num_rbits > 0) {
        eeprom_num_rbits--;
        return (eeprom_rbits >> eeprom_num_rbits) & 1;
    }
    return 1;
}

void eeprom_write_bit(uint16_t value) {
    assert(eeprom_width != 0);
    eeprom_wbits <<= 1;
    eeprom_wbits |= value & 1;
    eeprom_num_wbits++;
    switch (eeprom_state) {
        case 0:  // Start of stream
            if (eeprom_num_wbits < 2) break;
            eeprom_state = (uint32_t) eeprom_wbits;
            assert(eeprom_state == 2 || eeprom_state == 3);
            eeprom_wbits = 0;
            eeprom_num_wbits = 0;
            break;

        case 1:  // End of stream
            eeprom_state = 0;
            eeprom_wbits = 0;
            eeprom_num_wbits = 0;
            break;

        case 2:  // Write request
            if (eeprom_num_wbits < eeprom_width) break;
            eeprom_addr = (uint32_t) (eeprom_wbits * 8);
            eeprom_rbits = 0;
            eeprom_num_rbits = 0;
            eeprom_state = 4;
            eeprom_wbits = 0;
            eeprom_num_wbits = 0;
            break;

        case 3:  // Read request
            if (eeprom_num_wbits < eeprom_width) break;
            eeprom_addr = (uint32_t) (eeprom_wbits * 8);
            eeprom_rbits = 0;
            eeprom_num_rbits = 68;
            for (int i = 0; i < 8; i++) {
                uint8_t b = backup_eeprom[eeprom_addr + i];
                for (int j = 7; j >= 0; j--) {
                    eeprom_rbits <<= 1;
                    eeprom_rbits |= (b >> j) & 1;
                }
            }
            eeprom_state = 1;
            eeprom_wbits = 0;
            eeprom_num_wbits = 0;
            break;

        case 4:  // Data
            if (eeprom_num_wbits < 64) break;
            for (int i = 0; i < 8; i++) {
                uint8_t b = 0;
                for (int j = 7; j >= 0; j--) {
                    b <<= 1;
                    b |= (eeprom_wbits >> ((7 - i) * 8 + j)) & 1;
                }
                backup_eeprom[eeprom_addr + i] = b;
            }
            eeprom_state = 1;
            eeprom_wbits = 0;
            eeprom_num_wbits = 0;
            break;

        default:
            assert(false);
            break;
    }
}
