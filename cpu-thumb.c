#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "cpu.h"

// LSL Rd, Rm, #<shift_imm>
// LSR Rd, Rm, #<shift_imm>
// ASR Rd, Rm, #<shift_imm>
void thumb_shift_by_immediate(void) {
    uint32_t opc = BITS(thumb_op, 11, 12);
    uint32_t imm = BITS(thumb_op, 6, 10);
    uint32_t Rm = BITS(thumb_op, 3, 5);
    uint32_t Rd = BITS(thumb_op, 0, 2);

#ifdef DEBUG
    if (log_instructions && log_thumb_instructions) {
        thumb_print_opcode();
        switch (opc) {
            case SHIFT_LSL: print_mnemonic("lsl"); break;
            case SHIFT_LSR: print_mnemonic("lsr"); break;
            case SHIFT_ASR: print_mnemonic("asr"); break;
        }
        print_register(Rd);
        printf(",");
        print_register(Rm);
        printf(",");
        print_immediate(imm);
        printf("\n");
    }
#endif

    arm_op = COND_AL << 28 | ARM_MOV << 21 | 0x01 << 20 | Rd << 12 | imm << 7 | Rm;
    switch (opc) {
        case SHIFT_LSL: arm_op |= SHIFT_LSL << 5; break;
        case SHIFT_LSR: arm_op |= SHIFT_LSR << 5; break;
        case SHIFT_ASR: arm_op |= SHIFT_ASR << 5; break;
    }
    arm_data_processing_register();
}

