#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cpu.h"

uint64_t instruction_count = 0;

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
int (*arm_lookup[4096])(void);

uint16_t thumb_op;
uint16_t thumb_pipeline[2];
int (*thumb_lookup[256])(void);

void arm_init_registers(bool skip_bios) {
    if (skip_bios) {
        r[13] = 0x03007f00;
        r13_irq = 0x03007fa0;
        r13_svc = 0x03007fe0;
        r[15] = 0x08000000;
        cpsr = PSR_MODE_SYS;
    } else {
        r[15] = VEC_RESET;
        cpsr = PSR_I | PSR_F | PSR_MODE_SVC;
    }
}

uint32_t align_word(uint32_t address, uint32_t value) {
    return ROR(value, 8 * (address & 3));
}

uint32_t align_halfword(uint32_t address, uint16_t value) {
    return ROR(value, 8 * (address & 1));
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
        default:
            assert(false);
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
        default:
            assert(false);
            break;
    }
}

uint16_t cond_lookup[16] = {
    0x56aa,  // ----
    0x6a6a,  // ---V
    0x55a6,  // --C-
    0x6966,  // --CV
    0x66a9,  // -Z--
    0x6a69,  // -Z-V
    0x66a5,  // -ZC-
    0x6a65,  // -ZCV
    0x6a9a,  // N---
    0x565a,  // N--V
    0x6996,  // N-C-
    0x5556,  // N-CV
    0x6a99,  // NZ--
    0x6659,  // NZ-V
    0x6a95,  // NZC-
    0x6655   // NZCV
};

bool condition_passed(uint32_t cond) {
    return BIT(cond_lookup[BITS(cpsr, 28, 31)], cond);
}

void write_cpsr(uint32_t psr) {
    // T bit should only be set by branches
    assert(branch_taken || ((cpsr & PSR_T) == (psr & PSR_T)));

    // In architecture versions without 26-bit backwards-compatibility support,
    // CPSR bit 4 (M[4]) always reads as 1, and all writes to it are ignored.
    psr |= 0x10;

    uint32_t old_mode = cpsr & PSR_MODE;
    cpsr = psr;
    uint32_t new_mode = cpsr & PSR_MODE;
    mode_change(old_mode, new_mode);
}

uint32_t read_spsr() {
    uint32_t mode = cpsr & PSR_MODE;
    switch (mode) {
        case PSR_MODE_USR: break;
        case PSR_MODE_FIQ: return spsr_fiq;
        case PSR_MODE_IRQ: return spsr_irq;
        case PSR_MODE_SVC: return spsr_svc;
        case PSR_MODE_ABT: return spsr_abt;
        case PSR_MODE_UND: return spsr_und;
        case PSR_MODE_SYS: break;
        default: assert(false); break;
    }
    return cpsr;
}

void write_spsr(uint32_t psr) {
    uint32_t mode = cpsr & PSR_MODE;
    switch (mode) {
        case PSR_MODE_USR: break;
        case PSR_MODE_FIQ: spsr_fiq = psr; break;
        case PSR_MODE_IRQ: spsr_irq = psr; break;
        case PSR_MODE_SVC: spsr_svc = psr; break;
        case PSR_MODE_ABT: spsr_abt = psr; break;
        case PSR_MODE_UND: spsr_und = psr; break;
        case PSR_MODE_SYS: break;
        default: assert(false); break;
    }
}

int arm_undefined_instruction(void) {
    assert(false);

    r14_und = r[REG_PC] - SIZEOF_INSTR;  // ARM: PC + 4, Thumb: PC + 2
    spsr_und = cpsr;
    write_cpsr((cpsr & ~(PSR_T | PSR_MODE)) | PSR_I | PSR_MODE_UND);
    r[REG_PC] = VEC_UNDEF;
    branch_taken = true;

    return 1;
}

int thumb_undefined_instruction(void) {
    assert(false);

    return arm_undefined_instruction();
}

int arm_step(void) {
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

    uint32_t cond = (arm_op >> 28) & 0xf;
    int cycles = 1;
    if (condition_passed(cond)) {
        uint32_t index = ((arm_op >> 4) & 0xf) | ((arm_op >> 16) & 0xff0);
        int (*handler)(void) = arm_lookup[index];
        assert(handler != NULL);
        cycles = (*handler)();
    }

    if (!branch_taken) {
        r[15] += 4;
    }

    return cycles;
}

int thumb_step(void) {
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

    uint16_t index = (thumb_op >> 8) & 0xff;
    int (*handler)(void) = thumb_lookup[index];
    assert(handler != NULL);
    int cycles = (*handler)();

    if (!branch_taken) {
        r[15] += 2;
    }

    return cycles;
}

