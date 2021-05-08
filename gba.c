// gcc -std=c99 -Wall -Wextra -Wpedantic -O2 -o gba gba.c -lmingw32 -lSDL2main -lSDL2 && gba

#include <SDL2/SDL.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

//#define DEBUG
bool log_instructions = false;
bool log_arm_instructions = true;
bool log_thumb_instructions = true;
bool log_registers = false;
bool single_step = true;
uint64_t instruction_count = 0;
uint64_t start_logging_at = 1917;

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 160
#define SCREEN_SCALE 3
#define SCREEN_WIDTH (DISPLAY_WIDTH * SCREEN_SCALE)
#define SCREEN_HEIGHT (DISPLAY_HEIGHT * SCREEN_SCALE)
#define NUM_KEYS 10

bool keys[NUM_KEYS];

#define ARM_AND 0
#define ARM_EOR 1
#define ARM_SUB 2
#define ARM_RSB 3
#define ARM_ADD 4
#define ARM_ADC 5
#define ARM_SBC 6
#define ARM_RSC 7
#define ARM_TST 8
#define ARM_TEQ 9
#define ARM_CMP 0xa
#define ARM_CMN 0xb
#define ARM_ORR 0xc
#define ARM_MOV 0xd
#define ARM_BIC 0xe
#define ARM_MVN 0xf

#define COND_EQ 0
#define COND_NE 1
#define COND_CS 2
#define COND_CC 3
#define COND_MI 4
#define COND_PL 5
#define COND_VS 6
#define COND_VC 7
#define COND_HI 8
#define COND_LS 9
#define COND_GE 0xa
#define COND_LT 0xb
#define COND_GT 0xc
#define COND_LE 0xd
#define COND_AL 0xe
#define COND_NV 0xf

#define PC_RESET          0
#define PC_UNDEF          4
#define PC_SWI            8
#define PC_ABORT_PREFETCH 0xc
#define PC_ABORT_DATA     0x10
#define PC_IRQ            0x18
#define PC_FIQ            0x1c

#define PSR_N    (1 << 31)
#define PSR_Z    (1 << 30)
#define PSR_C    (1 << 29)
#define PSR_V    (1 << 28)
#define PSR_I    (1 << 7)
#define PSR_F    (1 << 6)
#define PSR_T    (1 << 5)
#define PSR_MODE 0x1f

#define PSR_MODE_USR 0x10
#define PSR_MODE_FIQ 0x11
#define PSR_MODE_IRQ 0x12
#define PSR_MODE_SVC 0x13
#define PSR_MODE_ABT 0x17
#define PSR_MODE_UND 0x1b
#define PSR_MODE_SYS 0x1f

#define REG_SP 0xd
#define REG_PC 0xf

#define SHIFT_LSL 0
#define SHIFT_LSR 1
#define SHIFT_ASR 2
#define SHIFT_ROR 3

#define THUMB_AND 0
#define THUMB_EOR 1
#define THUMB_LSL 2
#define THUMB_LSR 3
#define THUMB_ASR 4
#define THUMB_ADC 5
#define THUMB_SBC 6
#define THUMB_ROR 7
#define THUMB_TST 8
#define THUMB_NEG 9
#define THUMB_CMP 0xa
#define THUMB_CMN 0xb
#define THUMB_ORR 0xc
#define THUMB_MUL 0xd
#define THUMB_BIC 0xe
#define THUMB_MVN 0xf

uint8_t system_rom[0x4000];
uint8_t cpu_ewram[0x40000];
uint8_t cpu_iwram[0x8000];
uint8_t palette_ram[0x400];
uint8_t video_ram[0x18000];
uint8_t object_ram[0x400];
uint8_t rom[0x2000000];

uint32_t r[16];
uint32_t r8_usr, r9_usr, r10_usr, r11_usr, r12_usr, r13_usr, r14_usr;
uint32_t r8_fiq, r9_fiq, r10_fiq, r11_fiq, r12_fiq, r13_fiq, r14_fiq;
uint32_t r13_irq, r14_irq;
uint32_t r13_svc, r14_svc;
uint32_t r13_abt, r14_abt;
uint32_t r13_und, r14_und;
uint32_t cpsr;
uint32_t spsr_fiq, spsr_irq, spsr_svc, spsr_abt, spsr_und;
bool branch_taken;

uint32_t arm_op;
uint32_t arm_pipeline[2];
void (*arm_lookup[4096])(void);

uint16_t thumb_op;
uint16_t thumb_pipeline[2];
void (*thumb_lookup[256])(void);

#define IO_DISPCNT   0
#define IO_DISPSTAT  4
#define IO_VCOUNT    6
#define IO_BG0CNT    8
#define IO_BG1CNT    0xa
#define IO_BG0HOFS   0x10
#define IO_BG0VOFS   0x12
#define IO_WIN0H     0x40
#define IO_WIN1H     0x42
#define IO_WIN0V     0x44
#define IO_WIN1V     0x46
#define IO_WININ     0x48
#define IO_WINOUT    0x4a
#define IO_SOUNDBIAS 0x88
#define IO_KEYINPUT  0x130
#define IO_IE        0x200
#define IO_IME       0x208

uint16_t io_dispcnt;
uint16_t io_dispstat;
uint16_t io_vcount;
uint16_t io_bg0cnt;
uint16_t io_bg1cnt;
uint16_t io_bg2cnt;
uint16_t io_bg3cnt;
uint16_t io_bg0hofs;
uint16_t io_bg0vofs;
uint16_t io_win0h;
uint16_t io_win1h;
uint16_t io_win0v;
uint16_t io_win1v;
uint16_t io_winin;
uint16_t io_winout;
uint16_t io_soundbias;
uint16_t io_keyinput;
uint16_t io_ie;
uint16_t io_if;
uint16_t io_ime;

uint32_t asr(uint32_t x, uint32_t n) {
	return (x & (1 << 31)) ? ~(~x >> n) : x >> n;
}

uint32_t ror(uint32_t x, uint32_t n) {
	return (x >> n) | (x << (32 - n));
}

uint32_t align_word(uint32_t address, uint32_t value) {
	return ror(value, 8 * (address & 3));
}

uint32_t align_halfword(uint32_t address, uint16_t value) {
	return ror(value, 8 * (address & 1));
}

uint8_t io_read_byte(uint32_t address) {
	switch (address) {
		default:
			printf("io_read_byte(0x%08x);\n", address);
			return 0;
	}
}

void io_write_byte(uint32_t address, uint8_t value) {
	switch (address) {
		default:
			printf("io_write_byte(0x%08x, 0x%02x);\n", address, value);
			break;
	}
}

uint16_t io_read_halfword(uint32_t address) {
	switch (address) {
		case IO_DISPSTAT:
			io_dispstat = (io_dispstat + 1) % 8;
			return io_dispstat;

		case IO_VCOUNT:
			io_vcount = (io_vcount + 1) % 228;
			return io_vcount;

		case IO_SOUNDBIAS:
			return io_soundbias;

		case IO_KEYINPUT:
			io_keyinput = 0x3ff;
			for (int i = 0; i < NUM_KEYS; i++) {
				if (keys[i]) io_keyinput &= ~(1 << i);
			}
			return io_keyinput;

		case IO_IE:
			return io_ie;

		case IO_IME:
			return io_ime;

		default:
			printf("io_read_halfword(0x%08x);\n", address);
			return 0;
	}
}

void io_write_halfword(uint32_t address, uint16_t value) {
	switch (address) {
		case IO_DISPCNT:
			io_dispcnt = value;
			printf("DISPCNT = 0x%04x\n", io_dispcnt);
			break;

		case IO_DISPSTAT:
			io_dispstat = (io_dispstat & 7) | (value & ~7);
			printf("DISPSTAT = 0x%04x\n", io_dispstat);
			break;

		case IO_BG0CNT:
			io_bg0cnt = value;
			printf("BG0CNT = 0x%04x\n", io_bg0cnt);
			break;

		case IO_BG1CNT:
			io_bg1cnt = value;
			printf("BG1CNT = 0x%04x\n", io_bg1cnt);
			break;

		case IO_BG0HOFS:
			io_bg0hofs = value;
			printf("BG0HOFS = 0x%04x\n", io_bg0hofs);
			break;

		case IO_BG0VOFS:
			io_bg0vofs = value;
			printf("BG0VOFS = 0x%04x\n", io_bg0vofs);
			break;

		case IO_WIN0H:
			io_win0h = value;
			//printf("WIN0H = 0x%04x\n", io_win0h);
			break;

		case IO_WIN1H:
			io_win1h = value;
			//printf("WIN1H = 0x%04x\n", io_win1h);
			break;

		case IO_WIN0V:
			io_win0v = value;
			//printf("WIN0V = 0x%04x\n", io_win0v);
			break;

		case IO_WIN1V:
			io_win1v = value;
			//printf("WIN1V = 0x%04x\n", io_win1v);
			break;

		case IO_WININ:
			io_winin = value;
			printf("WININ = 0x%04x\n", io_winin);
			break;

		case IO_WINOUT:
			io_winout = value;
			printf("WINOUT = 0x%04x\n", io_winout);
			break;

		case IO_SOUNDBIAS:
			io_soundbias = value;
			//printf("SOUNDBIAS = 0x%04x\n", io_soundbias);
			break;

		case IO_IE:
			io_ie = value;
			printf("IE = 0x%04x\n", io_ie);
			break;

		case IO_IME:
			io_ime = value;
			printf("IME = 0x%04x\n", io_ime);
			break;

		default:
			printf("io_write_halfword(0x%08x, 0x%04x);\n", address, value);
			break;
	}
}

uint32_t io_read_word(uint32_t address) {
	switch (address) {
		case IO_DISPSTAT:
			if (io_vcount == 0 || io_vcount == 0xa0) {  // FIXME
				io_dispstat = 0;
				io_vcount = 0x9f;
			} else {
				io_dispstat = 1;
				io_vcount = 0xa0;
			}
			return io_dispstat | io_vcount << 16;

		case IO_IE:
			return io_ie | io_if << 16;

		default:
			printf("io_read_word(0x%08x);\n", address);
			return 0;
	}
}

void io_write_word(uint32_t address, uint32_t value) {
	switch (address) {
		case IO_DISPCNT:
			io_dispcnt = (uint16_t) value;
			printf("DISPCNT = 0x%04x\n", io_dispcnt);
			break;

		case IO_BG0CNT:
			io_bg0cnt = (uint16_t) value;
			io_bg1cnt = (uint16_t)(value >> 16);
			printf("BG0CNT = 0x%04x\n", io_bg0cnt);
			printf("BG1CNT = 0x%04x\n", io_bg1cnt);
			break;

		case IO_BG0HOFS:
			io_bg0hofs = (uint16_t) value;
			io_bg0vofs = (uint16_t)(value >> 16);
			//printf("BG0HOFS = 0x%04x\n", io_bg0hofs);
			//printf("BG0VOFS = 0x%04x\n", io_bg0vofs);
			break;

		case IO_IE:
			io_ie = (uint16_t) value;
			io_if = (uint16_t)(value >> 16);
			printf("IE = 0x%04x\n", io_ie);
			printf("IF = 0x%04x\n", io_if);
			break;

		case IO_IME:
			io_ime = (uint16_t) value;
			printf("IME = 0x%04x\n", io_ime);
			break;

		default:
			printf("io_write_word(0x%08x, 0x%08x);\n", address, value);
			break;
	}
}

uint8_t memory_read_byte(uint32_t address) {
	if (address < 0x4000) {
		assert(false);
		return system_rom[address];
	}
	if (address >= 0x02000000 && address < 0x02040000) {
		return cpu_ewram[address - 0x02000000];
	}
	if (address >= 0x03000000 && address < 0x03008000) {
		return cpu_iwram[address - 0x03000000];
	}
	if (address >= 0x04000000 && address < 0x05000000) {
		return io_read_byte(address - 0x04000000);
	}
	if (address >= 0x08000000 && address < 0x0a000000) {
		return rom[address - 0x08000000];
	}
	printf("memory_read_byte(0x%08x);\n", address);
	assert(false);
	return 0;
}

