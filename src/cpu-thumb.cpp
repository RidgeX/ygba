// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#include "cpu.h"

#include <stdint.h>
#include <cassert>
#include <cstdlib>
#include <string>

#include <fmt/core.h>

#include "memory.h"

void thumb_shift_by_immediate_disasm(uint32_t address, uint16_t op, std::string &s) {
    UNUSED(address);

    uint32_t opc = BITS(op, 11, 12);
    uint32_t imm = BITS(op, 6, 10);
    uint32_t Rm = BITS(op, 3, 5);
    uint32_t Rd = BITS(op, 0, 2);

    switch (opc) {
        case SHIFT_LSL: s += "lsl"; break;
        case SHIFT_LSR: s += "lsr"; break;
        case SHIFT_ASR: s += "asr"; break;
        default: std::abort();
    }
    s += " ";
    print_register(s, Rd);
    s += ", ";
    print_register(s, Rm);
    s += ", ";
    print_immediate(s, imm);
}

void thumb_shift_by_immediate(uint16_t op) {
    uint32_t opc = BITS(op, 11, 12);
    uint32_t imm = BITS(op, 6, 10);
    uint32_t Rm = BITS(op, 3, 5);
    uint32_t Rd = BITS(op, 0, 2);

    arm_op = COND_AL << 28 | ARM_MOV << 21 | 0x01 << 20 | Rd << 12 | imm << 7 | Rm;
    switch (opc) {
        case SHIFT_LSL: arm_op |= SHIFT_LSL << 5; break;
        case SHIFT_LSR: arm_op |= SHIFT_LSR << 5; break;
        case SHIFT_ASR: arm_op |= SHIFT_ASR << 5; break;
        default: std::abort();
    }
    arm_data_processing_register(arm_op);
}

void thumb_add_or_subtract_register_disasm(uint32_t address, uint16_t op, std::string &s) {
    UNUSED(address);

    uint32_t opc = BIT(op, 9);
    uint32_t Rm = BITS(op, 6, 8);
    uint32_t Rn = BITS(op, 3, 5);
    uint32_t Rd = BITS(op, 0, 2);

    switch (opc) {
        case 0: s += "add"; break;
        case 1: s += "sub"; break;
        default: std::abort();
    }
    s += " ";
    print_register(s, Rd);
    s += ", ";
    print_register(s, Rn);
    s += ", ";
    print_register(s, Rm);
}

void thumb_add_or_subtract_register(uint16_t op) {
    uint32_t opc = BIT(op, 9);
    uint32_t Rm = BITS(op, 6, 8);
    uint32_t Rn = BITS(op, 3, 5);
    uint32_t Rd = BITS(op, 0, 2);

    arm_op = COND_AL << 28 | 0x01 << 20 | Rn << 16 | Rd << 12 | Rm;
    switch (opc) {
        case 0: arm_op |= ARM_ADD << 21; break;
        case 1: arm_op |= ARM_SUB << 21; break;
        default: std::abort();
    }
    arm_data_processing_register(arm_op);
}

void thumb_add_or_subtract_immediate_disasm(uint32_t address, uint16_t op, std::string &s) {
    UNUSED(address);

    uint32_t opc = BIT(op, 9);
    uint32_t imm = BITS(op, 6, 8);
    uint32_t Rn = BITS(op, 3, 5);
    uint32_t Rd = BITS(op, 0, 2);

    switch (opc) {
        case 0: s += "add"; break;
        case 1: s += "sub"; break;
        default: std::abort();
    }
    s += " ";
    print_register(s, Rd);
    s += ", ";
    print_register(s, Rn);
    s += ", ";
    print_immediate(s, imm);
}

void thumb_add_or_subtract_immediate(uint16_t op) {
    uint32_t opc = BIT(op, 9);
    uint32_t imm = BITS(op, 6, 8);
    uint32_t Rn = BITS(op, 3, 5);
    uint32_t Rd = BITS(op, 0, 2);

    arm_op = COND_AL << 28 | 0x21 << 20 | Rn << 16 | Rd << 12 | imm;
    switch (opc) {
        case 0: arm_op |= ARM_ADD << 21; break;
        case 1: arm_op |= ARM_SUB << 21; break;
        default: std::abort();
    }
    arm_data_processing_immediate(arm_op);
}