void lookup_bind(int (**lookup)(void), const char *mask, int (*f)(void)) {
    uint32_t n = 0;
    uint32_t m = 0;
    for (const char *p = mask; *p != '\0'; p++) {
        n <<= 1;
        m <<= 1;
        switch (*p) {
            case '1': n |= 1; break;
            case 'x': m |= 1; break;
        }
    }

    uint32_t s = m;
    while (true) {
        lookup[n | s] = f;
        if (s == 0) break;
        s = (s - 1) & m;
    }
}

void arm_bind(const char *mask, int (*f)(void)) {
    lookup_bind(arm_lookup, mask, f);
}

void thumb_bind(const char *mask, int (*f)(void)) {
    lookup_bind(thumb_lookup, mask, f);
}

void arm_init_lookup(void) {
    memset(arm_lookup, 0, sizeof(void *) * 4096);

    arm_bind("000xxxxxxxxx", arm_data_processing_register);
    arm_bind("001xxxxxxxxx", arm_data_processing_immediate);
    arm_bind("010xxxxxxxxx", arm_load_store_word_or_byte_immediate);
    arm_bind("011xxxxxxxx0", arm_load_store_word_or_byte_register);
    arm_bind("011xxxxxxxx1", arm_undefined_instruction);
    arm_bind("100xxxxxxxxx", arm_load_store_multiple);
    arm_bind("101xxxxxxxxx", arm_branch);
    //arm_bind("110xxxxxxxxx", arm_coprocessor_load_store);
    //arm_bind("1110xxxxxxxx", arm_coprocessor_data_processing);
    arm_bind("1111xxxxxxxx", arm_software_interrupt);
    arm_bind("00010x00xxxx", arm_special_data_processing_register);  // must be before ldrh/strh
    arm_bind("00010x10xxx0", arm_special_data_processing_register);
    arm_bind("000000xx1001", arm_multiply);
    arm_bind("00001xxx1001", arm_multiply_long);
    arm_bind("000xx0xx1011", arm_load_store_halfword_register);
    arm_bind("000xx1xx1011", arm_load_store_halfword_immediate);
    arm_bind("000xx0xx11x1", arm_load_signed_halfword_or_signed_byte_register);
    arm_bind("000xx1xx11x1", arm_load_signed_halfword_or_signed_byte_immediate);
    arm_bind("00010x001001", arm_swap);
    arm_bind("000100100001", arm_branch_and_exchange);
    //arm_bind("00010x10xxx1", arm_branch_and_exchange?);
    arm_bind("00110010xxxx", arm_special_data_processing_immediate);
    arm_bind("00110110xxxx", arm_special_data_processing_immediate);
}

void thumb_init_lookup(void) {
    memset(thumb_lookup, 0, sizeof(void *) * 256);

    thumb_bind("000xxxxx", thumb_shift_by_immediate);
    thumb_bind("000110xx", thumb_add_or_subtract_register);
    thumb_bind("000111xx", thumb_add_or_subtract_immediate);
    thumb_bind("001xxxxx", thumb_add_subtract_compare_or_move_immediate);
    thumb_bind("010000xx", thumb_data_processing_register);
    thumb_bind("010001xx", thumb_special_data_processing);
    thumb_bind("01000111", thumb_branch_and_exchange);
    thumb_bind("01001xxx", thumb_load_from_literal_pool);
    thumb_bind("0101xxxx", thumb_load_store_register);
    thumb_bind("011xxxxx", thumb_load_store_word_or_byte_immediate);
    thumb_bind("1000xxxx", thumb_load_store_halfword_immediate);
    thumb_bind("1001xxxx", thumb_load_store_to_or_from_stack);
    thumb_bind("1010xxxx", thumb_add_to_sp_or_pc);
    thumb_bind("1011x0xx", thumb_adjust_stack_pointer);
    thumb_bind("1011x1xx", thumb_push_or_pop_register_list);
    thumb_bind("1100xxxx", thumb_load_store_multiple);
    thumb_bind("1101xxxx", thumb_conditional_branch);
    thumb_bind("11011110", thumb_undefined_instruction);
    thumb_bind("11011111", thumb_software_interrupt);
    thumb_bind("11100xxx", thumb_unconditional_branch);
    thumb_bind("11101xxx", thumb_undefined_instruction);
    thumb_bind("11110xxx", thumb_branch_with_link_prefix);
    thumb_bind("11111xxx", thumb_branch_with_link_suffix);
}