void memory_write_byte(uint32_t address, uint8_t value) {
	if (address >= 0x02000000 && address < 0x02040000) {
		cpu_ewram[address - 0x02000000] = value;
		return;
	}
	if (address >= 0x03000000 && address < 0x03008000) {
		cpu_iwram[address - 0x03000000] = value;
		return;
	}
	if (address >= 0x04000000 && address < 0x05000000) {
		io_write_byte(address - 0x04000000, value);
		return;
	}
	if (address >= 0x05000000 && address < 0x05000400) {
		*(uint16_t *)&palette_ram[address - 0x05000000] = value | value << 8;
		return;
	}
	if (address >= 0x06000000 && address < 0x06018000) {
		*(uint16_t *)&video_ram[address - 0x06000000] = value | value << 8;
		return;
	}
	printf("memory_write_byte(0x%08x, 0x%02x);\n", address, value);
	assert(false);
}

uint16_t memory_read_halfword(uint32_t address) {
	assert((address & 1) == 0);
	if (address < 0x4000) {
		return *(uint16_t *)&system_rom[address];
	}
	if (address >= 0x02000000 && address < 0x02040000) {
		return *(uint16_t *)&cpu_ewram[address - 0x02000000];
	}
	if (address >= 0x03000000 && address < 0x03008000) {
		return *(uint16_t *)&cpu_iwram[address - 0x03000000];
	}
	if (address >= 0x04000000 && address < 0x05000000) {
		return io_read_halfword(address - 0x04000000);
	}
	if (address >= 0x06000000 && address < 0x06018000) {
		return *(uint16_t *)&video_ram[address - 0x06000000];
	}
	if (address >= 0x08000000 && address < 0x0a000000) {
		return *(uint16_t *)&rom[address - 0x08000000];
	}
	printf("memory_read_halfword(0x%08x);\n", address);
	assert(false);
	return 0;
}

void memory_write_halfword(uint32_t address, uint16_t value) {
	if ((address & 1) != 0) {
		assert(false);
		return;
	}
	if (address >= 0x02000000 && address < 0x02040000) {
		*(uint16_t *)&cpu_ewram[address - 0x02000000] = value;
		return;
	}
	if (address >= 0x03000000 && address < 0x03008000) {
		*(uint16_t *)&cpu_iwram[address - 0x03000000] = value;
		return;
	}
	if (address >= 0x04000000 && address < 0x05000000) {
		io_write_halfword(address - 0x04000000, value);
		return;
	}
	if (address >= 0x05000000 && address < 0x05000400) {
		*(uint16_t *)&palette_ram[address - 0x05000000] = value;
		return;
	}
	if (address >= 0x06000000 && address < 0x06018000) {
		*(uint16_t *)&video_ram[address - 0x06000000] = value;
		return;
	}
	if (address >= 0x07000000 && address < 0x07000400) {
		*(uint16_t *)&object_ram[address - 0x07000000] = value;
		return;
	}
	printf("memory_write_halfword(0x%08x, 0x%04x);\n", address, value);
	assert(false);  // FIXME
}

uint32_t memory_read_word(uint32_t address) {
	address &= ~3;
	if (address < 0x4000) {
		return *(uint32_t *)&system_rom[address];
	}
	if (address >= 0x02000000 && address < 0x02040000) {
		return *(uint32_t *)&cpu_ewram[address - 0x02000000];
	}
	if (address >= 0x03000000 && address < 0x03008000) {
		return *(uint32_t *)&cpu_iwram[address - 0x03000000];
	}
	if (address >= 0x04000000 && address < 0x05000000) {
		return io_read_word(address - 0x04000000);
	}
	if (address >= 0x07000000 && address < 0x07000400) {
		return *(uint32_t *)&object_ram[address - 0x07000000];
	}
	if (address >= 0x08000000 && address < 0x0a000000) {
		return *(uint32_t *)&rom[address - 0x08000000];
	}
	printf("memory_read_word(0x%08x);\n", address);
	//assert(false);
	return 0;
}

void memory_write_word(uint32_t address, uint32_t value) {
	address &= ~3;
	if (address >= 0x02000000 && address < 0x02040000) {
		*(uint32_t *)&cpu_ewram[address - 0x02000000] = value;
		return;
	}
	if (address >= 0x03000000 && address < 0x03008000) {
		*(uint32_t *)&cpu_iwram[address - 0x03000000] = value;
		return;
	}
	if (address >= 0x04000000 && address < 0x05000000) {
		io_write_word(address - 0x04000000, value);
		return;
	}
	if (address >= 0x05000000 && address < 0x05000400) {
		*(uint32_t *)&palette_ram[address - 0x05000000] = value;
		return;
	}
	if (address >= 0x06000000 && address < 0x06018000) {
		*(uint32_t *)&video_ram[address - 0x06000000] = value;
		return;
	}
	if (address >= 0x07000000 && address < 0x07000400) {
		*(uint32_t *)&object_ram[address - 0x07000000] = value;
		return;
	}
	printf("memory_write_word(0x%08x, 0x%08x);\n", address, value);
	//assert(false);
}

uint32_t bit_count(uint32_t x) {
	return __builtin_popcount(x);
}

uint32_t lowest_set_bit(uint32_t x) {
	for (int i = 0; i < 32; i++) {
		if (x & (1 << i)) return i;
	}
	assert(false);
	return 0;
}

void mode_change(uint32_t old_mode, uint32_t new_mode) {
	if (old_mode == new_mode) return;
	if (old_mode == PSR_MODE_USR && new_mode == PSR_MODE_SYS) return;
	if (old_mode == PSR_MODE_SYS && new_mode == PSR_MODE_USR) return;

	if (old_mode == PSR_MODE_FIQ) {
		r8_fiq = r[8];
		r9_fiq = r[9];
		r10_fiq = r[10];
		r11_fiq = r[11];
		r12_fiq = r[12];
		r[8] = r8_usr;
		r[9] = r9_usr;
		r[10] = r10_usr;
		r[11] = r11_usr;
		r[12] = r12_usr;
	}
	if (new_mode == PSR_MODE_FIQ) {
		r8_usr = r[8];
		r9_usr = r[9];
		r10_usr = r[10];
		r11_usr = r[11];
		r12_usr = r[12];
		r[8] = r8_fiq;
		r[9] = r9_fiq;
		r[10] = r10_fiq;
		r[11] = r11_fiq;
		r[12] = r12_fiq;
	}
	switch (old_mode) {
		case PSR_MODE_USR:
		case PSR_MODE_SYS:
			r13_usr = r[13];
			r14_usr = r[14];
			break;
		case PSR_MODE_FIQ:
			r13_fiq = r[13];
			r14_fiq = r[14];
			break;
		case PSR_MODE_IRQ:
			r13_irq = r[13];
			r14_irq = r[14];
			break;
		case PSR_MODE_SVC:
			r13_svc = r[13];
			r14_svc = r[14];
			break;
		case PSR_MODE_ABT:
			r13_abt = r[13];
			r14_abt = r[14];
			break;
		case PSR_MODE_UND:
			r13_und = r[13];
			r14_und = r[14];
			break;
	}
	switch (new_mode) {
		case PSR_MODE_USR:
		case PSR_MODE_SYS:
			r[13] = r13_usr;
			r[14] = r14_usr;
			break;
		case PSR_MODE_FIQ:
			r[13] = r13_fiq;
			r[14] = r14_fiq;
			break;
		case PSR_MODE_IRQ:
			r[13] = r13_irq;
			r[14] = r14_irq;
			break;
		case PSR_MODE_SVC:
			r[13] = r13_svc;
			r[14] = r14_svc;
			break;
		case PSR_MODE_ABT:
			r[13] = r13_abt;
			r[14] = r14_abt;
			break;
		case PSR_MODE_UND:
			r[13] = r13_und;
			r[14] = r14_und;
			break;		
	}
}

bool cpsr_check_condition(uint32_t cond) {
	bool N = (cpsr & PSR_N) != 0;
	bool Z = (cpsr & PSR_Z) != 0;
	bool C = (cpsr & PSR_C) != 0;
	bool V = (cpsr & PSR_V) != 0;

	switch (cond) {
		case COND_EQ: return Z;
		case COND_NE: return !Z;
		case COND_CS: return C;
		case COND_CC: return !C;
		case COND_MI: return N;
		case COND_PL: return !N;
		case COND_VS: return V;
		case COND_VC: return !V;
		case COND_HI: return C && !Z;
		case COND_LS: return !C || Z;
		case COND_GE: return N == V;
		case COND_LT: return N != V;
		case COND_GT: return (N == V) && !Z;
		case COND_LE: return (N != V) || Z;
		case COND_AL: return true;
		case COND_NV: break;
		default: break;
	}

	assert(false);
	return false;
}

void write_cpsr(uint32_t psr) {
	uint32_t old_mode = cpsr & PSR_MODE;
	cpsr = psr;
	uint32_t new_mode = cpsr & PSR_MODE;
	mode_change(old_mode, new_mode);
}

uint32_t read_spsr() {
	uint32_t mode = cpsr & PSR_MODE;
	switch (mode) {
		case PSR_MODE_USR: assert(false); break;
		case PSR_MODE_FIQ: return spsr_fiq; break;
		case PSR_MODE_IRQ: return spsr_irq; break;
		case PSR_MODE_SVC: return spsr_svc; break;
		case PSR_MODE_ABT: return spsr_abt; break;
		case PSR_MODE_UND: return spsr_und; break;
		case PSR_MODE_SYS: assert(false); break;
		default: assert(false); break;
	}
	assert(false);
	return 0;
}

void write_spsr(uint32_t psr) {
	uint32_t mode = cpsr & PSR_MODE;
	switch (mode) {
		case PSR_MODE_USR: assert(false); break;
		case PSR_MODE_FIQ: spsr_fiq = psr; break;
		case PSR_MODE_IRQ: spsr_irq = psr; break;
		case PSR_MODE_SVC: spsr_svc = psr; break;
		case PSR_MODE_ABT: spsr_abt = psr; break;
		case PSR_MODE_UND: spsr_und = psr; break;
		case PSR_MODE_SYS: assert(false); break;
		default: assert(false); break;
	}
}

void print_psr(uint32_t psr) {
	putchar(psr & PSR_N ? 'N' : '-');
	putchar(psr & PSR_Z ? 'Z' : '-');
	putchar(psr & PSR_C ? 'C' : '-');
	putchar(psr & PSR_V ? 'V' : '-');
	putchar(psr & PSR_I ? 'I' : '-');
	putchar(psr & PSR_F ? 'F' : '-');
	putchar(psr & PSR_T ? 'T' : '-');
}

void print_registers(void) {
	bool T = (cpsr & PSR_T) != 0;
	printf(" r0: %08X   r1: %08X   r2: %08X   r3: %08X\n", r[0], r[1], r[2], r[3]);
	printf(" r4: %08X   r5: %08X   r6: %08X   r7: %08X\n", r[4], r[5], r[6], r[7]);
	printf(" r8: %08X   r9: %08X  r10: %08X  r11: %08X\n", r[8], r[9], r[10], r[11]);
	printf("r12: %08X  r13: %08X  r14: %08X  r15: %08X\n", r[12], r[13], r[14], r[15] - (T ? 2 : 4));
	printf("cpsr: %08X [", cpsr);
	print_psr(cpsr);
	printf("]\n");
	printf("Cycle: %lld\n", instruction_count);
	/*
	uint32_t mode = cpsr & PSR_MODE;
	switch (mode) {
		case PSR_MODE_USR: printf("User32"); break;
		case PSR_MODE_FIQ: printf("FIQ32"); break;
		case PSR_MODE_IRQ: printf("IRQ32"); break;
		case PSR_MODE_SVC: printf("SVC32"); break;
		case PSR_MODE_ABT: printf("Abort32"); break;
		case PSR_MODE_UND: printf("Undef32"); break;
		case PSR_MODE_SYS: printf("System32"); break;
		default: printf("Ill_%02x", mode); break;
	}
	if (mode == PSR_MODE_USR || mode == PSR_MODE_SYS) {
		printf("\n\n");
		return;
	}
	printf("  spsr = ");
	print_psr(read_spsr());
	printf("\n\n");
	*/
}