void thumb_add_subtract_compare_or_move_immediate_disasm(uint32_t address, uint16_t op, std::string &s) {
    UNUSED(address);

    uint32_t opc = BITS(op, 11, 12);
    uint32_t Rdn = BITS(op, 8, 10);
    uint32_t imm = BITS(op, 0, 7);

    switch (opc) {
        case 0: s += "mov"; break;
        case 1: s += "cmp"; break;
        case 2: s += "add"; break;
        case 3: s += "sub"; break;
        default: std::abort();
    }
    s += " ";
    print_register(s, Rdn);
    s += ", ";
    print_immediate(s, imm);
}

void thumb_add_subtract_compare_or_move_immediate(uint16_t op) {
    uint32_t opc = BITS(op, 11, 12);
    uint32_t Rdn = BITS(op, 8, 10);
    uint32_t imm = BITS(op, 0, 7);

    arm_op = COND_AL << 28 | 0x21 << 20 | imm;
    switch (opc) {
        case 0: arm_op |= ARM_MOV << 21 | Rdn << 12; break;
        case 1: arm_op |= ARM_CMP << 21 | Rdn << 16; break;
        case 2: arm_op |= ARM_ADD << 21 | Rdn << 16 | Rdn << 12; break;
        case 3: arm_op |= ARM_SUB << 21 | Rdn << 16 | Rdn << 12; break;
        default: std::abort();
    }
    arm_data_processing_immediate(arm_op);
}

void thumb_data_processing_register_disasm(uint32_t address, uint16_t op, std::string &s) {
    UNUSED(address);

    uint32_t opc = BITS(op, 6, 9);
    uint32_t Rms = BITS(op, 3, 5);
    uint32_t Rdn = BITS(op, 0, 2);

    switch (opc) {
        case THUMB_AND: s += "and"; break;
        case THUMB_EOR: s += "eor"; break;
        case THUMB_LSL: s += "lsl"; break;
        case THUMB_LSR: s += "lsr"; break;
        case THUMB_ASR: s += "asr"; break;
        case THUMB_ADC: s += "adc"; break;
        case THUMB_SBC: s += "sbc"; break;
        case THUMB_ROR: s += "ror"; break;
        case THUMB_TST: s += "tst"; break;
        case THUMB_NEG: s += "neg"; break;
        case THUMB_CMP: s += "cmp"; break;
        case THUMB_CMN: s += "cmn"; break;
        case THUMB_ORR: s += "orr"; break;
        case THUMB_MUL: s += "mul"; break;
        case THUMB_BIC: s += "bic"; break;
        case THUMB_MVN: s += "mvn"; break;
        default: std::abort();
    }
    s += " ";
    print_register(s, Rdn);
    s += ", ";
    print_register(s, Rms);
}

void thumb_data_processing_register(uint16_t op) {
    uint32_t opc = BITS(op, 6, 9);
    uint32_t Rms = BITS(op, 3, 5);
    uint32_t Rdn = BITS(op, 0, 2);

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
        default: std::abort();
    }
    if (opc == THUMB_MUL) {
        arm_multiply(arm_op);
    } else if (opc == THUMB_NEG) {
        arm_data_processing_immediate(arm_op);
    } else {
        arm_data_processing_register(arm_op);
    }
}

void thumb_special_data_processing_disasm(uint32_t address, uint16_t op, std::string &s) {
    UNUSED(address);

    uint32_t opc = BITS(op, 8, 9);
    uint32_t Rm = BITS(op, 3, 6);
    uint32_t Rdn = BIT(op, 7) << 3 | BITS(op, 0, 2);

    switch (opc) {
        case 0: s += "add"; break;
        case 1: s += "cmp"; break;
        case 2: s += "mov"; break;
        default: std::abort();
    }
    s += " ";
    print_register(s, Rdn);
    s += ", ";
    print_register(s, Rm);
}

