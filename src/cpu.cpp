#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cpu.h"

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
int (*arm_lookup[4096])(uint32_t);
void (*arm_lookup_disasm[4096])(uint32_t, uint32_t, char *);

uint16_t thumb_op;
uint16_t thumb_pipeline[2];
int (*thumb_lookup[256])(uint16_t);
void (*thumb_lookup_disasm[256])(uint32_t, uint16_t, char *);

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

uint32_t get_pc(void) {
    return r[15] - (branch_taken ? 0 : 2 * SIZEOF_INSTR);
}

void arm_disasm(uint32_t address, uint32_t op, char *s) {
    uint32_t index = BITS(op, 20, 27) << 4 | BITS(op, 4, 7);
    void (*handler)(uint32_t, uint32_t, char *) = arm_lookup_disasm[index];
    assert(handler != NULL);
    (*handler)(address, op, s);
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

    uint32_t cond = BITS(arm_op, 28, 31);
    int cycles = 1;
    if (condition_passed(cond)) {
        uint32_t index = BITS(arm_op, 20, 27) << 4 | BITS(arm_op, 4, 7);
        int (*handler)(uint32_t) = arm_lookup[index];
        assert(handler != NULL);
        cycles = (*handler)(arm_op);
    }

    if (!branch_taken) {
        r[15] += 4;
    }

    return cycles;
}

void thumb_disasm(uint32_t address, uint16_t op, char *s) {
    uint16_t index = BITS(op, 8, 15);
    void (*handler)(uint32_t, uint16_t, char *) = thumb_lookup_disasm[index];
    assert(handler != NULL);
    (*handler)(address, op, s);
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

    uint16_t index = BITS(thumb_op, 8, 15);
    int (*handler)(uint16_t) = thumb_lookup[index];
    assert(handler != NULL);
    int cycles = (*handler)(thumb_op);

    if (!branch_taken) {
        r[15] += 2;
    }

    return cycles;
}