void arm_print_opcode(void) {
	bool T = (cpsr & PSR_T) != 0;
	printf("%08X:  %08X\t", r[15] - (T ? 4 : 8), arm_op);
}

void thumb_print_opcode(void) {
	printf("%08X:  %04X    \t", r[15] - 4, thumb_op);
}

void arm_print_mnemonic(char *s) {
	printf("%s ", s);
}

void thumb_print_mnemonic(char *s) {
	printf("%s ", s);
}

void arm_print_register(uint32_t i) {
	printf("r%d", i);
}

void thumb_print_register(uint32_t i) {
	switch (i) {
		case 13: printf("sp"); break;
		case 14: printf("lr"); break;
		case 15: printf("pc"); break;
		default: printf("r%d", i); break;
	}
}

void arm_print_immediate(uint32_t i) {
	if (i > 9) {
		printf("#0x%X", i);
	} else {
		printf("#%d", i);
	}
}

void thumb_print_immediate(uint32_t i) {
	if (i > 9) {
		printf("#0x%X", i);
	} else {
		printf("#%d", i);
	}
}

void arm_print_address(uint32_t i) {
	printf("0x%08X", i);
}

void thumb_print_address(uint32_t i) {
	printf("0x%08X", i);
}

void print_shift_amount(uint32_t i) {
	printf("#%d", i);
}

void print_shift_op(uint32_t shop, uint32_t shamt, uint32_t shreg, uint32_t Rs) {
	switch (shop) {
		case SHIFT_LSL:
			if (shreg) {
				printf(",lsl ");
				arm_print_register(Rs);
			} else if (shamt != 0) {
				printf(",lsl ");
				print_shift_amount(shamt);
			}
			break;

		case SHIFT_LSR:
			printf(",lsr ");
			if (shreg) {
				arm_print_register(Rs);
			} else {
				print_shift_amount(shamt);
			}
			break;

		case SHIFT_ASR:
			printf(",asr ");
			if (shreg) {
				arm_print_register(Rs);
			} else {
				print_shift_amount(shamt);
			}
			break;

		case SHIFT_ROR:
			if (shreg) {
				printf(",ror ");
				arm_print_register(Rs);
			} else if (shamt == 0) {
				printf(",rrx");
			} else {
				printf(",ror ");
				print_shift_amount(shamt);
			}
			break;

		default:
			assert(false);
			break;
	}
}

uint32_t handle_shift_op(uint32_t m, uint32_t s, uint32_t shop, uint32_t shamt, bool shreg) {
	switch (shop) {
		case SHIFT_LSL:
			if (s >= 32) {
				m = 0;
			} else {
				m <<= s;
			}
			break;

		case SHIFT_LSR:
			if (s >= 32) {
				m = 0;
			} else {
				m >>= s;
			}
			break;

		case SHIFT_ASR:
			if (s >= 32) {
				m = (m & (1 << 31)) ? ~0 : 0;
			} else {
				m = asr(m, s);
			}
			break;

		case SHIFT_ROR:
			if (!shreg && shamt == 0) {
				assert(false);
			} else {
				m = ror(m, s & 0x1f);
			}
			break;

		default:
			assert(false);
			break;
	}

	return m;
}

void arm_data_processing_register(void) {
	uint32_t alu = (arm_op >> 21) & 0xf;
	bool S = (arm_op & (1 << 20)) != 0;
	uint32_t Rn = (arm_op >> 16) & 0xf;
	uint32_t Rd = (arm_op >> 12) & 0xf;
	uint32_t Rs = (arm_op >> 8) & 0xf;
	uint32_t shamt = (arm_op >> 7) & 0x1f;
	uint32_t shop = (arm_op >> 5) & 3;
	bool shreg = (arm_op & (1 << 4)) != 0;
	uint32_t Rm = arm_op & 0xf;

	if ((shop == SHIFT_LSR || shop == SHIFT_ASR) && shamt == 0) shamt = 32;

	if (alu == ARM_TST || alu == ARM_TEQ || alu == ARM_CMP || alu == ARM_CMN) {
		assert(S == 1);
		assert(Rd == 0);
	} else if (alu == ARM_MOV || alu == ARM_MVN) {
		assert(Rn == 0);
	}

#ifdef DEBUG
	if (log_instructions && log_arm_instructions) {
		arm_print_opcode();
		switch (alu) {
			case ARM_AND: arm_print_mnemonic(S ? "ands" : "and"); break;
			case ARM_EOR: arm_print_mnemonic(S ? "eors" : "eor"); break;
			case ARM_SUB: arm_print_mnemonic(S ? "subs" : "sub"); break;
			case ARM_RSB: arm_print_mnemonic(S ? "rsbs" : "rsb"); break;
			case ARM_ADD: arm_print_mnemonic(S ? "adds" : "add"); break;
			case ARM_ADC: arm_print_mnemonic(S ? "adcs" : "adc"); break;
			case ARM_SBC: arm_print_mnemonic(S ? "sbcs" : "sbc"); break;
			case ARM_RSC: arm_print_mnemonic(S ? "rscs" : "rsc"); break;
			case ARM_TST: arm_print_mnemonic("tst"); break;
			case ARM_TEQ: arm_print_mnemonic("teq"); break;
			case ARM_CMP: arm_print_mnemonic("cmp"); break;
			case ARM_CMN: arm_print_mnemonic("cmn"); break;
			case ARM_ORR: arm_print_mnemonic(S ? "orrs" : "orr"); break;
			case ARM_MOV: arm_print_mnemonic(S ? "movs" : "mov"); break;
			case ARM_BIC: arm_print_mnemonic(S ? "bics" : "bic"); break;
			case ARM_MVN: arm_print_mnemonic(S ? "mvns" : "mvn"); break;
			default: assert(false); break;
		}
		switch (alu) {
			case ARM_AND:
			case ARM_EOR:
			case ARM_SUB:
			case ARM_RSB:
			case ARM_ADD:
			case ARM_ADC:
			case ARM_SBC:
			case ARM_RSC:
			case ARM_ORR:
			case ARM_BIC:
				arm_print_register(Rd);
				printf(",");
				arm_print_register(Rn);
				printf(",");
				arm_print_register(Rm);
				break;

			case ARM_TST:
			case ARM_TEQ:
			case ARM_CMP:
			case ARM_CMN:
				arm_print_register(Rn);
				printf(",");
				arm_print_register(Rm);
				break;

			case ARM_MOV:
			case ARM_MVN:
				arm_print_register(Rd);
				printf(",");
				arm_print_register(Rm);
				break;

			default:
				assert(false);
				break;
		}
		print_shift_op(shop, shamt, shreg, Rs);
		printf("\n");
	}
#endif

	uint32_t s = (shreg ? r[Rs] : shamt);
	uint32_t m = handle_shift_op(r[Rm], s, shop, shamt, shreg);
	uint64_t n = r[Rn];
	uint64_t result = 0;
	switch (alu) {
		case ARM_AND: result = n & m; break;
		case ARM_EOR: result = n ^ m; break;
		case ARM_SUB: result = n - m; break;
		case ARM_RSB: result = m - n; break;
		case ARM_ADD: result = n + m; break;
		case ARM_ADC: result = n + m + (cpsr & PSR_C ? 1 : 0); break;
		case ARM_SBC: result = n - m - (cpsr & PSR_C ? 0 : 1); break;
		case ARM_RSC: assert(false); break;  // FIXME
		case ARM_TST: result = n & m; break;
		case ARM_TEQ: result = n ^ m; break;
		case ARM_CMP: result = n - m; break;
		case ARM_CMN: result = n + m; break;
		case ARM_ORR: result = n | m; break;
		case ARM_MOV: result = m; break;
		case ARM_BIC: result = n & ~m; break;
		case ARM_MVN: result = ~m; break;
		default: assert(false); break;
	}
	if (S) {  // flags
		switch (shop) {
			case SHIFT_LSL:
				if (s == 0) {
					// Not altered
				} else if (s >= 1 && s <= 32) {
					if ((r[Rm] & (1 << (32 - s))) != 0) { cpsr |= PSR_C; } else { cpsr &= ~PSR_C; }
				} else {
					cpsr &= ~PSR_C;
				}
				break;

			case SHIFT_LSR:
				if (s == 0) {
					// Not altered
				} else if (s >= 1 && s <= 32) {
					if ((r[Rm] & (1 << (s - 1))) != 0) { cpsr |= PSR_C; } else { cpsr &= ~PSR_C; }
				} else {
					cpsr &= ~PSR_C;
				}
				break;

			case SHIFT_ASR:
				if (s == 0) {
					// Not altered
				} else if (s >= 1 && s <= 32) {
					if ((r[Rm] & (1 << (s - 1))) != 0) { cpsr |= PSR_C; } else { cpsr &= ~PSR_C; }
				} else {
					if ((r[Rm] & (1 << 31)) != 0) { cpsr |= PSR_C; } else { cpsr &= ~PSR_C; }
				}
				break;

			case SHIFT_ROR:
				if (s != 0) {
					if ((r[Rm] & (1 << (s - 1))) != 0) { cpsr |= PSR_C; } else { cpsr &= ~PSR_C; }
				}
				break;

			default:
				assert(false);
				break;
		}
		if ((result & 0x80000000) != 0) { cpsr |= PSR_N; } else { cpsr &= ~PSR_N; }
		if ((result & 0xffffffff) == 0) { cpsr |= PSR_Z; } else { cpsr &= ~PSR_Z; }
		if (alu == ARM_SUB || alu == ARM_SBC || alu == ARM_CMP) {
			if ((result & 0xffffffff00000000LL) != 0) { cpsr &= ~PSR_C; } else { cpsr |= PSR_C; }
		} else if (alu == ARM_ADD || alu == ARM_ADC || alu == ARM_CMN) {
			if ((result & 0xffffffff00000000LL) != 0) { cpsr |= PSR_C; } else { cpsr &= ~PSR_C; }
		}
		if (alu == ARM_SUB || alu == ARM_SBC || alu == ARM_CMP) {
			if (n >= 0x80000000 && (result > 0 && result < 0x80000000)) { cpsr |= PSR_V; } else { cpsr &= ~PSR_V; }
		} else if (alu == ARM_ADD || alu == ARM_ADC || alu == ARM_CMN) {
			if (n < 0x80000000 && result >= 0x80000000) { cpsr |= PSR_V; } else { cpsr &= ~PSR_V; }
		}
	}
	if (alu != ARM_TST && alu != ARM_TEQ && alu != ARM_CMP && alu != ARM_CMN) {
		r[Rd] = (uint32_t) result;
		if (Rd == 15) {
			r[Rd] &= ~1;
			if (S) write_cpsr(read_spsr());
			branch_taken = true;
		}
	}
}