void thumb_special_data_processing(uint16_t op) {
    uint32_t opc = BITS(op, 8, 9);
    uint32_t Rm = BITS(op, 3, 6);
    uint32_t Rdn = BIT(op, 7) << 3 | BITS(op, 0, 2);

    assert(!(Rm < 8 && Rdn < 8));  // unpredictable

    arm_op = COND_AL << 28 | Rm;
    switch (opc) {
        case 0: arm_op |= ARM_ADD << 21 | Rdn << 16 | Rdn << 12; break;
        case 1: arm_op |= ARM_CMP << 21 | 0x01 << 20 | Rdn << 16; break;
        case 2: arm_op |= ARM_MOV << 21 | Rdn << 12; break;
        default: std::abort();
    }
    arm_data_processing_register(arm_op);
}

void thumb_branch_and_exchange_disasm(uint32_t address, uint16_t op, std::string &s) {
    UNUSED(address);

    bool L = BIT(op, 7);
    uint32_t Rm = BITS(op, 3, 6);

    s += (L ? "blx" : "bx");
    s += " ";
    print_register(s, Rm);
}

void thumb_branch_and_exchange(uint16_t op) {
    bool L = BIT(op, 7);
    uint32_t Rm = BITS(op, 3, 6);
    uint32_t sbz = BITS(op, 0, 2);

    assert(!L);        // unpredictable
    assert(sbz == 0);  // should be zero

    arm_op = COND_AL << 28 | 0x12 << 20 | 0xfff << 8 | 0x1 << 4 | Rm;
    arm_branch_and_exchange(arm_op);
}

void thumb_load_from_literal_pool_disasm(uint32_t address, uint16_t op, std::string &s) {
    uint32_t Rd = BITS(op, 8, 10);
    uint32_t imm = BITS(op, 0, 7);

    s += "ldr";
    s += " ";
    print_register(s, Rd);
    s += ", ";
    uint32_t pc_rel_address = (address & ~3) + 4 + (imm << 2);
    s += fmt::format("=0x{:08X}", memory_read_word(pc_rel_address));
    s += "  @ ";
    s += fmt::format("0x{:08X}", pc_rel_address);
}

void thumb_load_from_literal_pool(uint16_t op) {
    uint32_t Rd = BITS(op, 8, 10);
    uint32_t imm = BITS(op, 0, 7);

    arm_op = COND_AL << 28 | 0x59 << 20 | REG_PC << 16 | Rd << 12 | imm << 2;
    arm_load_store_word_or_byte_immediate(arm_op);
}

void thumb_load_store_register_disasm(uint32_t address, uint16_t op, std::string &s) {
    UNUSED(address);

    uint32_t opc = BITS(op, 9, 11);
    uint32_t Rm = BITS(op, 6, 8);
    uint32_t Rn = BITS(op, 3, 5);
    uint32_t Rd = BITS(op, 0, 2);

    switch (opc) {
        case 0: s += "str"; break;
        case 1: s += "strh"; break;
        case 2: s += "strb"; break;
        case 3: s += "ldrsb"; break;
        case 4: s += "ldr"; break;
        case 5: s += "ldrh"; break;
        case 6: s += "ldrb"; break;
        case 7: s += "ldrsh"; break;
        default: std::abort();
    }
    s += " ";
    print_register(s, Rd);
    s += ", [";
    print_register(s, Rn);
    s += ", ";
    print_register(s, Rm);
    s += "]";
}

void thumb_load_store_register(uint16_t op) {
    uint32_t opc = BITS(op, 9, 11);
    uint32_t Rm = BITS(op, 6, 8);
    uint32_t Rn = BITS(op, 3, 5);
    uint32_t Rd = BITS(op, 0, 2);

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
        default: std::abort();
    }
    if (opc == 1 || opc == 5) {
        arm_load_store_halfword_register(arm_op);
    } else if (opc == 3 || opc == 7) {
        arm_load_signed_halfword_or_signed_byte_register(arm_op);
    } else {
        arm_load_store_word_or_byte_register(arm_op);
    }
}

