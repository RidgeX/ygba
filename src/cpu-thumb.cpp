#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "cpu.h"

int thumb_shift_by_immediate(void) {
    uint32_t opc = BITS(thumb_op, 11, 12);
    uint32_t imm = BITS(thumb_op, 6, 10);
    uint32_t Rm = BITS(thumb_op, 3, 5);
    uint32_t Rd = BITS(thumb_op, 0, 2);

    arm_op = COND_AL << 28 | ARM_MOV << 21 | 0x01 << 20 | Rd << 12 | imm << 7 | Rm;
    switch (opc) {
        case SHIFT_LSL: arm_op |= SHIFT_LSL << 5; break;
        case SHIFT_LSR: arm_op |= SHIFT_LSR << 5; break;
        case SHIFT_ASR: arm_op |= SHIFT_ASR << 5; break;
    }
    return arm_data_processing_register();
}

int thumb_add_or_subtract_register(void) {
    uint32_t opc = BIT(thumb_op, 9);
    uint32_t Rm = BITS(thumb_op, 6, 8);
    uint32_t Rn = BITS(thumb_op, 3, 5);
    uint32_t Rd = BITS(thumb_op, 0, 2);

    arm_op = COND_AL << 28 | 0x01 << 20 | Rn << 16 | Rd << 12 | Rm;
    switch (opc) {
        case 0: arm_op |= ARM_ADD << 21; break;
        case 1: arm_op |= ARM_SUB << 21; break;
    }
    return arm_data_processing_register();
}

int thumb_add_or_subtract_immediate(void) {
    uint32_t opc = BIT(thumb_op, 9);
    uint32_t imm = BITS(thumb_op, 6, 8);
    uint32_t Rn = BITS(thumb_op, 3, 5);
    uint32_t Rd = BITS(thumb_op, 0, 2);

    arm_op = COND_AL << 28 | 0x21 << 20 | Rn << 16 | Rd << 12 | imm;
    switch (opc) {
        case 0: arm_op |= ARM_ADD << 21; break;
        case 1: arm_op |= ARM_SUB << 21; break;
    }
    return arm_data_processing_immediate();
}

int thumb_add_subtract_compare_or_move_immediate(void) {
    uint32_t opc = BITS(thumb_op, 11, 12);
    uint32_t Rdn = BITS(thumb_op, 8, 10);
    uint32_t imm = BITS(thumb_op, 0, 7);

    arm_op = COND_AL << 28 | 0x21 << 20 | imm;
    switch (opc) {
        case 0: arm_op |= ARM_MOV << 21 | Rdn << 12; break;
        case 1: arm_op |= ARM_CMP << 21 | Rdn << 16; break;
        case 2: arm_op |= ARM_ADD << 21 | Rdn << 16 | Rdn << 12; break;
        case 3: arm_op |= ARM_SUB << 21 | Rdn << 16 | Rdn << 12; break;
    }
    return arm_data_processing_immediate();
}

int thumb_data_processing_register(void) {
    uint32_t opc = BITS(thumb_op, 6, 9);
    uint32_t Rms = BITS(thumb_op, 3, 5);
    uint32_t Rdn = BITS(thumb_op, 0, 2);

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
        return arm_multiply();
    } else if (opc == THUMB_NEG) {
        return arm_data_processing_immediate();
    } else {
        return arm_data_processing_register();
    }
}

int thumb_special_data_processing(void) {
    uint32_t opc = BITS(thumb_op, 8, 9);
    uint32_t Rm = BITS(thumb_op, 3, 6);
    uint32_t Rdn = BIT(thumb_op, 7) << 3 | BITS(thumb_op, 0, 2);

    assert(!(Rm < 8 && Rdn < 8));  // unpredictable

    arm_op = COND_AL << 28 | Rm;
    switch (opc) {
        case 0: arm_op |= ARM_ADD << 21 | Rdn << 16 | Rdn << 12; break;
        case 1: arm_op |= ARM_CMP << 21 | 0x01 << 20 | Rdn << 16; break;
        case 2: arm_op |= ARM_MOV << 21 | Rdn << 12; break;
    }
    return arm_data_processing_register();
}

int thumb_branch_and_exchange(void) {
    bool L = BIT(thumb_op, 7);
    uint32_t Rm = BITS(thumb_op, 3, 6);
    uint32_t sbz = BITS(thumb_op, 0, 2);

    assert(!L);  // unpredictable
    assert(sbz == 0);  // should be zero

    arm_op = COND_AL << 28 | 0x12 << 20 | 0xfff << 8 | 0x1 << 4 | Rm;
    return arm_branch_and_exchange();
}

int thumb_load_from_literal_pool(void) {
    uint32_t Rd = BITS(thumb_op, 8, 10);
    uint32_t imm = BITS(thumb_op, 0, 7);

    arm_op = COND_AL << 28 | 0x59 << 20 | REG_PC << 16 | Rd << 12 | imm << 2;
    return arm_load_store_word_or_byte_immediate();
}

int thumb_load_store_register(void) {
    uint32_t opc = BITS(thumb_op, 9, 11);
    uint32_t Rm = BITS(thumb_op, 6, 8);
    uint32_t Rn = BITS(thumb_op, 3, 5);
    uint32_t Rd = BITS(thumb_op, 0, 2);

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
        return arm_load_store_halfword_register();
    } else if (opc == 3 || opc == 7) {
        return arm_load_signed_halfword_or_signed_byte_register();
    } else {
        return arm_load_store_word_or_byte_register();
    }
}