void arm_data_processing_immediate(void) {
	uint32_t alu = (arm_op >> 21) & 0xf;
	bool S = (arm_op & (1 << 20)) != 0;
	uint32_t Rn = (arm_op >> 16) & 0xf;
	uint32_t Rd = (arm_op >> 12) & 0xf;
	uint32_t rot = (arm_op >> 8) & 0xf;
	uint32_t imm_unrotated = arm_op & 0xff;
	uint64_t imm = ror(imm_unrotated, 2 * rot);

	if (alu == ARM_TST || alu == ARM_TEQ || alu == ARM_CMP || alu == ARM_CMN) {
		assert(S == 1);
		assert(Rd == 0);
	} else if (alu == ARM_MOV || alu == ARM_MVN) {
		assert(Rn == 0);
	}

#ifdef DEBUG
	if (log_instructions && log_arm_instructions) {
		arm_print_opcode();
		switch (alu) {
			case ARM_AND: arm_print_mnemonic(S ? "ands" : "and"); break;
			case ARM_EOR: arm_print_mnemonic(S ? "eors" : "eor"); break;
			case ARM_SUB: arm_print_mnemonic(S ? "subs" : "sub"); break;
			case ARM_RSB: arm_print_mnemonic(S ? "rsbs" : "rsb"); break;
			case ARM_ADD: arm_print_mnemonic(S ? "adds" : "add"); break;
			case ARM_ADC: arm_print_mnemonic(S ? "adcs" : "adc"); break;
			case ARM_SBC: arm_print_mnemonic(S ? "sbcs" : "sbc"); break;
			case ARM_RSC: arm_print_mnemonic(S ? "rscs" : "rsc"); break;
			case ARM_TST: arm_print_mnemonic("tst"); break;
			case ARM_TEQ: arm_print_mnemonic("teq"); break;
			case ARM_CMP: arm_print_mnemonic("cmp"); break;
			case ARM_CMN: arm_print_mnemonic("cmn"); break;
			case ARM_ORR: arm_print_mnemonic(S ? "orrs" : "orr"); break;
			case ARM_MOV: arm_print_mnemonic(S ? "movs" : "mov"); break;
			case ARM_BIC: arm_print_mnemonic(S ? "bics" : "bic"); break;
			case ARM_MVN: arm_print_mnemonic(S ? "mvns" : "mvn"); break;
			default: assert(false); break;
		}
		switch (alu) {
			case ARM_AND:
			case ARM_EOR:
			case ARM_SUB:
			case ARM_RSB:
			case ARM_ADD:
			case ARM_ADC:
			case ARM_SBC:
			case ARM_RSC:
			case ARM_ORR:
			case ARM_BIC:
				arm_print_register(Rd);
				printf(",");
				arm_print_register(Rn);
				printf(",");
				arm_print_immediate(imm);
				printf("\n");
				break;

			case ARM_TST:
			case ARM_TEQ:
			case ARM_CMP:
			case ARM_CMN:
				arm_print_register(Rn);
				printf(",");
				arm_print_immediate(imm);
				printf("\n");
				break;

			case ARM_MOV:
			case ARM_MVN:
				arm_print_register(Rd);
				printf(",");
				arm_print_immediate(imm);
				printf("\n");
				break;

			default:
				assert(false);
				break;
		}
	}
#endif

	uint64_t n = r[Rn];
	if (Rn == 15) {
		n &= ~3;
	}
	uint64_t result = 0;
	switch (alu) {
		case ARM_AND: result = n & imm; break;
		case ARM_EOR: result = n ^ imm; break;
		case ARM_SUB: result = n - imm; break;
		case ARM_RSB: result = imm - n; break;
		case ARM_ADD: result = n + imm; break;
		case ARM_ADC: result = n + imm + (cpsr & PSR_C ? 1 : 0); break;
		case ARM_SBC: result = n - imm - (cpsr & PSR_C ? 0 : 1); break;
		case ARM_RSC: assert(false); break;  // FIXME
		case ARM_TST: result = n & imm; break;
		case ARM_TEQ: result = n ^ imm; break;
		case ARM_CMP: result = n - imm; break;
		case ARM_CMN: result = n + imm; break;
		case ARM_ORR: result = n | imm; break;
		case ARM_MOV: result = imm; break;
		case ARM_BIC: result = n & ~imm; break;
		case ARM_MVN: result = ~imm; break;
		default: assert(false); break;
	}
	if (S) {  // flags
		if (rot != 0) {  // FIXME?
			if ((imm_unrotated & (1 << (2 * rot - 1))) != 0) { cpsr |= PSR_C; } else { cpsr &= ~PSR_C; }
		}
		if ((result & 0x80000000) != 0) { cpsr |= PSR_N; } else { cpsr &= ~PSR_N; }
		if ((result & 0xffffffff) == 0) { cpsr |= PSR_Z; } else { cpsr &= ~PSR_Z; }
		if (alu == ARM_SUB || alu == ARM_SBC || alu == ARM_CMP) {
			if ((result & 0xffffffff00000000LL) != 0) { cpsr &= ~PSR_C; } else { cpsr |= PSR_C; }
		} else if (alu == ARM_ADD || alu == ARM_ADC || alu == ARM_CMN) {
			if ((result & 0xffffffff00000000LL) != 0) { cpsr |= PSR_C; } else { cpsr &= ~PSR_C; }
		}
		if (alu == ARM_SUB || alu == ARM_SBC || alu == ARM_CMP) {
			if (n >= 0x80000000 && (result > 0 && result < 0x80000000)) { cpsr |= PSR_V; } else { cpsr &= ~PSR_V; }
		} else if (alu == ARM_ADD || alu == ARM_ADC || alu == ARM_CMN) {
			if (n < 0x80000000 && result >= 0x80000000) { cpsr |= PSR_V; } else { cpsr &= ~PSR_V; }
		}
	}
	if (alu != ARM_TST && alu != ARM_TEQ && alu != ARM_CMP && alu != ARM_CMN) {
		r[Rd] = (uint32_t) result;
		if (Rd == 15) {
			r[Rd] &= ~1;
			branch_taken = true;
		}
	}
}

void arm_single_data_transfer_register(void) {
	bool P = (arm_op & (1 << 24)) != 0;
	bool U = (arm_op & (1 << 23)) != 0;
	bool B = (arm_op & (1 << 22)) != 0;
	bool W = (arm_op & (1 << 21)) != 0;
	bool L = (arm_op & (1 << 20)) != 0;
	uint32_t Rn = (arm_op >> 16) & 0xf;
	uint32_t Rd = (arm_op >> 12) & 0xf;
	uint32_t shamt = (arm_op >> 7) & 0x1f;
	uint32_t shop = (arm_op >> 4) & 3;
	uint32_t Rm = arm_op & 0xf;

#ifdef DEBUG
	if (log_instructions && log_arm_instructions) {
		arm_print_opcode();
		if (L) {
			arm_print_mnemonic(B ? "ldrb" : "ldr");
		} else {
			arm_print_mnemonic(B ? "strb" : "str");
		}
		arm_print_register(Rd);
		printf(",[");
		arm_print_register(Rn);
		printf(",");
		arm_print_register(Rm);
		print_shift_op(shop, shamt, false, 0);
		printf("]\n");
	}
#endif

	assert((arm_op & (1 << 4)) == 0);

	assert(P);  // FIXME
	assert(U);  // FIXME
	assert(!W);  // FIXME

	uint32_t s = shamt;
	uint32_t m = handle_shift_op(r[Rm], s, shop, shamt, false);
	uint32_t n = r[Rn];
	if (Rn == 15) n &= ~3;
	if (L) {
		if (B) {
			r[Rd] = memory_read_byte(n + m);
		} else {
			r[Rd] = align_word(n + m, memory_read_word(n + m));
		}
	} else {
		if (B) {
			memory_write_byte(n + m, (uint8_t) r[Rd]);
		} else {
			memory_write_word(n + m, r[Rd]);
		}
	}
}

void arm_single_data_transfer_immediate(void) {
	bool P = (arm_op & (1 << 24)) != 0;
	bool U = (arm_op & (1 << 23)) != 0;
	bool B = (arm_op & (1 << 22)) != 0;
	bool W = (arm_op & (1 << 21)) != 0;
	bool L = (arm_op & (1 << 20)) != 0;
	uint32_t Rn = (arm_op >> 16) & 0xf;
	uint32_t Rd = (arm_op >> 12) & 0xf;
	uint32_t imm = arm_op & 0xfff;

#ifdef DEBUG
	if (log_instructions && log_arm_instructions) {
		arm_print_opcode();
		if (L) {
			arm_print_mnemonic(B ? "ldrb" : "ldr");
		} else {
			arm_print_mnemonic(B ? "strb" : "str");
		}
		arm_print_register(Rd);
		printf(",[");
		arm_print_register(Rn);
		printf(P ? "," : "],");
		if (!U) printf("-");
		arm_print_immediate(imm);
		printf(P ? "]\n" : "\n");
	}
#endif

	assert(!W);  // FIXME

	uint32_t n = r[Rn];
	if (Rn == 15) n &= ~3;
	if (P) n += (U ? imm : -imm);
	if (L) {
		if (B) {
			r[Rd] = memory_read_byte(n);
		} else {
			r[Rd] = align_word(n, memory_read_word(n));
		}
	} else {
		if (B) {
			memory_write_byte(n, (uint8_t) r[Rd]);
		} else {
			memory_write_word(n, r[Rd]);
		}
	}
	if (!P) r[Rn] = n;  // FIXME?
}

void arm_block_data_transfer(void) {
	bool P = (arm_op & (1 << 24)) != 0;
	bool U = (arm_op & (1 << 23)) != 0;
	bool S = (arm_op & (1 << 22)) != 0;
	bool W = (arm_op & (1 << 21)) != 0;
	bool L = (arm_op & (1 << 20)) != 0;
	uint32_t Rn = (arm_op >> 16) & 0xf;
	uint32_t rlist = arm_op & 0xffff;

#ifdef DEBUG
	if (log_instructions && log_arm_instructions) {
		arm_print_opcode();
		if (L) {
			if (!P && !U) {
				arm_print_mnemonic("ldmda");
			} else if (!P && U) {
				arm_print_mnemonic("ldmia");
			} else if (P && !U) {
				arm_print_mnemonic("ldmdb");
			} else if (P && U) {
				arm_print_mnemonic("ldmib");
			}
		} else {
			if (!P && !U) {
				arm_print_mnemonic("stmda");
			} else if (!P && U) {
				arm_print_mnemonic("stmia");
			} else if (P && !U) {
				arm_print_mnemonic("stmdb");
			} else if (P && U) {
				arm_print_mnemonic("stmib");
			}
		}
		arm_print_register(Rn);
		if (W) printf("!");
		printf(",{");
		bool first = true;
		int i = 0;
		while (i < 16) {
			if (rlist & (1 << i)) {
				int j = i + 1;
				while (rlist & (1 << j)) j++;
				if (j == i + 1) {
					if (!first) printf(",");
					arm_print_register(i);
				} else if (j == i + 2) {
					if (!first) printf(",");
					arm_print_register(i);
					printf(",");
					arm_print_register(j - 1);
				} else {
					if (!first) printf(",");
					arm_print_register(i);
					printf("-");
					arm_print_register(j - 1);
				}
				i = j;
				first = false;
			}
			i++;
		}
		printf("}\n");
	}
#endif

	assert(!S);

	uint32_t count = bit_count(rlist);
	if (rlist == 0) {
		rlist |= 1 << 15;
		count = 16;
	}
	uint32_t address = r[Rn];
	if (!U) address -= 4 * count;
	if (U == P) address += 4;
	for (int i = 0; i < 16; i++) {
		if (rlist & (1 << i)) {
			if (L) {
				r[i] = memory_read_word(address);
				if (i == 15) {
					r[i] &= ~1;
					branch_taken = true;
				}
			} else {
				//if ((uint32_t) i == Rn && W) assert((uint32_t) i == lowest_set_bit(rlist));
				if (i == 15) {
					memory_write_word(address, r[i] + 2);
				} else {
					memory_write_word(address, r[i]);
				}
			}
			address += 4;
		}
	}
	if (W) {
		if (L) assert((rlist & (1 << Rn)) == 0);
		r[Rn] += (U ? 4 : -4) * count;
	}
}

void arm_branch(void) {
	bool L = (arm_op & (1 << 24)) != 0;
	uint32_t imm = arm_op & 0xffffff;
	if (arm_op & 0x800000) imm |= ~0xffffff;

#ifdef DEBUG
	if (log_instructions && log_arm_instructions) {
		arm_print_opcode();
		arm_print_mnemonic(L ? "bl" : "b");
		arm_print_address(r[15] + (imm << 2));
		printf("\n");
	}
#endif

	if (L) r[14] = r[15] - 4;
	r[15] += imm << 2;
	branch_taken = true;
}

void arm_software_interrupt(void) {
#ifdef DEBUG
	if (log_instructions && log_arm_instructions) {
		arm_print_opcode();
		arm_print_mnemonic("swi");
		arm_print_address(arm_op & 0xffffff);
		printf("\n");
	}
#endif

	r14_svc = r[15] - 4;
	spsr_svc = cpsr;
	write_cpsr((cpsr & ~PSR_MODE) | PSR_I | PSR_MODE_SVC);
	r[15] = PC_SWI;
	branch_taken = true;
}