void thumb_load_store_word_or_byte_immediate_disasm(uint32_t address, uint16_t op, std::string &s) {
    UNUSED(address);

    bool B = BIT(op, 12);
    bool L = BIT(op, 11);
    uint32_t imm = BITS(op, 6, 10);
    uint32_t Rn = BITS(op, 3, 5);
    uint32_t Rd = BITS(op, 0, 2);

    s += (L ? "ldr" : "str");
    if (B) s += "b";
    s += " ";
    print_register(s, Rd);
    s += ", [";
    print_register(s, Rn);
    s += ", ";
    print_immediate(s, B ? imm : imm << 2);
    s += "]";
}

void thumb_load_store_word_or_byte_immediate(uint16_t op) {
    bool B = BIT(op, 12);
    bool L = BIT(op, 11);
    uint32_t imm = BITS(op, 6, 10);
    uint32_t Rn = BITS(op, 3, 5);
    uint32_t Rd = BITS(op, 0, 2);

    arm_op = COND_AL << 28 | 0x58 << 20 | Rn << 16 | Rd << 12;
    if (B) {
        arm_op |= 0x04 << 20 | imm;
    } else {
        arm_op |= imm << 2;
    }
    if (L) {
        arm_op |= 0x01 << 20;
    }
    arm_load_store_word_or_byte_immediate(arm_op);
}

void thumb_load_store_halfword_immediate_disasm(uint32_t address, uint16_t op, std::string &s) {
    UNUSED(address);

    bool L = BIT(op, 11);
    uint32_t imm = BITS(op, 6, 10);
    uint32_t Rn = BITS(op, 3, 5);
    uint32_t Rd = BITS(op, 0, 2);

    s += (L ? "ldrh" : "strh");
    s += " ";
    print_register(s, Rd);
    s += ", [";
    print_register(s, Rn);
    s += ", ";
    print_immediate(s, imm << 1);
    s += "]";
}

void thumb_load_store_halfword_immediate(uint16_t op) {
    bool L = BIT(op, 11);
    uint32_t imm = BITS(op, 6, 10);
    uint32_t Rn = BITS(op, 3, 5);
    uint32_t Rd = BITS(op, 0, 2);

    arm_op = COND_AL << 28 | Rn << 16 | Rd << 12 | BITS(imm, 3, 4) << 8 | 0xb << 4 | BITS(imm, 0, 2) << 1;
    if (L) {
        arm_op |= 0x1d << 20;
    } else {
        arm_op |= 0x1c << 20;
    }
    arm_load_store_halfword_immediate(arm_op);
}

void thumb_load_store_to_or_from_stack_disasm(uint32_t address, uint16_t op, std::string &s) {
    UNUSED(address);

    bool L = BIT(op, 11);
    uint32_t Rd = BITS(op, 8, 10);
    uint32_t imm = BITS(op, 0, 7);

    s += (L ? "ldr" : "str");
    s += " ";
    print_register(s, Rd);
    s += ", [";
    print_register(s, REG_SP);
    s += ", ";
    print_immediate(s, imm << 2);
    s += "]";
}

void thumb_load_store_to_or_from_stack(uint16_t op) {
    bool L = BIT(op, 11);
    uint32_t Rd = BITS(op, 8, 10);
    uint32_t imm = BITS(op, 0, 7);

    arm_op = COND_AL << 28 | REG_SP << 16 | Rd << 12 | imm << 2;
    if (L) {
        arm_op |= 0x59 << 20;
    } else {
        arm_op |= 0x58 << 20;
    }
    arm_load_store_word_or_byte_immediate(arm_op);
}

void thumb_add_to_sp_or_pc_disasm(uint32_t address, uint16_t op, std::string &s) {
    UNUSED(address);

    bool SP = BIT(op, 11);
    uint32_t Rd = BITS(op, 8, 10);
    uint32_t imm = BITS(op, 0, 7);

    s += "add";
    s += " ";
    print_register(s, Rd);
    s += ", ";
    print_register(s, SP ? REG_SP : REG_PC);
    s += ", ";
    print_immediate(s, ROR(imm, 30));
}