// ADD Rd, Rn, Rm
// SUB Rd, Rn, Rm
void thumb_add_or_subtract_register(void) {
    uint32_t opc = BIT(thumb_op, 9);
    uint32_t Rm = BITS(thumb_op, 6, 8);
    uint32_t Rn = BITS(thumb_op, 3, 5);
    uint32_t Rd = BITS(thumb_op, 0, 2);

#ifdef DEBUG
    if (log_instructions && log_thumb_instructions) {
        thumb_print_opcode();
        switch (opc) {
            case 0: print_mnemonic("add"); break;
            case 1: print_mnemonic("sub"); break;
        }
        print_register(Rd);
        printf(",");
        print_register(Rn);
        printf(",");
        print_register(Rm);
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

// ADD Rd, Rn, #<3_bit_immediate>
// SUB Rd, Rn, #<3_bit_immediate>
void thumb_add_or_subtract_immediate(void) {
    uint32_t opc = BIT(thumb_op, 9);
    uint32_t imm = BITS(thumb_op, 6, 8);
    uint32_t Rn = BITS(thumb_op, 3, 5);
    uint32_t Rd = BITS(thumb_op, 0, 2);

#ifdef DEBUG
    if (log_instructions && log_thumb_instructions) {
        thumb_print_opcode();
        switch (opc) {
            case 0: print_mnemonic("add"); break;
            case 1: print_mnemonic("sub"); break;
        }
        print_register(Rd);
        printf(",");
        print_register(Rn);
        printf(",");
        print_immediate(imm);
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

// MOV Rd, #<8_bit_immediate>
// CMP Rn, #<8_bit_immediate>
// ADD Rd, #<8_bit_immediate>
// SUB Rd, #<8_bit_immediate>
void thumb_add_subtract_compare_or_move_immediate(void) {
    uint32_t opc = BITS(thumb_op, 11, 12);
    uint32_t Rdn = BITS(thumb_op, 8, 10);
    uint32_t imm = BITS(thumb_op, 0, 7);

#ifdef DEBUG
    if (log_instructions && log_thumb_instructions) {
        thumb_print_opcode();
        switch (opc) {
            case 0: print_mnemonic("mov"); break;
            case 1: print_mnemonic("cmp"); break;
            case 2: print_mnemonic("add"); break;
            case 3: print_mnemonic("sub"); break;
        }
        print_register(Rdn);
        printf(",");
        print_immediate(imm);
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

// AND Rd, Rm
// EOR Rd, Rm
// LSL Rd, Rs
// LSR Rd, Rs
// ASR Rd, Rs
// ADC Rd, Rm
// SBC Rd, Rm
// ROR Rd, Rs
// TST Rn, Rm
// NEG Rd, Rm
// CMP Rn, Rm
// CMN Rn, Rm
// ORR Rd, Rm
// MUL Rd, Rm
// BIC Rd, Rm
// MVN Rd, Rm
void thumb_data_processing_register(void) {
    uint32_t opc = BITS(thumb_op, 6, 9);
    uint32_t Rms = BITS(thumb_op, 3, 5);
    uint32_t Rdn = BITS(thumb_op, 0, 2);

#ifdef DEBUG
    if (log_instructions && log_thumb_instructions) {
        thumb_print_opcode();
        switch (opc) {
            case THUMB_AND: print_mnemonic("and"); break;
            case THUMB_EOR: print_mnemonic("eor"); break;
            case THUMB_LSL: print_mnemonic("lsl"); break;
            case THUMB_LSR: print_mnemonic("lsr"); break;
            case THUMB_ASR: print_mnemonic("asr"); break;
            case THUMB_ADC: print_mnemonic("adc"); break;
            case THUMB_SBC: print_mnemonic("sbc"); break;
            case THUMB_ROR: print_mnemonic("ror"); break;
            case THUMB_TST: print_mnemonic("tst"); break;
            case THUMB_NEG: print_mnemonic("neg"); break;
            case THUMB_CMP: print_mnemonic("cmp"); break;
            case THUMB_CMN: print_mnemonic("cmn"); break;
            case THUMB_ORR: print_mnemonic("orr"); break;
            case THUMB_MUL: print_mnemonic("mul"); break;
            case THUMB_BIC: print_mnemonic("bic"); break;
            case THUMB_MVN: print_mnemonic("mvn"); break;
        }
        print_register(Rdn);
        printf(",");
        print_register(Rms);
        printf("\n");
    }
#endif

    arm_op = COND_AL << 28 | 0x01 << 20;
    switch (opc) {
        case THUMB_AND: arm_op |= ARM_AND << 21 | Rdn << 16 | Rdn << 12 | Rms; break;
        case THUMB_EOR: arm_op |= ARM_EOR << 21 | Rdn << 16 | Rdn << 12 | Rms; break;
        case THUMB_LSL: arm_op |= ARM_MOV << 21 | Rdn << 12 | Rms << 8 | SHIFT_LSL << 5 | 0x1 << 4 | Rdn; break;
        case THUMB_LSR: arm_op |= ARM_MOV << 21 | Rdn << 12 | Rms << 8 | SHIFT_LSR << 5 | 0x1 << 4 | Rdn; break;
        case THUMB_ASR: arm_op |= ARM_MOV << 21 | Rdn << 12 | Rms << 8 | SHIFT_ASR << 5 | 0x1 << 4 | Rdn; break;
        case THUMB_ADC: arm_op |= ARM_ADC << 21 | Rdn << 16 | Rdn << 12 | Rms; break;
        case THUMB_SBC: arm_op |= ARM_SBC << 21 | Rdn << 16 | Rdn << 12 | Rms; break;
        case THUMB_ROR: arm_op |= ARM_MOV << 21 | Rdn << 12 | Rms << 8 | SHIFT_ROR << 5 | 0x1 << 4 | Rdn; break;
        case THUMB_TST: arm_op |= ARM_TST << 21 | Rdn << 16 | Rms; break;
        case THUMB_NEG: arm_op |= ARM_RSB << 21 | 0x20 << 20 | Rms << 16 | Rdn << 12; break;
        case THUMB_CMP: arm_op |= ARM_CMP << 21 | Rdn << 16 | Rms; break;
        case THUMB_CMN: arm_op |= ARM_CMN << 21 | Rdn << 16 | Rms; break;
        case THUMB_ORR: arm_op |= ARM_ORR << 21 | Rdn << 16 | Rdn << 12 | Rms; break;
        case THUMB_MUL: arm_op |= Rdn << 16 | Rdn << 8 | 0x9 << 4 | Rms; break;
        case THUMB_BIC: arm_op |= ARM_BIC << 21 | Rdn << 16 | Rdn << 12 | Rms; break;
        case THUMB_MVN: arm_op |= ARM_MVN << 21 | Rdn << 12 | Rms; break;
    }
    if (opc == THUMB_MUL) {
        arm_multiply();
    } else if (opc == THUMB_NEG) {
        arm_data_processing_immediate();
    } else {
        arm_data_processing_register();
    }
}

// ADD Rd, Rm
// CMP Rn, Rm
// MOV Rd, Rm
void thumb_special_data_processing(void) {
    uint32_t opc = BITS(thumb_op, 8, 9);
    uint32_t Rm = BITS(thumb_op, 3, 6);
    uint32_t Rdn = BIT(thumb_op, 7) << 3 | BITS(thumb_op, 0, 2);

#ifdef DEBUG
    if (log_instructions && log_thumb_instructions) {
        thumb_print_opcode();
        switch (opc) {
            case 0: print_mnemonic("add"); break;
            case 1: print_mnemonic("cmp"); break;
            case 2: print_mnemonic("mov"); break;
        }
        print_register(Rdn);
        printf(",");
        print_register(Rm);
        printf("\n");
    }
#endif

    assert(!(Rm < 8 && Rdn < 8));  // unpredictable

    arm_op = COND_AL << 28 | Rm;
    switch (opc) {
        case 0: arm_op |= ARM_ADD << 21 | Rdn << 16 | Rdn << 12; break;
        case 1: arm_op |= ARM_CMP << 21 | 0x01 << 20 | Rdn << 16; break;
        case 2: arm_op |= ARM_MOV << 21 | Rdn << 12; break;
    }
    arm_data_processing_register();
}

// BX Rm
void thumb_branch_and_exchange(void) {
    bool L = BIT(thumb_op, 7);
    uint32_t Rm = BITS(thumb_op, 3, 6);
    uint32_t sbz = BITS(thumb_op, 0, 2);

#ifdef DEBUG
    if (log_instructions && log_thumb_instructions) {
        thumb_print_opcode();
        print_mnemonic(L ? "blx" : "bx");
        print_register(Rm);
        printf("\n");
    }
#endif

    assert(!L);  // unpredictable
    assert(sbz == 0);  // should be zero

    arm_op = COND_AL << 28 | 0x12 << 20 | 0xfff << 8 | 0x1 << 4 | Rm;
    arm_branch_and_exchange();
}

// LDR Rd, [PC, #8_bit_offset]
void thumb_load_from_literal_pool(void) {
    uint32_t Rd = BITS(thumb_op, 8, 10);
    uint32_t imm = BITS(thumb_op, 0, 7);

#ifdef DEBUG
    if (log_instructions && log_thumb_instructions) {
        thumb_print_opcode();
        print_mnemonic("ldr");
        print_register(Rd);
        printf(",");
        print_address((r[REG_PC] & ~3) + (imm << 2));
        printf("\n");
    }
#endif

    arm_op = COND_AL << 28 | 0x59 << 20 | REG_PC << 16 | Rd << 12 | imm << 2;
    arm_load_store_word_or_byte_immediate();
}

// STR Rd, [Rn, Rm]
// STRH Rd, [Rn, Rm]
// STRB Rd, [Rn, Rm]
// LDRSB Rd, [Rn, Rm]
// LDR Rd, [Rn, Rm]
// LDRH Rd, [Rn, Rm]
// LDRB Rd, [Rn, Rm]
// LDRSH Rd, [Rn, Rm]
void thumb_load_store_register(void) {
    uint32_t opc = BITS(thumb_op, 9, 11);
    uint32_t Rm = BITS(thumb_op, 6, 8);
    uint32_t Rn = BITS(thumb_op, 3, 5);
    uint32_t Rd = BITS(thumb_op, 0, 2);

#ifdef DEBUG
    if (log_instructions && log_thumb_instructions) {
        thumb_print_opcode();
        switch (opc) {
            case 0: print_mnemonic("str"); break;
            case 1: print_mnemonic("strh"); break;
            case 2: print_mnemonic("strb"); break;
            case 3: print_mnemonic("ldrsb"); break;
            case 4: print_mnemonic("ldr"); break;
            case 5: print_mnemonic("ldrh"); break;
            case 6: print_mnemonic("ldrb"); break;
            case 7: print_mnemonic("ldrsh"); break;
        }
        print_register(Rd);
        printf(",[");
        print_register(Rn);
        printf(",");
        print_register(Rm);
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
    }
    if (opc == 1 || opc == 5) {
        arm_load_store_halfword_register();
    } else if (opc == 3 || opc == 7) {
        arm_load_signed_halfword_or_signed_byte_register();
    } else {
        arm_load_store_word_or_byte_register();
    }
}

// STR Rd, [Rn, #5_bit_offset]
// LDR Rd, [Rn, #5_bit_offset]
// STRB Rd, [Rn, #5_bit_offset]
// LDRB Rd, [Rn, #5_bit_offset]
void thumb_load_store_word_or_byte_immediate(void) {
    bool B = BIT(thumb_op, 12);
    bool L = BIT(thumb_op, 11);
    uint32_t imm = BITS(thumb_op, 6, 10);
    uint32_t Rn = BITS(thumb_op, 3, 5);
    uint32_t Rd = BITS(thumb_op, 0, 2);

#ifdef DEBUG
    if (log_instructions && log_thumb_instructions) {
        thumb_print_opcode();
        if (L) {
            print_mnemonic(B ? "ldrb" : "ldr");
        } else {
            print_mnemonic(B ? "strb" : "str");
        }
        print_register(Rd);
        printf(",[");
        print_register(Rn);
        printf(",");
        print_immediate(imm);
        printf("]\n");
    }
#endif

    arm_op = COND_AL << 28 | 0x58 << 20 | Rn << 16 | Rd << 12;
    if (B) {
        arm_op |= 0x04 << 20 | imm;
    } else {
        arm_op |= imm << 2;
    }
    if (L) {
        arm_op |= 0x01 << 20;
    }
    arm_load_store_word_or_byte_immediate();
}

// STRH Rd, [Rn, #5_bit_offset]
// LDRH Rd, [Rn, #5_bit_offset]
void thumb_load_store_halfword_immediate(void) {
    bool L = BIT(thumb_op, 11);
    uint32_t imm = BITS(thumb_op, 6, 10);
    uint32_t Rn = BITS(thumb_op, 3, 5);
    uint32_t Rd = BITS(thumb_op, 0, 2);

#ifdef DEBUG
    if (log_instructions && log_thumb_instructions) {
        thumb_print_opcode();
        print_mnemonic(L ? "ldrh" : "strh");
        print_register(Rd);
        printf(",[");
        print_register(Rn);
        printf(",");
        print_immediate(imm);
        printf("]\n");
    }
#endif

    arm_op = COND_AL << 28 | Rn << 16 | Rd << 12 | BITS(imm, 3, 4) << 8 | 0xb << 4 | BITS(imm, 0, 2) << 1;
    if (L) {
        arm_op |= 0x1d << 20;
    } else {
        arm_op |= 0x1c << 20;
    }
    arm_load_store_halfword_immediate();
}

// STR Rd, [SP, #8_bit_offset]
// LDR Rd, [SP, #8_bit_offset]
void thumb_load_store_to_or_from_stack(void) {
    bool L = BIT(thumb_op, 11);
    uint32_t Rd = BITS(thumb_op, 8, 10);
    uint32_t imm = BITS(thumb_op, 0, 7);

#ifdef DEBUG
    if (log_instructions && log_thumb_instructions) {
        thumb_print_opcode();
        print_mnemonic(L ? "ldr" : "str");
        print_register(Rd);
        printf(",[");
        print_register(REG_SP);
        printf(",");
        print_immediate(imm);
        printf("]\n");
    }
#endif

    arm_op = COND_AL << 28 | REG_SP << 16 | Rd << 12 | imm << 2;
    if (L) {
        arm_op |= 0x59 << 20;
    } else {
        arm_op |= 0x58 << 20;
    }
    arm_load_store_word_or_byte_immediate();
}

// ADD Rd, PC, #<8_bit_immediate>
// ADD Rd, SP, #<8_bit_immediate>
void thumb_add_to_sp_or_pc(void) {
    bool SP = BIT(thumb_op, 11);
    uint32_t Rd = BITS(thumb_op, 8, 10);
    uint32_t imm = BITS(thumb_op, 0, 7);

#ifdef DEBUG
    if (log_instructions && log_thumb_instructions) {
        thumb_print_opcode();
        print_mnemonic("add");
        print_register(Rd);
        printf(",");
        print_register(SP ? REG_SP : REG_PC);
        printf(",");
        print_immediate(ror(imm, 30));
        printf("\n");
    }
#endif

    arm_op = COND_AL << 28 | ARM_ADD << 21 | 0x20 << 20 | Rd << 12 | 0xf << 8 | imm;
    if (SP) {
        arm_op |= REG_SP << 16;
    } else {
        arm_op |= REG_PC << 16;
    }
    arm_data_processing_immediate();
}

// ADD SP, SP, #<7_bit_immediate>
// SUB SP, SP, #<7_bit_immediate>
void thumb_adjust_stack_pointer(void) {
    uint32_t opc = BIT(thumb_op, 7);
    uint32_t imm = BITS(thumb_op, 0, 6);
    uint32_t sbz = BITS(thumb_op, 8, 11) & 0xb;

#ifdef DEBUG
    if (log_instructions && log_thumb_instructions) {
        thumb_print_opcode();
        print_mnemonic(opc == 1 ? "sub" : "add");
        print_register(REG_SP);
        printf(",");
        print_immediate(ror(imm, 30));
        printf("\n");
    }
#endif

    assert(sbz == 0);  // should be zero

    arm_op = COND_AL << 28 | 0x20 << 20 | REG_SP << 16 | REG_SP << 12 | 0xf << 8 | imm;
    if (opc == 1) {
        arm_op |= ARM_SUB << 21;
    } else {
        arm_op |= ARM_ADD << 21;
    }
    arm_data_processing_immediate();
}

// PUSH {<register_list>, <LR>}
// POP {<register_list>, <PC>}
void thumb_push_or_pop_register_list(void) {
    bool L = BIT(thumb_op, 11);
    bool R = BIT(thumb_op, 8);
    uint32_t rlist = BITS(thumb_op, 0, 7);
    uint32_t sbz = BIT(thumb_op, 9);

#ifdef DEBUG
    if (log_instructions && log_thumb_instructions) {
        thumb_print_opcode();
        print_mnemonic(L ? "pop" : "push");
        printf("{");
        bool first = thumb_print_rlist(rlist);
        if (R) {
            if (!first) printf(",");
            print_register(L ? REG_PC : REG_LR);
        }
        printf("}\n");
    }
#endif

    assert(sbz == 0);  // should be zero

    arm_op = COND_AL << 28 | REG_SP << 16 | rlist;
    if (L) {
        arm_op |= 0x8b << 20;
        if (R) arm_op |= 1 << REG_PC;
    } else {
        arm_op |= 0x92 << 20;
        if (R) arm_op |= 1 << REG_LR;
    }
    arm_load_store_multiple();
}

// STMIA Rn!, {<register_list>}
// LDMIA Rn!, {<register_list>}
void thumb_load_store_multiple(void) {
    bool L = BIT(thumb_op, 11);
    uint32_t Rn = BITS(thumb_op, 8, 10);
    uint32_t rlist = BITS(thumb_op, 0, 7);

#ifdef DEBUG
    if (log_instructions && log_thumb_instructions) {
        thumb_print_opcode();
        print_mnemonic(L ? "ldmia" : "stmia");
        print_register(Rn);
        printf("!,{");
        thumb_print_rlist(rlist);
        printf("}\n");
    }
#endif

    arm_op = COND_AL << 28 | Rn << 16 | rlist;
    if (L) {
        arm_op |= 0x8b << 20;
    } else {
        arm_op |= 0x8a << 20;
    }
    arm_load_store_multiple();
}

// B<cond> <target_address>
void thumb_conditional_branch(void) {
    uint32_t cond = BITS(thumb_op, 8, 11);
    uint32_t imm = BITS(thumb_op, 0, 7);
    ZERO_EXTEND(imm, 7);

#ifdef DEBUG
    if (log_instructions && log_thumb_instructions) {
        thumb_print_opcode();
        switch (cond) {
            case COND_EQ: print_mnemonic("beq"); break;
            case COND_NE: print_mnemonic("bne"); break;
            case COND_CS: print_mnemonic("bcs"); break;
            case COND_CC: print_mnemonic("bcc"); break;
            case COND_MI: print_mnemonic("bmi"); break;
            case COND_PL: print_mnemonic("bpl"); break;
            case COND_VS: print_mnemonic("bvs"); break;
            case COND_VC: print_mnemonic("bvc"); break;
            case COND_HI: print_mnemonic("bhi"); break;
            case COND_LS: print_mnemonic("bls"); break;
            case COND_GE: print_mnemonic("bge"); break;
            case COND_LT: print_mnemonic("blt"); break;
            case COND_GT: print_mnemonic("bgt"); break;
            case COND_LE: print_mnemonic("ble"); break;
        }
        print_address(r[REG_PC] + (imm << 1));
        printf("\n");
    }
#endif

    if (condition_passed(cond)) {
        r[REG_PC] += imm << 1;
        branch_taken = true;
    }
}

// SWI <8_bit_immediate>
void thumb_software_interrupt(void) {
    uint32_t imm = BITS(thumb_op, 0, 7);

#ifdef DEBUG
    if (log_instructions && log_thumb_instructions) {
        thumb_print_opcode();
        print_mnemonic("swi");
        print_address(imm);
        printf("\n");
    }
#endif

    arm_op = COND_AL << 28 | 0xf0 << 20 | imm;
    arm_software_interrupt();
}

// B <target_address>
void thumb_unconditional_branch(void) {
    uint32_t imm = BITS(thumb_op, 0, 10);
    ZERO_EXTEND(imm, 10);

#ifdef DEBUG
    if (log_instructions && log_thumb_instructions) {
        thumb_print_opcode();
        print_mnemonic("b");
        print_address(r[REG_PC] + (imm << 1));
        printf("\n");
    }
#endif

    r[REG_PC] += imm << 1;
    branch_taken = true;
}

// BL <target_address>
void thumb_branch_with_link_prefix(void) {
    uint32_t imm = BITS(thumb_op, 0, 10);
    ZERO_EXTEND(imm, 10);

#ifdef DEBUG
    if (log_instructions && log_thumb_instructions) {
        thumb_print_opcode();
        print_mnemonic("bl.1");
        printf("\n");
    }
#endif

    r[REG_LR] = r[REG_PC] + (imm << 12);
}

// BL <target_address>
void thumb_branch_with_link_suffix(void) {
    uint32_t imm = BITS(thumb_op, 0, 10);
    uint32_t target_address = r[REG_LR] + (imm << 1);

#ifdef DEBUG
    if (log_instructions && log_thumb_instructions) {
        thumb_print_opcode();
        print_mnemonic("bl.2");
        print_address(target_address);
        printf("\n");
    }
#endif

    r[REG_LR] = (r[REG_PC] - 2) | 1;
    r[REG_PC] = target_address;
    branch_taken = true;
}