void arm_multiply(void) {
	bool A = (arm_op & (1 << 21)) != 0;
	bool S = (arm_op & (1 << 20)) != 0;
	uint32_t Rd = (arm_op >> 16) & 0xf;
	uint32_t Rn = (arm_op >> 12) & 0xf;
	uint32_t Rs = (arm_op >> 8) & 0xf;
	uint32_t Rm = arm_op & 0xf;

	if (!A) assert(Rn == 0);
	assert(Rd != 15 && Rm != 15 && Rs != 15);

#ifdef DEBUG
	if (log_instructions && log_arm_instructions) {
		arm_print_opcode();
		if (A) {
			arm_print_mnemonic(S ? "mlas" : "mla");
		} else {
			arm_print_mnemonic(S ? "muls" : "mul");
		}
		arm_print_register(Rd);
		printf(",");
		arm_print_register(Rm);
		printf(",");
		arm_print_register(Rs);
		if (A) {
			printf(",");
			arm_print_register(Rn);
		}
		printf("\n");
	}
#endif

	if (A) {  // don't simplify this, must read all inputs before writing output
		r[Rd] = r[Rm] * r[Rs] + r[Rn];
	} else {
		r[Rd] = r[Rm] * r[Rs];
	}

	if (S) {
		if ((r[Rd] & 0x80000000) != 0) { cpsr |= PSR_N; } else { cpsr &= ~PSR_N; }
		if (r[Rd] == 0) { cpsr |= PSR_Z; } else { cpsr &= ~PSR_Z; }
	}
}

void arm_load_store_halfword_register(void) {
	bool P = (arm_op & (1 << 24)) != 0;
	bool U = (arm_op & (1 << 23)) != 0;
	bool I = (arm_op & (1 << 22)) != 0;
	bool W = (arm_op & (1 << 21)) != 0;
	bool L = (arm_op & (1 << 20)) != 0;
	uint32_t Rn = (arm_op >> 16) & 0xf;
	uint32_t Rd = (arm_op >> 12) & 0xf;
	uint32_t sbz = (arm_op >> 8) & 0xf;
	uint32_t opc = (arm_op >> 4) & 0xf;
	uint32_t Rm = arm_op & 0xf;

	assert(!I);
	assert(sbz == 0);
	assert(opc == 0xb);

#ifdef DEBUG
	if (log_instructions && log_arm_instructions) {
		arm_print_opcode();
		arm_print_mnemonic(L ? "ldrh" : "strh");
		arm_print_register(Rd);
		printf(",[");
		arm_print_register(Rn);
		if (P) {
			printf(",");
			arm_print_register(Rm);
			printf("]\n");
		} else {
			printf("],");
			arm_print_register(Rm);
			printf("\n");
		}
	}
#endif

	assert(U);  // FIXME
	assert(!W);  // FIXME

	uint32_t n = r[Rn];
	if (Rn == 15) n &= ~3;
	if (P) n += r[Rm];
	if (L) {
		r[Rd] = align_halfword(n, memory_read_halfword(n & ~1));
	} else {
		memory_write_halfword(n, (uint16_t) r[Rd]);
	}
	if (!P) r[Rn] += r[Rm];  // FIXME?
}

void arm_load_store_halfword_immediate(void) {
	bool P = (arm_op & (1 << 24)) != 0;
	bool U = (arm_op & (1 << 23)) != 0;
	bool I = (arm_op & (1 << 22)) != 0;
	bool W = (arm_op & (1 << 21)) != 0;
	bool L = (arm_op & (1 << 20)) != 0;
	uint32_t Rn = (arm_op >> 16) & 0xf;
	uint32_t Rd = (arm_op >> 12) & 0xf;
	uint32_t imm = (arm_op & 0xf) | ((arm_op >> 4) & 0xf0);
	uint32_t opc = (arm_op >> 4) & 0xf;

	assert(I);
	assert(opc == 0xb);

#ifdef DEBUG
	if (log_instructions && log_arm_instructions) {
		arm_print_opcode();
		arm_print_mnemonic(L ? "ldrh" : "strh");
		arm_print_register(Rd);
		printf(",[");
		arm_print_register(Rn);
		if (P) {
			printf(",");
			arm_print_immediate(imm);
			printf("]\n");
		} else {
			printf("],");
			arm_print_immediate(imm);
			printf("\n");
		}
	}
#endif

	assert(U);  // FIXME
	assert(!W);  // FIXME

	uint32_t n = r[Rn];
	if (Rn == 15) n &= ~3;
	if (P) n += imm;
	if (L) {
		r[Rd] = align_halfword(n, memory_read_halfword(n & ~1));
	} else {
		memory_write_halfword(n, (uint16_t) r[Rd]);
	}
	if (!P) r[Rn] += imm;  // FIXME?
}

void arm_load_store_signed_byte_halfword_register(void) {
	bool P = (arm_op & (1 << 24)) != 0;
	bool U = (arm_op & (1 << 23)) != 0;
	bool I = (arm_op & (1 << 22)) != 0;
	bool W = (arm_op & (1 << 21)) != 0;
	bool L = (arm_op & (1 << 20)) != 0;
	uint32_t Rn = (arm_op >> 16) & 0xf;
	uint32_t Rd = (arm_op >> 12) & 0xf;
	uint32_t sbz = (arm_op >> 8) & 0xf;
	uint32_t opc = (arm_op >> 4) & 0xf;
	uint32_t Rm = arm_op & 0xf;

	assert(!I);
	assert(L);
	assert(sbz == 0);
	assert(opc == 0xd || opc == 0xf);

#ifdef DEBUG
	if (log_instructions && log_arm_instructions) {
		arm_print_opcode();
		if (opc == 0xd) {
			arm_print_mnemonic("ldrsb");
		} else if (opc == 0xf) {
			arm_print_mnemonic("ldrsh");
		} else {
			assert(false);
		}
		arm_print_register(Rd);
		printf(",[");
		arm_print_register(Rn);
		if (P) {
			printf(",");
			arm_print_register(Rm);
			printf("]\n");
		} else {
			printf("],");
			arm_print_register(Rm);
			printf("\n");
		}
	}
#endif

	assert(U);  // FIXME
	assert(!W);  // FIXME

	uint32_t n = r[Rn];
	if (Rn == 15) n &= ~3;
	if (P) n += r[Rm];
	if (opc == 0xd) {
		r[Rd] = memory_read_byte(n);
		if (r[Rd] & 0x80) r[Rd] |= ~0xff;
	} else if (opc == 0xf) {
		r[Rd] = align_halfword(n, memory_read_halfword(n & ~1));
		if ((n & 1) != 0) {
			if (r[Rd] & 0x80) r[Rd] |= ~0xff;
		} else {
			if (r[Rd] & 0x8000) r[Rd] |= ~0xffff;
		}
	} else {
		assert(false);
	}
	if (!P) r[Rn] += r[Rm];  // FIXME?
}

void arm_load_store_signed_byte_halfword_immediate(void) {
#ifdef DEBUG
	arm_print_opcode();
	printf("\n");
#endif

	assert(false);
}

void arm_special_data_processing_register(void) {
	bool R = (arm_op & (1 << 22)) != 0;
	bool b21 = (arm_op & (1 << 21)) != 0;
	uint32_t mask = (arm_op >> 16) & 0xf;
	uint32_t Rd = (arm_op >> 12) & 0xf;
	uint32_t Rm = arm_op & 0xf;

	if (b21) {
		assert(Rd == 0xf);
		assert((arm_op & 0xff0) == 0);
	} else {
		assert(mask == 0xf);
		assert((arm_op & 0xfff) == 0);
	}

#ifdef DEBUG
	if (log_instructions && log_arm_instructions) {
		arm_print_opcode();
		if (b21) {
			arm_print_mnemonic("msr");
			printf(R ? "spsr" : "cpsr");
			printf("_");
			switch (mask) {
				case 8: printf("f"); break;
				case 9: printf("cf"); break;
				default: assert(false); break;
			}
			printf(",");
			arm_print_register(Rm);
		} else {
			arm_print_mnemonic("mrs");
			arm_print_register(Rd);
			printf(",");
			printf(R ? "spsr" : "cpsr");
		}
		printf("\n");
	}
#endif

	if (b21) {
		if (mask == 8) {
			if (R) write_spsr(r[Rm] & 0xf0000000);
			else write_cpsr(r[Rm] & 0xf0000000);
		} else if (mask == 9) {
			if (R) write_spsr(r[Rm] & 0xf00000ff);
			else write_cpsr(r[Rm] & 0xf00000ff);
		} else {
			assert(false);
		}
	} else {
		if (R) r[Rd] = read_spsr();
		else r[Rd] = cpsr;
	}
}

void arm_special_data_processing_immediate(void) {
	bool R = (arm_op & (1 << 22)) != 0;
	uint32_t mask = (arm_op >> 16) & 0xf;
	uint32_t Rd = (arm_op >> 12) & 0xf;
	uint32_t rot = (arm_op >> 8) & 0xf;
	uint32_t imm = ror(arm_op & 0xff, 2 * rot);

	assert(Rd == 0xf);

#ifdef DEBUG
	if (log_instructions && log_arm_instructions) {
		arm_print_opcode();
		arm_print_mnemonic("msr");
		printf(R ? "spsr" : "cpsr");
		printf("_");
		switch (mask) {
			case 8: printf("f"); break;
			case 9: printf("cf"); break;
			default: assert(false); break;
		}
		printf(",");
		arm_print_immediate(imm);
		printf("\n");
	}
#endif

	assert(!R);
	if (mask == 8) {
		write_cpsr(imm & 0xf0000000);
	} else if (mask == 9) {
		write_cpsr(imm & 0xf00000ff);
	} else {
		assert(false);
	}
}

void arm_branch_and_exchange(void) {
	uint32_t sbo = (arm_op >> 8) & 0xfff;
	uint32_t Rm = arm_op & 0xf;

	assert(sbo == 0xfff);
	if ((r[Rm] & 1) == 0) {
		assert((r[Rm] & 2) == 0);
	}

#ifdef DEBUG
	if (log_instructions && log_arm_instructions) {
		arm_print_opcode();
		arm_print_mnemonic("bx");
		arm_print_register(Rm);
		printf("  ; Rm = 0x%x", r[Rm]);
		printf("\n");
	}
#endif

	if ((r[Rm] & 1) != 0) { cpsr |= PSR_T; } else { cpsr &= ~PSR_T; }
	r[15] = r[Rm] & ~1;
	branch_taken = true;
}

void thumb_shift_by_immediate(void) {
	uint16_t opc = (thumb_op >> 11) & 3;
	uint16_t imm = (thumb_op >> 6) & 0x1f;
	uint16_t Rm = (thumb_op >> 3) & 7;
	uint16_t Rd = thumb_op & 7;

	assert(opc != 3);

#ifdef DEBUG
	if (log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		switch (opc) {
			case 0: thumb_print_mnemonic("lsl"); break;
			case 1: thumb_print_mnemonic("lsr"); break;
			case 2: thumb_print_mnemonic("asr"); break;
			case 3: assert(false); break;
		}
		thumb_print_register(Rd);
		printf(",");
		thumb_print_register(Rm);
		printf(",");
		thumb_print_immediate(imm);
		printf("\n");
	}
#endif

	arm_op = COND_AL << 28 | 0x1b << 20 | Rd << 12 | imm << 7 | Rm;
	switch (opc) {
		case 0: arm_op |= SHIFT_LSL << 5; break;
		case 1: arm_op |= SHIFT_LSR << 5; break;
		case 2: arm_op |= SHIFT_ASR << 5; break;
		case 3: assert(false); break;
	}
	arm_data_processing_register();
}

void thumb_add_subtract_register(void) {
	uint16_t opc = (thumb_op >> 9) & 1;
	uint16_t Rm = (thumb_op >> 6) & 7;
	uint16_t Rn = (thumb_op >> 3) & 7;
	uint16_t Rd = thumb_op & 7;

#ifdef DEBUG
	if (log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		switch (opc) {
			case 0: thumb_print_mnemonic("add"); break;
			case 1: thumb_print_mnemonic("sub"); break;
		}
		thumb_print_register(Rd);
		printf(",");
		thumb_print_register(Rn);
		printf(",");
		thumb_print_register(Rm);
		printf("\n");
	}
#endif

	arm_op = COND_AL << 28 | 0x01 << 20 | Rn << 16 | Rd << 12 | Rm;
	switch (opc) {
		case 0: arm_op |= ARM_ADD << 21; break;
		case 1: arm_op |= ARM_SUB << 21; break;
	}
	arm_data_processing_register();
}