void thumb_add_to_sp_or_pc(uint16_t op) {
    bool SP = BIT(op, 11);
    uint32_t Rd = BITS(op, 8, 10);
    uint32_t imm = BITS(op, 0, 7);

    arm_op = COND_AL << 28 | ARM_ADD << 21 | 0x20 << 20 | Rd << 12 | 0xf << 8 | imm;
    if (SP) {
        arm_op |= REG_SP << 16;
    } else {
        arm_op |= REG_PC << 16;
    }
    arm_data_processing_immediate(arm_op);
}

void thumb_adjust_stack_pointer_disasm(uint32_t address, uint16_t op, std::string &s) {
    UNUSED(address);

    uint32_t opc = BIT(op, 7);
    uint32_t imm = BITS(op, 0, 6);

    switch (opc) {
        case 0: s += "add"; break;
        case 1: s += "sub"; break;
        default: std::abort();
    }
    s += " ";
    print_register(s, REG_SP);
    s += ", ";
    print_immediate(s, ROR(imm, 30));
}

void thumb_adjust_stack_pointer(uint16_t op) {
    uint32_t opc = BIT(op, 7);
    uint32_t imm = BITS(op, 0, 6);
    uint32_t sbz = BITS(op, 8, 11) & 0xb;

    assert(sbz == 0);  // should be zero

    arm_op = COND_AL << 28 | 0x20 << 20 | REG_SP << 16 | REG_SP << 12 | 0xf << 8 | imm;
    if (opc == 1) {
        arm_op |= ARM_SUB << 21;
    } else {
        arm_op |= ARM_ADD << 21;
    }
    arm_data_processing_immediate(arm_op);
}

void thumb_push_or_pop_register_list_disasm(uint32_t address, uint16_t op, std::string &s) {
    UNUSED(address);

    bool L = BIT(op, 11);
    bool R = BIT(op, 8);
    uint32_t rlist = BITS(op, 0, 7);

    s += (L ? "pop" : "push");
    s += " {";
    bool first = print_thumb_rlist(s, rlist);
    if (R) {
        if (!first) s += ", ";
        print_register(s, L ? REG_PC : REG_LR);
    }
    s += "}";
}

void thumb_push_or_pop_register_list(uint16_t op) {
    bool L = BIT(op, 11);
    bool R = BIT(op, 8);
    uint32_t rlist = BITS(op, 0, 7);
    uint32_t sbz = BIT(op, 9);

    assert(sbz == 0);  // should be zero

    arm_op = COND_AL << 28 | REG_SP << 16 | rlist;
    if (L) {
        arm_op |= 0x8b << 20;
        if (R) arm_op |= 1 << REG_PC;
    } else {
        arm_op |= 0x92 << 20;
        if (R) arm_op |= 1 << REG_LR;
    }
    arm_load_store_multiple(arm_op);
}

void thumb_load_store_multiple_disasm(uint32_t address, uint16_t op, std::string &s) {
    UNUSED(address);

    bool L = BIT(op, 11);
    uint32_t Rn = BITS(op, 8, 10);
    uint32_t rlist = BITS(op, 0, 7);

    s += (L ? "ldmia" : "stmia");
    s += " ";
    print_register(s, Rn);
    s += "!, {";
    print_thumb_rlist(s, rlist);
    s += "}";
}

void thumb_load_store_multiple(uint16_t op) {
    bool L = BIT(op, 11);
    uint32_t Rn = BITS(op, 8, 10);
    uint32_t rlist = BITS(op, 0, 7);

    arm_op = COND_AL << 28 | Rn << 16 | rlist;
    if (L) {
        arm_op |= 0x8b << 20;
    } else {
        arm_op |= 0x8a << 20;
    }
    arm_load_store_multiple(arm_op);
}