int thumb_load_store_word_or_byte_immediate(void) {
    bool B = BIT(thumb_op, 12);
    bool L = BIT(thumb_op, 11);
    uint32_t imm = BITS(thumb_op, 6, 10);
    uint32_t Rn = BITS(thumb_op, 3, 5);
    uint32_t Rd = BITS(thumb_op, 0, 2);

    arm_op = COND_AL << 28 | 0x58 << 20 | Rn << 16 | Rd << 12;
    if (B) {
        arm_op |= 0x04 << 20 | imm;
    } else {
        arm_op |= imm << 2;
    }
    if (L) {
        arm_op |= 0x01 << 20;
    }
    return arm_load_store_word_or_byte_immediate();
}

int thumb_load_store_halfword_immediate(void) {
    bool L = BIT(thumb_op, 11);
    uint32_t imm = BITS(thumb_op, 6, 10);
    uint32_t Rn = BITS(thumb_op, 3, 5);
    uint32_t Rd = BITS(thumb_op, 0, 2);

    arm_op = COND_AL << 28 | Rn << 16 | Rd << 12 | BITS(imm, 3, 4) << 8 | 0xb << 4 | BITS(imm, 0, 2) << 1;
    if (L) {
        arm_op |= 0x1d << 20;
    } else {
        arm_op |= 0x1c << 20;
    }
    return arm_load_store_halfword_immediate();
}

int thumb_load_store_to_or_from_stack(void) {
    bool L = BIT(thumb_op, 11);
    uint32_t Rd = BITS(thumb_op, 8, 10);
    uint32_t imm = BITS(thumb_op, 0, 7);

    arm_op = COND_AL << 28 | REG_SP << 16 | Rd << 12 | imm << 2;
    if (L) {
        arm_op |= 0x59 << 20;
    } else {
        arm_op |= 0x58 << 20;
    }
    return arm_load_store_word_or_byte_immediate();
}

int thumb_add_to_sp_or_pc(void) {
    bool SP = BIT(thumb_op, 11);
    uint32_t Rd = BITS(thumb_op, 8, 10);
    uint32_t imm = BITS(thumb_op, 0, 7);

    arm_op = COND_AL << 28 | ARM_ADD << 21 | 0x20 << 20 | Rd << 12 | 0xf << 8 | imm;
    if (SP) {
        arm_op |= REG_SP << 16;
    } else {
        arm_op |= REG_PC << 16;
    }
    return arm_data_processing_immediate();
}

int thumb_adjust_stack_pointer(void) {
    uint32_t opc = BIT(thumb_op, 7);
    uint32_t imm = BITS(thumb_op, 0, 6);
    uint32_t sbz = BITS(thumb_op, 8, 11) & 0xb;

    assert(sbz == 0);  // should be zero

    arm_op = COND_AL << 28 | 0x20 << 20 | REG_SP << 16 | REG_SP << 12 | 0xf << 8 | imm;
    if (opc == 1) {
        arm_op |= ARM_SUB << 21;
    } else {
        arm_op |= ARM_ADD << 21;
    }
    return arm_data_processing_immediate();
}

int thumb_push_or_pop_register_list(void) {
    bool L = BIT(thumb_op, 11);
    bool R = BIT(thumb_op, 8);
    uint32_t rlist = BITS(thumb_op, 0, 7);
    uint32_t sbz = BIT(thumb_op, 9);

    assert(sbz == 0);  // should be zero

    arm_op = COND_AL << 28 | REG_SP << 16 | rlist;
    if (L) {
        arm_op |= 0x8b << 20;
        if (R) arm_op |= 1 << REG_PC;
    } else {
        arm_op |= 0x92 << 20;
        if (R) arm_op |= 1 << REG_LR;
    }
    return arm_load_store_multiple();
}

int thumb_load_store_multiple(void) {
    bool L = BIT(thumb_op, 11);
    uint32_t Rn = BITS(thumb_op, 8, 10);
    uint32_t rlist = BITS(thumb_op, 0, 7);

    arm_op = COND_AL << 28 | Rn << 16 | rlist;
    if (L) {
        arm_op |= 0x8b << 20;
    } else {
        arm_op |= 0x8a << 20;
    }
    return arm_load_store_multiple();
}

int thumb_conditional_branch(void) {
    uint32_t cond = BITS(thumb_op, 8, 11);
    uint32_t imm = BITS(thumb_op, 0, 7);
    ZERO_EXTEND(imm, 7);

    if (condition_passed(cond)) {
        r[REG_PC] += imm << 1;
        branch_taken = true;
    }

    return 1;
}

int thumb_software_interrupt(void) {
    uint32_t imm = BITS(thumb_op, 0, 7);

    arm_op = COND_AL << 28 | 0xf0 << 20 | imm;
    return arm_software_interrupt();
}

int thumb_unconditional_branch(void) {
    uint32_t imm = BITS(thumb_op, 0, 10);
    ZERO_EXTEND(imm, 10);

    r[REG_PC] += imm << 1;
    branch_taken = true;

    return 1;
}

int thumb_branch_with_link_prefix(void) {
    uint32_t imm = BITS(thumb_op, 0, 10);
    ZERO_EXTEND(imm, 10);

    r[REG_LR] = r[REG_PC] + (imm << 12);

    return 1;
}

int thumb_branch_with_link_suffix(void) {
    uint32_t imm = BITS(thumb_op, 0, 10);
    uint32_t target_address = r[REG_LR] + (imm << 1);

    r[REG_LR] = (r[REG_PC] - 2) | 1;
    r[REG_PC] = target_address;
    branch_taken = true;

    return 1;
}