void thumb_add_subtract_immediate(void) {
	uint16_t opc = (thumb_op >> 9) & 1;
	uint16_t imm = (thumb_op >> 6) & 7;
	uint16_t Rn = (thumb_op >> 3) & 7;
	uint16_t Rd = thumb_op & 7;

#ifdef DEBUG
	if (log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		switch (opc) {
			case 0: thumb_print_mnemonic("add"); break;
			case 1: thumb_print_mnemonic("sub"); break;
		}
		thumb_print_register(Rd);
		printf(",");
		thumb_print_register(Rn);
		printf(",");
		thumb_print_immediate(imm);
		printf("\n");
	}
#endif

	arm_op = COND_AL << 28 | 0x21 << 20 | Rn << 16 | Rd << 12 | imm;
	switch (opc) {
		case 0: arm_op |= ARM_ADD << 21; break;
		case 1: arm_op |= ARM_SUB << 21; break;
	}
	arm_data_processing_immediate();
}

void thumb_add_subtract_compare_move_immediate(void) {
	uint16_t opc = (thumb_op >> 11) & 3;
	uint16_t Rdn = (thumb_op >> 8) & 7;
	uint16_t imm = thumb_op & 0xff;

#ifdef DEBUG
	if (log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		switch (opc) {
			case 0: thumb_print_mnemonic("mov"); break;
			case 1: thumb_print_mnemonic("cmp"); break;
			case 2: thumb_print_mnemonic("add"); break;
			case 3: thumb_print_mnemonic("sub"); break;
		}
		thumb_print_register(Rdn);
		printf(",");
		thumb_print_immediate(imm);
		printf("\n");
	}
#endif

	arm_op = COND_AL << 28 | 0x21 << 20 | imm;
	switch (opc) {
		case 0: arm_op |= ARM_MOV << 21 | Rdn << 12; break;
		case 1: arm_op |= ARM_CMP << 21 | Rdn << 16; break;
		case 2: arm_op |= ARM_ADD << 21 | Rdn << 16 | Rdn << 12; break;
		case 3: arm_op |= ARM_SUB << 21 | Rdn << 16 | Rdn << 12; break;
	}
	arm_data_processing_immediate();
}

void thumb_data_processing_register(void) {
	uint16_t opc = (thumb_op >> 6) & 0xf;
	uint16_t Rm = (thumb_op >> 3) & 7;
	uint16_t Rn = thumb_op & 7;

#ifdef DEBUG
	if (log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		switch (opc) {
			case THUMB_AND: thumb_print_mnemonic("and"); break;
			case THUMB_EOR: thumb_print_mnemonic("eor"); break;
			case THUMB_LSL: thumb_print_mnemonic("lsl"); break;
			case THUMB_LSR: thumb_print_mnemonic("lsr"); break;
			case THUMB_ASR: thumb_print_mnemonic("asr"); break;
			case THUMB_ADC: thumb_print_mnemonic("adc"); break;
			case THUMB_SBC: thumb_print_mnemonic("sbc"); break;
			case THUMB_ROR: thumb_print_mnemonic("ror"); break;
			case THUMB_TST: thumb_print_mnemonic("tst"); break;
			case THUMB_NEG: thumb_print_mnemonic("neg"); break;
			case THUMB_CMP: thumb_print_mnemonic("cmp"); break;
			case THUMB_CMN: thumb_print_mnemonic("cmn"); break;
			case THUMB_ORR: thumb_print_mnemonic("orr"); break;
			case THUMB_MUL: thumb_print_mnemonic("mul"); break;
			case THUMB_BIC: thumb_print_mnemonic("bic"); break;
			case THUMB_MVN: thumb_print_mnemonic("mvn"); break;
		}
		thumb_print_register(Rn);
		printf(",");
		thumb_print_register(Rm);
		printf("\n");
	}
#endif

	arm_op = COND_AL << 28;
	switch (opc) {
		case THUMB_AND: arm_op |= ARM_AND << 21 | 0x01 << 20 | Rn << 16 | Rn << 12 | Rm; break;
		case THUMB_EOR: arm_op |= ARM_EOR << 21 | 0x01 << 20 | Rn << 16 | Rn << 12 | Rm; break;
		case THUMB_LSL: arm_op |= ARM_MOV << 21 | 0x01 << 20 | Rn << 12 | Rm << 8 | SHIFT_LSL << 5 | 1 << 4 | Rn; break;
		case THUMB_LSR: arm_op |= ARM_MOV << 21 | 0x01 << 20 | Rn << 12 | Rm << 8 | SHIFT_LSR << 5 | 1 << 4 | Rn; break;
		case THUMB_ASR: arm_op |= ARM_MOV << 21 | 0x01 << 20 | Rn << 12 | Rm << 8 | SHIFT_ASR << 5 | 1 << 4 | Rn; break;
		case THUMB_ADC: arm_op |= ARM_ADC << 21 | 0x01 << 20 | Rn << 16 | Rn << 12 | Rm; break;
		case THUMB_SBC: arm_op |= ARM_SBC << 21 | 0x01 << 20 | Rn << 16 | Rn << 12 | Rm; break;
		case THUMB_ROR: arm_op |= ARM_MOV << 21 | 0x01 << 20 | Rn << 12 | Rm << 8 | SHIFT_ROR << 5 | 1 << 4 | Rn; break;
		case THUMB_TST: arm_op |= ARM_TST << 21 | 0x01 << 20 | Rn << 16 | Rm; break;
		case THUMB_NEG: arm_op |= ARM_RSB << 21 | 0x21 << 20 | Rm << 16 | Rn << 12; break;
		case THUMB_CMP: arm_op |= ARM_CMP << 21 | 0x01 << 20 | Rn << 16 | Rm; break;
		case THUMB_CMN: arm_op |= ARM_CMN << 21 | 0x01 << 20 | Rn << 16 | Rm; break;
		case THUMB_ORR: arm_op |= ARM_ORR << 21 | 0x01 << 20 | Rn << 16 | Rn << 12 | Rm; break;
		case THUMB_MUL: arm_op |= ARM_AND << 21 | 0x01 << 20 | Rn << 16 | Rn << 8 | 0x9 << 4 | Rm; break;
		case THUMB_BIC: arm_op |= ARM_BIC << 21 | 0x01 << 20 | Rn << 16 | Rn << 12 | Rm; break;
		case THUMB_MVN: arm_op |= ARM_MVN << 21 | 0x01 << 20 | Rn << 12 | Rm; break;
	}
	if (opc == THUMB_NEG) {
		arm_data_processing_immediate();
	} else if (opc == THUMB_MUL) {
		arm_multiply();
	} else {
		arm_data_processing_register();
	}
}

void thumb_special_data_processing(void) {
	uint16_t opc = (thumb_op >> 8) & 3;
	uint16_t Rm = (thumb_op >> 3) & 0xf;
	uint16_t Rd = (thumb_op & 7) | ((thumb_op >> 4) & 8);

	assert(opc != 3);

#ifdef DEBUG
	if (log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		switch (opc) {
			case 0: thumb_print_mnemonic("add"); break;
			case 1: thumb_print_mnemonic("cmp"); break;
			case 2: thumb_print_mnemonic("mov"); break;
		}
		thumb_print_register(Rd);
		printf(",");
		thumb_print_register(Rm);
		printf("\n");
	}
#endif

	arm_op = COND_AL << 28 | Rm;
	switch (opc) {
		case 0: arm_op |= 0x08 << 20 | Rd << 16 | Rd << 12; break;
		case 1: arm_op |= 0x15 << 20 | Rd << 16; break;
		case 2: arm_op |= 0x1a << 20 | Rd << 12; break;
	}
	arm_data_processing_register();
}

void thumb_branch_exchange_instruction_set(void) {
	bool L = (thumb_op & (1 << 7)) != 0;
	uint16_t Rm = (thumb_op >> 3) & 0xf;
	uint16_t sbz = thumb_op & 7;

	assert(!L);
	assert(sbz == 0);

#ifdef DEBUG
	if (log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		thumb_print_mnemonic("bx");
		thumb_print_register(Rm);
		printf("\n");
	}
#endif

	arm_op = COND_AL << 28 | 0x12 << 20 | 0xfff << 8 | 0x1 << 4 | Rm;
	arm_branch_and_exchange();
}

void thumb_load_from_literal_pool(void) {
	uint16_t Rd = (thumb_op >> 8) & 7;
	uint16_t imm = thumb_op & 0xff;

#ifdef DEBUG
	if (log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		thumb_print_mnemonic("ldr");
		thumb_print_register(Rd);
		printf(",");
		thumb_print_address((r[15] & ~3) + (imm << 2));
		printf("\n");
	}
#endif

	arm_op = COND_AL << 28 | 0x59 << 20 | REG_PC << 16 | Rd << 12 | imm << 2;
	arm_single_data_transfer_immediate();
}

void thumb_load_store_register_offset(void) {
	uint16_t opc = (thumb_op >> 9) & 0x7;
	uint16_t Rm = (thumb_op >> 6) & 0x7;
	uint16_t Rn = (thumb_op >> 3) & 0x7;
	uint16_t Rd = thumb_op & 0x7;

#ifdef DEBUG
	if (log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		switch (opc) {
			case 0: thumb_print_mnemonic("str"); break;
			case 1: thumb_print_mnemonic("strh"); break;
			case 2: thumb_print_mnemonic("strb"); break;
			case 3: thumb_print_mnemonic("ldrsb"); break;
			case 4: thumb_print_mnemonic("ldr"); break;
			case 5: thumb_print_mnemonic("ldrh"); break;
			case 6: thumb_print_mnemonic("ldrb"); break;
			case 7: thumb_print_mnemonic("ldrsh"); break;
			default: assert(false); break;
		}
		thumb_print_register(Rd);
		printf(",[");
		thumb_print_register(Rn);
		printf(",");
		thumb_print_register(Rm);
		printf("]\n");
	}
#endif

	arm_op = COND_AL << 28 | Rn << 16 | Rd << 12 | Rm;
	switch (opc) {
		case 0: arm_op |= 0x78 << 20; break;
		case 1: arm_op |= 0x18 << 20 | 0xb << 4; break;
		case 2: arm_op |= 0x7c << 20; break;
		case 3: arm_op |= 0x19 << 20 | 0xd << 4; break;
		case 4: arm_op |= 0x79 << 20; break;
		case 5: arm_op |= 0x19 << 20 | 0xb << 4; break;
		case 6: arm_op |= 0x7d << 20; break;
		case 7: arm_op |= 0x19 << 20 | 0xf << 4; break;
		default: assert(false); break;
	}
	switch (opc) {
		case 0:
		case 2:
		case 4:
		case 6:
			arm_single_data_transfer_register();
			break;

		case 1:
		case 5:
			arm_load_store_halfword_register();
			break;

		case 3:
		case 7:
			arm_load_store_signed_byte_halfword_register();
			break;

		default:
			assert(false);
			break;
	}
}

void thumb_load_store_word_byte_immediate_offset(void) {
	bool B = (thumb_op & (1 << 12)) != 0;
	bool L = (thumb_op & (1 << 11)) != 0;
	uint16_t imm = (thumb_op >> 6) & 0x1f;
	uint16_t Rn = (thumb_op >> 3) & 7;
	uint16_t Rd = thumb_op & 7;

#ifdef DEBUG
	if (log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		if (L) {
			thumb_print_mnemonic(B ? "ldrb" : "ldr");
		} else {
			thumb_print_mnemonic(B ? "strb" : "str");
		}
		thumb_print_register(Rd);
		printf(",[");
		thumb_print_register(Rn);
		printf(",");
		thumb_print_immediate(imm);
		printf("]\n");
	}
#endif

	arm_op = COND_AL << 28 | 0x58 << 20 | Rn << 16 | Rd << 12;
	if (B) {
		arm_op |= 1 << 22 | imm;
	} else {
		arm_op |= imm << 2;
	}
	if (L) {
		arm_op |= 1 << 20;
	}
	arm_single_data_transfer_immediate();
}