void thumb_conditional_branch_disasm(uint32_t address, uint16_t op, std::string &s) {
    uint32_t cond = BITS(op, 8, 11);
    uint32_t imm = BITS(op, 0, 7);
    SIGN_EXTEND(imm, 7);

    switch (cond) {
        case COND_EQ: s += "beq"; break;
        case COND_NE: s += "bne"; break;
        case COND_CS: s += "bcs"; break;
        case COND_CC: s += "bcc"; break;
        case COND_MI: s += "bmi"; break;
        case COND_PL: s += "bpl"; break;
        case COND_VS: s += "bvs"; break;
        case COND_VC: s += "bvc"; break;
        case COND_HI: s += "bhi"; break;
        case COND_LS: s += "bls"; break;
        case COND_GE: s += "bge"; break;
        case COND_LT: s += "blt"; break;
        case COND_GT: s += "bgt"; break;
        case COND_LE: s += "ble"; break;
        default: std::abort();
    }
    s += " ";
    uint32_t pc_rel_address = address + 4 + (imm << 1);
    print_address(s, pc_rel_address);
}

void thumb_conditional_branch(uint16_t op) {
    uint32_t cond = BITS(op, 8, 11);
    uint32_t imm = BITS(op, 0, 7);
    SIGN_EXTEND(imm, 7);

    if (condition_passed(cond)) {
        r[REG_PC] += imm << 1;
        branch_taken = true;
    }
}

void thumb_software_interrupt_disasm(uint32_t address, uint16_t op, std::string &s) {
    UNUSED(address);

    uint32_t imm = BITS(op, 0, 7);

    s += "swi";
    s += " ";
    print_bios_function_name(s, imm);
}

void thumb_software_interrupt(uint16_t op) {
    uint32_t imm = BITS(op, 0, 7);

    arm_op = COND_AL << 28 | 0xf0 << 20 | imm;
    arm_software_interrupt(arm_op);
}

void thumb_unconditional_branch_disasm(uint32_t address, uint16_t op, std::string &s) {
    uint32_t imm = BITS(op, 0, 10);
    SIGN_EXTEND(imm, 10);

    s += "b";
    s += " ";
    uint32_t pc_rel_address = address + 4 + (imm << 1);
    print_address(s, pc_rel_address);
}

void thumb_unconditional_branch(uint16_t op) {
    uint32_t imm = BITS(op, 0, 10);
    SIGN_EXTEND(imm, 10);

    r[REG_PC] += imm << 1;
    branch_taken = true;
}

void thumb_branch_with_link_prefix_disasm(uint32_t address, uint16_t op, std::string &s) {
    UNUSED(address);
    UNUSED(op);

    s += "bl";
}

void thumb_branch_with_link_prefix(uint16_t op) {
    uint32_t imm = BITS(op, 0, 10);
    SIGN_EXTEND(imm, 10);

    r[REG_LR] = r[REG_PC] + (imm << 12);
}

void thumb_branch_with_link_suffix_disasm(uint32_t address, uint16_t op, std::string &s) {
    uint16_t op_last = memory_read_halfword(address - 2);
    uint32_t imm_last = BITS(op_last, 0, 10);
    SIGN_EXTEND(imm_last, 10);
    uint32_t imm = BITS(op, 0, 10);

    s += "bl";
    s += " ";
    if ((op_last & 0xf800) == 0xf000) {
        print_address(s, address + 2 + (imm_last << 12) + (imm << 1));
    } else {
        s += "lr + ";
        print_immediate(s, imm << 1);
    }
}

void thumb_branch_with_link_suffix(uint16_t op) {
    uint32_t imm = BITS(op, 0, 10);
    uint32_t target_address = r[REG_LR] + (imm << 1);

    r[REG_LR] = (r[REG_PC] - 2) | 1;
    r[REG_PC] = target_address;
    branch_taken = true;
}

void thumb_undefined_instruction_disasm(uint32_t address, uint16_t op, std::string &s) {
    UNUSED(address);
    UNUSED(op);

    s += "undefined";
}

void thumb_undefined_instruction(uint16_t op) {
    UNUSED(op);

    assert(false);
    arm_op = COND_AL << 28 | 0x7f << 20 | 0xf << 4;  // Permanently undefined
    arm_undefined_instruction(arm_op);
}