void lookup_bind(void (**lookup)(void), const char *mask, void (*f)(void)) {
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

void arm_bind(const char *mask, int (*execute)(uint32_t), void (*disasm)(uint32_t, uint32_t, char *)) {
    lookup_bind((void (**)()) arm_lookup, mask, (void (*)()) execute);
    lookup_bind((void (**)()) arm_lookup_disasm, mask, (void (*)()) disasm);
}

void thumb_bind(const char *mask, int (*execute)(uint16_t), void (*disasm)(uint32_t, uint16_t, char *)) {
    lookup_bind((void (**)()) thumb_lookup, mask, (void (*)()) execute);
    lookup_bind((void (**)()) thumb_lookup_disasm, mask, (void (*)()) disasm);
}

void arm_init_lookup(void) {
    memset(arm_lookup, 0, sizeof(void *) * 4096);

#define BIND(mask, f) arm_bind(mask, f, f##_disasm)

    BIND("000xxxxxxxxx", arm_data_processing_register);
    BIND("001xxxxxxxxx", arm_data_processing_immediate);
    BIND("010xxxxxxxxx", arm_load_store_word_or_byte_immediate);
    BIND("011xxxxxxxx0", arm_load_store_word_or_byte_register);
    BIND("011xxxxxxxx1", arm_undefined_instruction);
    BIND("100xxxxxxxxx", arm_load_store_multiple);
    BIND("101xxxxxxxxx", arm_branch);
    BIND("110xxxxxxxxx", arm_coprocessor_load_store);
    BIND("1110xxxxxxxx", arm_coprocessor_data_processing);
    BIND("1111xxxxxxxx", arm_software_interrupt);

    BIND("00010x00xxxx", arm_special_data_processing_register);  // must be before ldrh/strh
    BIND("00010x10xxx0", arm_special_data_processing_register);
    BIND("000000xx1001", arm_multiply);
    BIND("00001xxx1001", arm_multiply_long);
    BIND("000xx0xx1011", arm_load_store_halfword_register);
    BIND("000xx1xx1011", arm_load_store_halfword_immediate);
    BIND("000xx0xx11x1", arm_load_signed_halfword_or_signed_byte_register);
    BIND("000xx1xx11x1", arm_load_signed_halfword_or_signed_byte_immediate);
    BIND("00010x001001", arm_swap);
    BIND("000100100001", arm_branch_and_exchange);
  //BIND("00010x10xxx1", arm_branch_and_exchange);
    BIND("00110x10xxxx", arm_special_data_processing_immediate);

#undef BIND
}

void thumb_init_lookup(void) {
    memset(thumb_lookup, 0, sizeof(void *) * 256);

#define BIND(mask, f) thumb_bind(mask, f, f##_disasm)

    BIND("000xxxxx", thumb_shift_by_immediate);
    BIND("000110xx", thumb_add_or_subtract_register);
    BIND("000111xx", thumb_add_or_subtract_immediate);
    BIND("001xxxxx", thumb_add_subtract_compare_or_move_immediate);
    BIND("010000xx", thumb_data_processing_register);
    BIND("010001xx", thumb_special_data_processing);
    BIND("01000111", thumb_branch_and_exchange);
    BIND("01001xxx", thumb_load_from_literal_pool);
    BIND("0101xxxx", thumb_load_store_register);
    BIND("011xxxxx", thumb_load_store_word_or_byte_immediate);
    BIND("1000xxxx", thumb_load_store_halfword_immediate);
    BIND("1001xxxx", thumb_load_store_to_or_from_stack);
    BIND("1010xxxx", thumb_add_to_sp_or_pc);
    BIND("1011x0xx", thumb_adjust_stack_pointer);
    BIND("1011x1xx", thumb_push_or_pop_register_list);
    BIND("1100xxxx", thumb_load_store_multiple);
    BIND("1101xxxx", thumb_conditional_branch);
    BIND("11011110", thumb_undefined_instruction);
    BIND("11011111", thumb_software_interrupt);
    BIND("11100xxx", thumb_unconditional_branch);
    BIND("11101xxx", thumb_undefined_instruction);
    BIND("11110xxx", thumb_branch_with_link_prefix);
    BIND("11111xxx", thumb_branch_with_link_suffix);

#undef BIND
}

void print_arm_condition(char *s, uint32_t op) {
    switch (BITS(op, 28, 31)) {
        case COND_EQ: strcat(s, "eq"); break;
        case COND_NE: strcat(s, "ne"); break;
        case COND_CS: strcat(s, "cs"); break;
        case COND_CC: strcat(s, "cc"); break;
        case COND_MI: strcat(s, "mi"); break;
        case COND_PL: strcat(s, "pl"); break;
        case COND_VS: strcat(s, "vs"); break;
        case COND_VC: strcat(s, "vc"); break;
        case COND_HI: strcat(s, "hi"); break;
        case COND_LS: strcat(s, "ls"); break;
        case COND_GE: strcat(s, "ge"); break;
        case COND_LT: strcat(s, "lt"); break;
        case COND_GT: strcat(s, "gt"); break;
        case COND_LE: strcat(s, "le"); break;
    }
}

void print_register(char *s, uint32_t i) {
    char temp[4];

    switch (i) {
        case 13: strcpy(temp, "sp"); break;
        case 14: strcpy(temp, "lr"); break;
        case 15: strcpy(temp, "pc"); break;
        default: sprintf(temp, "r%d", i); break;
    }
    strcat(s, temp);
}

void print_immediate(char *s, uint32_t i) {
    char temp[12];

    if (i > 9) {
        sprintf(temp, "#0x%X", i);
    } else {
        sprintf(temp, "#%d", i);
    }
    strcat(s, temp);
}

void print_address(char *s, uint32_t i) {
    char temp[11];

    sprintf(temp, "0x%08X", i);
    strcat(s, temp);
}

static void print_shift_amount(char *s, uint32_t i) {
    char temp[12];

    sprintf(temp, "#%d", i);
    strcat(s, temp);
}

void print_arm_shift(char *s, uint32_t shop, uint32_t shamt, uint32_t shreg, uint32_t Rs) {
    switch (shop) {
        case SHIFT_LSL:
            if (shreg) {
                strcat(s, ", lsl ");
                print_register(s, Rs);
            } else if (shamt != 0) {
                strcat(s, ", lsl ");
                print_shift_amount(s, shamt);
            }
            break;

        case SHIFT_LSR:
            strcat(s, ", lsr ");
            if (shreg) {
                print_register(s, Rs);
            } else {
                print_shift_amount(s, shamt);
            }
            break;

        case SHIFT_ASR:
            strcat(s, ", asr ");
            if (shreg) {
                print_register(s, Rs);
            } else {
                print_shift_amount(s, shamt);
            }
            break;

        case SHIFT_ROR:
            if (shreg) {
                strcat(s, ", ror ");
                print_register(s, Rs);
            } else if (shamt != 0) {
                strcat(s, ", ror ");
                print_shift_amount(s, shamt);
            } else {
                strcat(s, ", rrx");
            }
            break;

        default:
            assert(false);
            break;
    }
}

static bool print_rlist(char *s, uint32_t rlist, uint32_t max) {
    bool first = true;
    uint32_t i = 0;
    while (i < max) {
        if (BIT(rlist, i)) {
            uint32_t j = i + 1;
            while (BIT(rlist, j)) j++;
            if (j == i + 1) {
                if (!first) strcat(s, ",");
                print_register(s, i);
            } else if (j == i + 2) {
                if (!first) strcat(s, ",");
                print_register(s, i);
                strcat(s, ",");
                print_register(s, j - 1);
            } else {
                if (!first) strcat(s, ",");
                print_register(s, i);
                strcat(s, "-");
                print_register(s, j - 1);
            }
            i = j;
            first = false;
        }
        i++;
    }
    return first;
}

bool print_arm_rlist(char *s, uint32_t rlist) {
    return print_rlist(s, rlist, 16);
}

bool print_thumb_rlist(char *s, uint32_t rlist) {
    return print_rlist(s, rlist, 8);
}