void thumb_load_store_halfword_immediate_offset(void) {
	bool L = (thumb_op & (1 << 11)) != 0;
	uint16_t imm = (thumb_op >> 6) & 0x1f;
	uint16_t Rn = (thumb_op >> 3) & 7;
	uint16_t Rd = thumb_op & 7;

#ifdef DEBUG
	if (log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		if (L) {
			thumb_print_mnemonic("ldrh");
		} else {
			thumb_print_mnemonic("strh");
		}
		thumb_print_register(Rd);
		printf(",[");
		thumb_print_register(Rn);
		printf(",");
		thumb_print_immediate(imm);
		printf("]\n");
	}
#endif

	arm_op = COND_AL << 28 | Rn << 16 | Rd << 12 | (imm & 0x18) << 5 | 0xb << 4 | (imm & 7) << 1;
	if (L) {
		arm_op |= 0x1d << 20;
	} else {
		arm_op |= 0x1c << 20;
	}
	arm_load_store_halfword_immediate();
}

void thumb_load_store_to_from_stack(void) {
	bool L = (thumb_op & (1 << 11)) != 0;
	uint16_t Rd = (thumb_op >> 8) & 7;
	int32_t imm = thumb_op & 0xff;

#ifdef DEBUG
	if (log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		thumb_print_mnemonic(L ? "ldr" : "str");
		thumb_print_register(Rd);
		printf(",[");
		thumb_print_register(REG_SP);
		printf(",");
		thumb_print_immediate(imm);
		printf("]\n");
	}
#endif

	arm_op = COND_AL << 28 | REG_SP << 16 | Rd << 12 | imm << 2;
	if (L) {
		arm_op |= 0x59 << 20;
	} else {
		arm_op |= 0x58 << 20;
	}
	arm_single_data_transfer_immediate();
}

void thumb_add_to_sp_or_pc(void) {
	bool SP = (thumb_op & (1 << 11)) != 0;
	uint16_t Rd = (thumb_op >> 8) & 7;
	int32_t imm = thumb_op & 0xff;

#ifdef DEBUG
	if (log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		thumb_print_mnemonic("add");
		thumb_print_register(Rd);
		printf(",");
		thumb_print_register(SP ? REG_SP : REG_PC);
		printf(",");
		thumb_print_immediate(ror(imm, 30));
		printf("\n");
	}
#endif

	arm_op = COND_AL << 28 | 0x28 << 20 | (SP ? REG_SP : REG_PC) << 16 | Rd << 12 | 0xf << 8 | imm;
	arm_data_processing_immediate();
}

void thumb_adjust_stack_pointer(void) {
	uint16_t opc = (thumb_op >> 7) & 1;
	uint32_t imm = thumb_op & 0x7f;

#ifdef DEBUG
	if (log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		thumb_print_mnemonic(opc == 1 ? "sub" : "add");
		thumb_print_register(REG_SP);
		printf(",");
		thumb_print_immediate(ror(imm, 30));
		printf("\n");
	}
#endif

	arm_op = COND_AL << 28 | 0x20 << 20 | REG_SP << 16 | REG_SP << 12 | 0xf << 8 | imm;
	if (opc == 1) {
		arm_op |= ARM_SUB << 21;
	} else {
		arm_op |= ARM_ADD << 21;
	}
	arm_data_processing_immediate();
}

void thumb_push_pop_register_list(void) {
	bool L = (thumb_op & (1 << 11)) != 0;
	bool R = (thumb_op & (1 << 8)) != 0;
	uint32_t rlist = thumb_op & 0xff;

#ifdef DEBUG
	if (log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		thumb_print_mnemonic(L ? "pop" : "push");
		printf("{");
		bool first = true;
		int i = 0;
		while (i < 8) {
			if (rlist & (1 << i)) {
				int j = i + 1;
				while (rlist & (1 << j)) j++;
				if (j == i + 1) {
					if (!first) printf(",");
					thumb_print_register(i);
				} else if (j == i + 2) {
					if (!first) printf(",");
					thumb_print_register(i);
					printf(",");
					thumb_print_register(j - 1);
				} else {
					if (!first) printf(",");
					thumb_print_register(i);
					printf("-");
					thumb_print_register(j - 1);
				}
				i = j;
				first = false;
			}
			i++;
		}
		if (L) {
			if (R) {
				if (!first) printf(",");
				thumb_print_register(15);
			}
		} else {
			if (R) {
				if (!first) printf(",");
				thumb_print_register(14);
			}
		}
		printf("}\n");
	}
#endif

	arm_op = COND_AL << 28 | REG_SP << 16 | rlist;
	if (L) {
		arm_op |= 0x8b << 20;
		if (R) arm_op |= 1 << 15;
	} else {
		arm_op |= 0x92 << 20;
		if (R) arm_op |= 1 << 14;
	}
	arm_block_data_transfer();
}

void thumb_load_store_multiple(void) {
	bool L = (thumb_op & (1 << 11)) != 0;
	uint32_t Rn = (thumb_op >> 8) & 7;
	uint32_t rlist = thumb_op & 0xff;

	bool W = true;
	if (L && (rlist & (1 << Rn)) != 0) {
		W = false;
	}

#ifdef DEBUG
	if (log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		thumb_print_mnemonic(L ? "ldmia" : "stmia");
		thumb_print_register(Rn);
		if (W) printf("!");
		printf(",{");
		bool first = true;
		int i = 0;
		while (i < 8) {
			if (rlist & (1 << i)) {
				int j = i + 1;
				while (rlist & (1 << j)) j++;
				if (j == i + 1) {
					if (!first) printf(",");
					thumb_print_register(i);
				} else if (j == i + 2) {
					if (!first) printf(",");
					thumb_print_register(i);
					printf(",");
					thumb_print_register(j - 1);
				} else {
					if (!first) printf(",");
					thumb_print_register(i);
					printf("-");
					thumb_print_register(j - 1);
				}
				i = j;
				first = false;
			}
			i++;
		}
		printf("}\n");
	}
#endif

	arm_op = COND_AL << 28 | Rn << 16 | rlist;
	if (L) {
		arm_op |= 0x89 << 20 | (W ? 1 : 0) << 21;
	} else {
		arm_op |= 0x8a << 20;
	}
	arm_block_data_transfer();
}

void thumb_conditional_branch(void) {
	uint16_t cond = (thumb_op >> 8) & 0xf;
	uint32_t imm = thumb_op & 0xff;
	if (thumb_op & 0x80) imm |= ~0xff;
	assert(cond != 0xe && cond != 0xf);

#ifdef DEBUG
	if (log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		switch (cond) {
			case COND_EQ: thumb_print_mnemonic("beq"); break;
			case COND_NE: thumb_print_mnemonic("bne"); break;
			case COND_CS: thumb_print_mnemonic("bcs"); break;
			case COND_CC: thumb_print_mnemonic("bcc"); break;
			case COND_MI: thumb_print_mnemonic("bmi"); break;
			case COND_PL: thumb_print_mnemonic("bpl"); break;
			case COND_VS: thumb_print_mnemonic("bvs"); break;
			case COND_VC: thumb_print_mnemonic("bvc"); break;
			case COND_HI: thumb_print_mnemonic("bhi"); break;
			case COND_LS: thumb_print_mnemonic("bls"); break;
			case COND_GE: thumb_print_mnemonic("bge"); break;
			case COND_LT: thumb_print_mnemonic("blt"); break;
			case COND_GT: thumb_print_mnemonic("bgt"); break;
			case COND_LE: thumb_print_mnemonic("ble"); break;
			case COND_AL: assert(false); break;
			case COND_NV: assert(false); break;
		}
		thumb_print_address(r[15] + (imm << 1));
		printf("\n");
	}
#endif

	if (cpsr_check_condition(cond)) {
		r[15] += imm << 1;
		branch_taken = true;
	}
}

void thumb_software_interrupt(void) {
//#ifdef DEBUG
	if (true) {  // log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		thumb_print_mnemonic("swi");
		thumb_print_address(thumb_op & 0xff);
		printf("\n");
	}
//#endif

	assert(false);
	r14_svc = (r[15] - 2) | 1;
	spsr_svc = cpsr;
	write_cpsr((cpsr & ~(PSR_T | PSR_MODE)) | PSR_I | PSR_MODE_SVC);
	r[15] = PC_SWI;
	branch_taken = true;
}

void thumb_unconditional_branch(void) {
	uint32_t imm = thumb_op & 0x7ff;
	if (thumb_op & 0x400) imm |= ~0x7ff;

#ifdef DEBUG
	if (log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		thumb_print_mnemonic("b");
		thumb_print_address(r[15] + (imm << 1));
		printf("\n");
	}
#endif

	r[15] += imm << 1;
	branch_taken = true;
}

void thumb_bl_prefix(void) {
	uint32_t imm = thumb_op & 0x7ff;
	if (imm & 0x400) imm |= ~0x7ff;

#ifdef DEBUG
	if (log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		thumb_print_mnemonic("bl.1");
		printf("\n");
	}
#endif

	r[14] = r[15] + (imm << 12);
}

void thumb_bl_suffix(void) {
	uint16_t imm = thumb_op & 0x7ff;

	uint32_t return_address = (r[15] - 2) | 1;
	uint32_t target_address = r[14] + (imm << 1);

#ifdef DEBUG
	if (log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		thumb_print_mnemonic("bl.2");
		thumb_print_address(target_address);
		printf("\n");
	}
#endif

	r[14] = return_address;
	r[15] = target_address;
	branch_taken = true;
}

void arm_undefined_instruction(void) {
#ifdef DEBUG
	if (log_instructions && log_arm_instructions) {
		arm_print_opcode();
		arm_print_mnemonic("undefined");
		printf("\n");
	}
#endif

	assert(false);
}

void thumb_undefined_instruction(void) {
#ifdef DEBUG
	if (log_instructions && log_thumb_instructions) {
		thumb_print_opcode();
		thumb_print_mnemonic("undefined");
		printf("\n");
	}
#endif

	assert(false);
}

void arm_step(void) {
	if (branch_taken) {
		arm_op = memory_read_word(r[15]);
		arm_pipeline[0] = memory_read_word(r[15] + 4);
		arm_pipeline[1] = memory_read_word(r[15] + 8);
		branch_taken = false;
		r[15] += 8;
	} else {
		arm_op = arm_pipeline[0];
		arm_pipeline[0] = arm_pipeline[1];
		arm_pipeline[1] = memory_read_word(r[15]);
	}

#ifdef DEBUG
	if (log_registers) {
		print_registers();
	}
#endif

	uint32_t cond = (arm_op >> 28) & 0xf;
	if (cond == COND_AL || cpsr_check_condition(cond)) {
		uint32_t index = ((arm_op >> 4) & 0xf) | ((arm_op >> 16) & 0xff0);
		void (*handler)(void) = arm_lookup[index];
		if (handler != NULL) {
			(*handler)();
		} else {
			arm_print_opcode();
			printf("unimplemented\n");
			printf("index = 0x%03x\n", index);
			assert(false);
		}
	}

#ifdef DEBUG
	if (log_registers) {
		printf("\n");
	}
#endif

	if (!branch_taken) {
		r[15] += 4;
	}
}

void thumb_step(void) {
	if (branch_taken) {
		thumb_op = memory_read_halfword(r[15]);
		thumb_pipeline[0] = memory_read_halfword(r[15] + 2);
		thumb_pipeline[1] = memory_read_halfword(r[15] + 4);
		branch_taken = false;
		r[15] += 4;
	} else {
		thumb_op = thumb_pipeline[0];
		thumb_pipeline[0] = thumb_pipeline[1];
		thumb_pipeline[1] = memory_read_halfword(r[15]);
	}

#ifdef DEBUG
	if (log_registers) {
		print_registers();
	}
#endif

	uint16_t index = (thumb_op >> 8) & 0xff;
	void (*handler)(void) = thumb_lookup[index];
	if (handler != NULL) {
		(*handler)();
	} else {
		thumb_print_opcode();
		printf("unimplemented\n");
		printf("index = 0x%02x\n", index);
		assert(false);
	}

#ifdef DEBUG
	if (log_registers) {
		printf("\n");
	}
#endif

	if (!branch_taken) {
		r[15] += 2;
	}
}

void arm_bind(uint32_t n, uint32_t m, void (*f)(void)) {
	uint32_t s = m;
	while (true) {
		arm_lookup[n | s] = f;
		if (s == 0) break;
		s = (s - 1) & m;
	}
}

void thumb_bind(uint16_t n, uint16_t m, void (*f)(void)) {
	uint16_t s = m;
	while (true) {
		thumb_lookup[n | s] = f;
		if (s == 0) break;
		s = (s - 1) & m;
	}
}

void arm_init_lookup(void) {
	memset(arm_lookup, 0, sizeof(void *) * 4096);

	arm_bind(0x000, 0xfff, NULL);  // arm_undefined_instruction);

	arm_bind(0x000, 0x1ff, arm_data_processing_register);
	arm_bind(0x200, 0x1ff, arm_data_processing_immediate);
	arm_bind(0x400, 0x1ff, arm_single_data_transfer_immediate);
	arm_bind(0x600, 0x1fe, arm_single_data_transfer_register);
	arm_bind(0x800, 0x1ff, arm_block_data_transfer);
	arm_bind(0xa00, 0x1ff, arm_branch);
	arm_bind(0xc00, 0x1ff, NULL);  // arm_coprocessor_load_store
	arm_bind(0xe00, 0x0ff, NULL);  // arm_coprocessor_data_processing
	arm_bind(0xf00, 0x0ff, arm_software_interrupt);
	arm_bind(0x009, 0x030, arm_multiply);
	arm_bind(0x089, 0x070, NULL);  // arm_multiply_long
	arm_bind(0x00b, 0x1b0, arm_load_store_halfword_register);
	arm_bind(0x04b, 0x1b0, arm_load_store_halfword_immediate);
	arm_bind(0x01d, 0x1e2, NULL);  // ldrsb/ldrsh
	arm_bind(0x100, 0x04f, arm_special_data_processing_register);
	arm_bind(0x109, 0x040, NULL);  // arm_swap
	arm_bind(0x120, 0x04e, arm_special_data_processing_register);
	arm_bind(0x121, 0x000, arm_branch_and_exchange);
	//arm_bind(0x121, 0x04e, NULL);  // bx?
	arm_bind(0x320, 0x00f, arm_special_data_processing_immediate);
	arm_bind(0x360, 0x00f, arm_special_data_processing_immediate);
}

void thumb_init_lookup(void) {
	memset(thumb_lookup, 0, sizeof(void *) * 256);

	thumb_bind(0x00, 0xff, thumb_undefined_instruction);

	thumb_bind(0x00, 0x0f, thumb_shift_by_immediate);
	thumb_bind(0x10, 0x07, thumb_shift_by_immediate);
	thumb_bind(0x18, 0x03, thumb_add_subtract_register);
	thumb_bind(0x1c, 0x03, thumb_add_subtract_immediate);
	thumb_bind(0x20, 0x1f, thumb_add_subtract_compare_move_immediate);
	thumb_bind(0x40, 0x03, thumb_data_processing_register);
	thumb_bind(0x44, 0x03, thumb_special_data_processing);
	thumb_bind(0x47, 0x00, thumb_branch_exchange_instruction_set);
	thumb_bind(0x48, 0x07, thumb_load_from_literal_pool);
	thumb_bind(0x50, 0x0f, thumb_load_store_register_offset);
	thumb_bind(0x60, 0x1f, thumb_load_store_word_byte_immediate_offset);
	thumb_bind(0x80, 0x0f, thumb_load_store_halfword_immediate_offset);
	thumb_bind(0x90, 0x0f, thumb_load_store_to_from_stack);
	thumb_bind(0xa0, 0x0f, thumb_add_to_sp_or_pc);
	thumb_bind(0xb0, 0x0b, thumb_adjust_stack_pointer);
	thumb_bind(0xb4, 0x0b, thumb_push_pop_register_list);
	thumb_bind(0xc0, 0x0f, thumb_load_store_multiple);
	thumb_bind(0xd0, 0x0f, thumb_conditional_branch);
	thumb_bind(0xde, 0x00, thumb_undefined_instruction);
	thumb_bind(0xdf, 0x00, thumb_software_interrupt);
	thumb_bind(0xe0, 0x07, thumb_unconditional_branch);
	thumb_bind(0xf0, 0x07, thumb_bl_prefix);
	thumb_bind(0xf8, 0x07, thumb_bl_suffix);
}

void gba_init(const char *filename) {
	arm_init_lookup();
	thumb_init_lookup();

	FILE *fp = fopen("system_rom.bin", "rb");
	assert(fp != NULL);
	fread(system_rom, sizeof(uint8_t), 0x4000, fp);
	fclose(fp);

	fp = fopen(filename, "rb");
	assert(fp != NULL);
	fseek(fp, 0, SEEK_END);
	uint32_t rom_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	fread(rom, sizeof(uint8_t), rom_size, fp);
	fclose(fp);

	memset(r, 0, sizeof(uint32_t) * 16);

	bool skip_bios = true;  // FIXME

	if (skip_bios) {
		r[13] = 0x03007f00;
		r13_irq = 0x03007fa0;
		r13_svc = 0x03007fe0;
		r[15] = 0x08000000;
		cpsr = PSR_MODE_SYS;
	} else {
		r[15] = PC_RESET;
		cpsr = PSR_I | PSR_F | PSR_MODE_SVC;
	}

	branch_taken = true;
}

void gba_emulate(void) {
	for (int i = 0; i < 4096; i++) {
#ifdef DEBUG
		if (single_step && instruction_count >= start_logging_at) {
			log_instructions = true;
			log_registers = true;
		}
#endif

		if (cpsr & PSR_T) {
			thumb_step();
		} else {
			arm_step();
		}

#ifdef DEBUG
		if (single_step && instruction_count >= start_logging_at) {
			char c = fgetc(stdin);
			if (c == EOF) exit(EXIT_SUCCESS);
		}
#endif

		instruction_count++;
	}
}

void gba_draw_tiled_bg(SDL_PixelFormat *format, Uint32 *pixels, int pitch, uint32_t character_base, uint32_t screen_base) {
	for (int y = 0; y < 20; y++) {
		for (int x = 0; x < 30; x++) {
			uint16_t tile_index = *(uint16_t *)&video_ram[screen_base + (y * 32 + x) * 2];
			uint8_t *tile = &video_ram[character_base + tile_index * 0x20];
			uint32_t tile_counter = 0;
			for (int j = 0; j < 8; j++) {
				for (int i = 0; i < 8; i += 2) {
					uint8_t pixel_indexes = tile[tile_counter++];
					uint8_t pixel_index_0 = pixel_indexes & 0xf;
					uint8_t pixel_index_1 = (pixel_indexes >> 4) & 0xf;
					uint16_t pixel_0 = *(uint16_t *)&palette_ram[pixel_index_0 * 2];
					uint16_t pixel_1 = *(uint16_t *)&palette_ram[pixel_index_1 * 2];
					uint32_t r, g, b;
					r = pixel_0 & 0x1f;
					g = (pixel_0 >> 5) & 0x1f;
					b = (pixel_0 >> 10) & 0x1f;
					r = (r << 3) | (r >> 2);
					g = (g << 3) | (g >> 2);
					b = (b << 3) | (b >> 2);
					pixels[(8*y+j) * (pitch / 4) + (8*x+i)] = SDL_MapRGB(format, r, g, b);
					r = pixel_1 & 0x1f;
					g = (pixel_1 >> 5) & 0x1f;
					b = (pixel_1 >> 10) & 0x1f;
					r = (r << 3) | (r >> 2);
					g = (g << 3) | (g >> 2);
					b = (b << 3) | (b >> 2);
					pixels[(8*y+j) * (pitch / 4) + (8*x+i+1)] = SDL_MapRGB(format, r, g, b);
				}
			}
		}
	}
}

void gba_draw_tiled(SDL_PixelFormat *format, Uint32 *pixels, int pitch) {
	uint32_t character_base, screen_base;

	if (io_dispcnt & (1 << 11)) {
		character_base = ((io_bg3cnt >> 2) & 3) * 0x4000;
		screen_base = ((io_bg3cnt >> 8) & 0x1f) * 0x800;
		gba_draw_tiled_bg(format, pixels, pitch, character_base, screen_base);
	}
	if (io_dispcnt & (1 << 10)) {
		character_base = ((io_bg2cnt >> 2) & 3) * 0x4000;
		screen_base = ((io_bg2cnt >> 8) & 0x1f) * 0x800;
		gba_draw_tiled_bg(format, pixels, pitch, character_base, screen_base);
	}
	if (io_dispcnt & (1 << 9)) {
		character_base = ((io_bg1cnt >> 2) & 3) * 0x4000;
		screen_base = ((io_bg1cnt >> 8) & 0x1f) * 0x800;
		gba_draw_tiled_bg(format, pixels, pitch, character_base, screen_base);
	}
	if (io_dispcnt & (1 << 8)) {
		character_base = ((io_bg0cnt >> 2) & 3) * 0x4000;
		screen_base = ((io_bg0cnt >> 8) & 0x1f) * 0x800;
		gba_draw_tiled_bg(format, pixels, pitch, character_base, screen_base);
	}
}

void gba_draw_bitmap(SDL_PixelFormat *format, Uint32 *pixels, int pitch, uint32_t mode) {
	for (int y = 0; y < 160; y++) {
		for (int x = 0; x < 240; x++) {
			uint16_t pixel;
			if (mode == 3) {
				pixel = *(uint16_t *)&video_ram[(y * 240 + x) * 2];
			} else {
				uint8_t pixel_index = video_ram[y * 240 + x];
				pixel = *(uint16_t *)&palette_ram[pixel_index * 2];
			}
			uint32_t r, g, b;
			r = pixel & 0x1f;
			g = (pixel >> 5) & 0x1f;
			b = (pixel >> 10) & 0x1f;
			r = (r << 3) | (r >> 2);
			g = (g << 3) | (g >> 2);
			b = (b << 3) | (b >> 2);
			pixels[y * (pitch / 4) + x] = SDL_MapRGB(format, r, g, b);
		}
	}
}

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s filename.gba\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	gba_init(argv[1]);

	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window *window = SDL_CreateWindow("GBA", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	Uint32 pixel_format = SDL_GetWindowPixelFormat(window);
	SDL_Texture *texture = SDL_CreateTexture(renderer, pixel_format, SDL_TEXTUREACCESS_STREAMING, DISPLAY_WIDTH, DISPLAY_HEIGHT);
	SDL_PixelFormat *format = SDL_AllocFormat(pixel_format);

	bool quit = false;
	while (!quit) {
		SDL_Event e;
		while (SDL_PollEvent(&e) != 0) {
			if (e.type == SDL_QUIT) {
				quit = true;
			}
		}

		const Uint8 *state = SDL_GetKeyboardState(NULL);
		keys[0] = state[SDL_SCANCODE_X];          // Button A
		keys[1] = state[SDL_SCANCODE_Z];          // Button B
		keys[2] = state[SDL_SCANCODE_BACKSPACE];  // Select
		keys[3] = state[SDL_SCANCODE_RETURN];     // Start
		keys[4] = state[SDL_SCANCODE_RIGHT];      // Right
		keys[5] = state[SDL_SCANCODE_LEFT];       // Left
		keys[6] = state[SDL_SCANCODE_UP];         // Up
		keys[7] = state[SDL_SCANCODE_DOWN];       // Down
		keys[8] = state[SDL_SCANCODE_S];          // Button R
		keys[9] = state[SDL_SCANCODE_A];          // Button L

		gba_emulate();

		Uint32 *pixels;
		int pitch;
		SDL_LockTexture(texture, NULL, (void**) &pixels, &pitch);
		uint32_t mode = io_dispcnt & 7;
		switch (mode) {
			case 0:
			default:
				gba_draw_tiled(format, pixels, pitch);
				break;

			case 3:
			case 4:
				gba_draw_bitmap(format, pixels, pitch, mode);
				break;
		}
		SDL_UnlockTexture(texture);

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(renderer);
		SDL_Rect displayRect = {0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT};
		SDL_Rect screenRect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
		SDL_RenderCopy(renderer, texture, &displayRect, &screenRect);
		SDL_RenderPresent(renderer);
	}

	SDL_FreeFormat(format);
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
    return 0;
}
