// Copyright (c) 2021 Ridge Shrubsall
// SPDX-License-Identifier: BSD-3-Clause

#include "cpu.h"

#include <stdint.h>
#include <bit>
#include <cassert>
#include <cstdlib>
#include <string>

#include "memory.h"

static uint64_t arm_alu_op(uint32_t opc, uint64_t n, uint64_t m) {
    switch (opc) {
        case ARM_AND: return n & m;
        case ARM_EOR: return n ^ m;
        case ARM_SUB: return n - m;
        case ARM_RSB: return m - n;
        case ARM_ADD: return n + m;
        case ARM_ADC: return n + m + FLAG_C();
        case ARM_SBC: return n - m - !FLAG_C();
        case ARM_RSC: return m - n - !FLAG_C();
        case ARM_TST: return n & m;
        case ARM_TEQ: return n ^ m;
        case ARM_CMP: return n - m;
        case ARM_CMN: return n + m;
        case ARM_ORR: return n | m;
        case ARM_MOV: return m;
        case ARM_BIC: return n & ~m;
        case ARM_MVN: return ~m;
        default: std::abort();
    }
}

static void arm_alu_flags(uint32_t opc, uint32_t n, uint32_t m, uint64_t result) {
    uint32_t result_lo = (uint32_t) result;
    uint32_t result_hi = (uint32_t) (result >> 32);
    bool sign_first = BIT(n, 31);
    bool sign_second = BIT(m, 31);
    bool sign_result = BIT(result_lo, 31);

    ASSIGN_N(sign_result);
    ASSIGN_Z(result_lo == 0);
    if (opc == ARM_ADD || opc == ARM_ADC || opc == ARM_CMN) {
        ASSIGN_C(result_hi != 0);
    } else if (opc == ARM_SUB || opc == ARM_SBC || opc == ARM_RSB || opc == ARM_RSC || opc == ARM_CMP) {
        ASSIGN_C(result_hi == 0);
    }
    if (opc == ARM_ADD || opc == ARM_ADC || opc == ARM_CMN) {
        ASSIGN_V(sign_first == sign_second && sign_first != sign_result);
    } else if (opc == ARM_SUB || opc == ARM_SBC || opc == ARM_CMP) {
        ASSIGN_V(sign_first != sign_second && sign_second == sign_result);
    } else if (opc == ARM_RSB || opc == ARM_RSC) {
        ASSIGN_V(sign_first != sign_second && sign_first == sign_result);
    }
}

static uint32_t arm_shifter_op(uint32_t m, uint32_t s, uint32_t shop, bool shreg, bool update_carry) {
    switch (shop) {
        case SHIFT_LSL:
            if (s >= 32) return 0;
            return m << s;

        case SHIFT_LSR:
            if (s >= 32) return 0;
            return m >> s;

        case SHIFT_ASR:
            if (s >= 32) return (BIT(m, 31) ? ~0 : 0);
            return ASR(m, s);

        case SHIFT_ROR:
            if (!shreg && s == 0) {  // ROR #0 -> RRX
                bool C = FLAG_C();
                if (update_carry) {
                    ASSIGN_C(BIT(m, 0));
                }
                return m >> 1 | (C ? (1 << 31) : 0);
            }
            return ROR(m, s & 31);

        default:
            std::abort();
    }
}

static void arm_shifter_flags(uint32_t m, uint32_t s, uint32_t shop) {
    switch (shop) {
        case SHIFT_LSL:
            if (s >= 1 && s <= 32) {
                ASSIGN_C(BIT(m, 32 - s));
            } else if (s > 32) {
                CLEAR_C();
            }
            break;

        case SHIFT_LSR:
            if (s >= 1 && s <= 32) {
                ASSIGN_C(BIT(m, s - 1));
            } else if (s > 32) {
                CLEAR_C();
            }
            break;

        case SHIFT_ASR:
            if (s >= 1 && s <= 32) {
                ASSIGN_C(BIT(m, s - 1));
            } else if (s > 32) {
                ASSIGN_C(BIT(m, 31));
            }
            break;

        case SHIFT_ROR:
            if (s > 0) {
                ASSIGN_C(BIT(m, s - 1));
            }
            break;

        default:
            std::abort();
    }
}

void arm_data_processing_register_disasm(uint32_t address, uint32_t op, std::string &s) {
    UNUSED(address);

    uint32_t opc = BITS(op, 21, 24);
    bool S = BIT(op, 20);
    uint32_t Rn = BITS(op, 16, 19);
    uint32_t Rd = BITS(op, 12, 15);
    uint32_t Rs = BITS(op, 8, 11);
    uint32_t shamt = BITS(op, 7, 11);
    uint32_t shop = BITS(op, 5, 6);
    bool shreg = BIT(op, 4);
    uint32_t Rm = BITS(op, 0, 3);

    if (!shreg && (shop == SHIFT_LSR || shop == SHIFT_ASR) && shamt == 0) {
        shamt = 32;  // LSR #0 -> LSR #32, ASR #0 -> ASR #32
    }

    bool is_test_or_compare = (opc == ARM_TST || opc == ARM_TEQ || opc == ARM_CMP || opc == ARM_CMN);
    bool is_move = (opc == ARM_MOV || opc == ARM_MVN);

    switch (opc) {
        case ARM_AND: s += (S ? "ands" : "and"); break;
        case ARM_EOR: s += (S ? "eors" : "eor"); break;
        case ARM_SUB: s += (S ? "subs" : "sub"); break;
        case ARM_RSB: s += (S ? "rsbs" : "rsb"); break;
        case ARM_ADD: s += (S ? "adds" : "add"); break;
        case ARM_ADC: s += (S ? "adcs" : "adc"); break;
        case ARM_SBC: s += (S ? "sbcs" : "sbc"); break;
        case ARM_RSC: s += (S ? "rscs" : "rsc"); break;
        case ARM_TST: s += (Rd == REG_PC ? "tstp" : "tst"); break;
        case ARM_TEQ: s += (Rd == REG_PC ? "teqp" : "teq"); break;
        case ARM_CMP: s += (Rd == REG_PC ? "cmpp" : "cmp"); break;
        case ARM_CMN: s += (Rd == REG_PC ? "cmnp" : "cmn"); break;
        case ARM_ORR: s += (S ? "orrs" : "orr"); break;
        case ARM_MOV: s += (S ? "movs" : "mov"); break;
        case ARM_BIC: s += (S ? "bics" : "bic"); break;
        case ARM_MVN: s += (S ? "mvns" : "mvn"); break;
        default: std::abort();
    }
    print_arm_condition(s, op);
    s += " ";
    if (!is_test_or_compare) {
        print_register(s, Rd);
        s += ", ";
    }
    if (!is_move) {
        print_register(s, Rn);
        s += ", ";
    }
    print_register(s, Rm);
    print_arm_shift(s, shop, shamt, shreg, Rs);
}

int arm_data_processing_register(uint32_t op) {
    uint32_t opc = BITS(op, 21, 24);
    bool S = BIT(op, 20);
    uint32_t Rn = BITS(op, 16, 19);
    uint32_t Rd = BITS(op, 12, 15);
    uint32_t Rs = BITS(op, 8, 11);
    uint32_t shamt = BITS(op, 7, 11);
    uint32_t shop = BITS(op, 5, 6);
    bool shreg = BIT(op, 4);
    uint32_t Rm = BITS(op, 0, 3);

    if (!shreg && (shop == SHIFT_LSR || shop == SHIFT_ASR) && shamt == 0) {
        shamt = 32;  // LSR #0 -> LSR #32, ASR #0 -> ASR #32
    }

    bool is_test_or_compare = (opc == ARM_TST || opc == ARM_TEQ || opc == ARM_CMP || opc == ARM_CMN);
    bool is_move = (opc == ARM_MOV || opc == ARM_MVN);

    if (is_test_or_compare) {
        assert(S == 1);
        assert(Rd == 0 || Rd == REG_PC);
    } else if (is_move) {
        assert(Rn == 0);
    }

    uint32_t s = (shreg ? (uint8_t) r[Rs] : shamt);  // Use least significant byte of Rs
    uint32_t m = r[Rm];
    if (Rm == REG_PC && shreg) m += SIZEOF_INSTR;  // PC ahead if shift by register
    m = arm_shifter_op(m, s, shop, shreg, true);
    uint32_t n = r[Rn];
    if (Rn == REG_PC && shreg) n += SIZEOF_INSTR;  // PC ahead if shift by register
    uint64_t result = arm_alu_op(opc, n, m);
    if (S) {
        arm_shifter_flags(r[Rm], s, shop);
        arm_alu_flags(opc, n, m, result);
    }
    if (!is_test_or_compare) {
        r[Rd] = (uint32_t) result;
        if (Rd == REG_PC) {  // PC altered
            r[Rd] &= ~1;
            branch_taken = true;
            if (S) write_cpsr(read_spsr());
        }
    } else {
        if (Rd == REG_PC) {           // ARMv2 mode change (obsolete)
            write_cpsr(read_spsr());  // Restores SPSR on ARMv4
        }
    }

    return 1;
}

void arm_data_processing_immediate_disasm(uint32_t address, uint32_t op, std::string &s) {
    UNUSED(address);

    uint32_t opc = BITS(op, 21, 24);
    bool S = BIT(op, 20);
    uint32_t Rn = BITS(op, 16, 19);
    uint32_t Rd = BITS(op, 12, 15);
    uint32_t rot = BITS(op, 8, 11);
    uint32_t imm = ROR(BITS(op, 0, 7), 2 * rot);

    bool is_test_or_compare = (opc == ARM_TST || opc == ARM_TEQ || opc == ARM_CMP || opc == ARM_CMN);
    bool is_move = (opc == ARM_MOV || opc == ARM_MVN);

    switch (opc) {
        case ARM_AND: s += (S ? "ands" : "and"); break;
        case ARM_EOR: s += (S ? "eors" : "eor"); break;
        case ARM_SUB: s += (S ? "subs" : "sub"); break;
        case ARM_RSB: s += (S ? "rsbs" : "rsb"); break;
        case ARM_ADD: s += (S ? "adds" : "add"); break;
        case ARM_ADC: s += (S ? "adcs" : "adc"); break;
        case ARM_SBC: s += (S ? "sbcs" : "sbc"); break;
        case ARM_RSC: s += (S ? "rscs" : "rsc"); break;
        case ARM_TST: s += "tst"; break;
        case ARM_TEQ: s += "teq"; break;
        case ARM_CMP: s += "cmp"; break;
        case ARM_CMN: s += "cmn"; break;
        case ARM_ORR: s += (S ? "orrs" : "orr"); break;
        case ARM_MOV: s += (S ? "movs" : "mov"); break;
        case ARM_BIC: s += (S ? "bics" : "bic"); break;
        case ARM_MVN: s += (S ? "mvns" : "mvn"); break;
        default: std::abort();
    }
    print_arm_condition(s, op);
    s += " ";
    if (!is_test_or_compare) {
        print_register(s, Rd);
        s += ", ";
    }
    if (!is_move) {
        print_register(s, Rn);
        s += ", ";
    }
    print_immediate(s, imm);
}

int arm_data_processing_immediate(uint32_t op) {
    uint32_t opc = BITS(op, 21, 24);
    bool S = BIT(op, 20);
    uint32_t Rn = BITS(op, 16, 19);
    uint32_t Rd = BITS(op, 12, 15);
    uint32_t rot = BITS(op, 8, 11);
    uint32_t imm = ROR(BITS(op, 0, 7), 2 * rot);

    bool is_test_or_compare = (opc == ARM_TST || opc == ARM_TEQ || opc == ARM_CMP || opc == ARM_CMN);
    bool is_move = (opc == ARM_MOV || opc == ARM_MVN);

    if (is_test_or_compare) {
        assert(S == 1);
        assert(Rd == 0);
    } else if (is_move) {
        assert(Rn == 0);
    }

    uint32_t n = r[Rn];
    if (Rn == REG_PC) n &= ~3;  // Align PC to word
    uint64_t result = arm_alu_op(opc, n, imm);
    if (S) {
        if (rot > 0) {
            ASSIGN_C(BIT(imm, 31));
        }
        arm_alu_flags(opc, n, imm, result);
    }
    if (!is_test_or_compare) {
        r[Rd] = (uint32_t) result;
        if (Rd == REG_PC) {  // PC altered
            r[Rd] &= ~1;
            branch_taken = true;
            if (S) write_cpsr(read_spsr());
        }
    }

    return 1;
}

void arm_load_store_word_or_byte_register_disasm(uint32_t address, uint32_t op, std::string &s) {
    UNUSED(address);

    bool P = BIT(op, 24);
    bool U = BIT(op, 23);
    bool B = BIT(op, 22);
    bool W = BIT(op, 21);
    bool L = BIT(op, 20);
    uint32_t Rn = BITS(op, 16, 19);
    uint32_t Rd = BITS(op, 12, 15);
    uint32_t shamt = BITS(op, 7, 11);
    uint32_t shop = BITS(op, 5, 6);
    uint32_t Rm = BITS(op, 0, 3);

    bool T = !P && W;

    if ((shop == SHIFT_LSR || shop == SHIFT_ASR) && shamt == 0) {
        shamt = 32;  // LSR #0 -> LSR #32, ASR #0 -> ASR #32
    }

    s += (L ? "ldr" : "str");
    if (B) s += "b";
    if (T) s += "t";
    print_arm_condition(s, op);
    s += " ";
    print_register(s, Rd);
    s += ", [";
    print_register(s, Rn);
    s += (P ? ", " : "], ");
    if (!U) s += "-";
    print_register(s, Rm);
    print_arm_shift(s, shop, shamt, false, 0);
    if (P) {
        s += "]";
        if (W) s += "!";
    }
}

int arm_load_store_word_or_byte_register(uint32_t op) {
    bool P = BIT(op, 24);
    bool U = BIT(op, 23);
    bool B = BIT(op, 22);
    bool W = BIT(op, 21);
    bool L = BIT(op, 20);
    uint32_t Rn = BITS(op, 16, 19);
    uint32_t Rd = BITS(op, 12, 15);
    uint32_t shamt = BITS(op, 7, 11);
    uint32_t shop = BITS(op, 5, 6);
    bool shreg = BIT(op, 4);
    uint32_t Rm = BITS(op, 0, 3);

    bool T = !P && W;
    assert(!T);

    assert(!shreg);
    if ((shop == SHIFT_LSR || shop == SHIFT_ASR) && shamt == 0) {
        shamt = 32;  // LSR #0 -> LSR #32, ASR #0 -> ASR #32
    }

    uint32_t m = arm_shifter_op(r[Rm], shamt, shop, false, false);
    uint32_t n = r[Rn];
    if (Rn == REG_PC) n &= ~3;
    if (P) n += (U ? m : -m);
    if (L) {
        if (B) {
            r[Rd] = memory_read_byte(n);
        } else {
            r[Rd] = align_word(n, memory_read_word(n));
        }
        if (Rd == REG_PC) branch_taken = true;
    } else {
        uint32_t d = r[Rd];
        if (Rd == REG_PC) d += SIZEOF_INSTR;
        if (B) {
            memory_write_byte(n, (uint8_t) d);
        } else {
            memory_write_word(n, d);
        }
    }
    if (!P) n += (U ? m : -m);
    if ((!P || W) && (!L || Rd != Rn)) r[Rn] = n;

    return 1;
}

void arm_load_store_word_or_byte_immediate_disasm(uint32_t address, uint32_t op, std::string &s) {
    UNUSED(address);

    bool P = BIT(op, 24);
    bool U = BIT(op, 23);
    bool B = BIT(op, 22);
    bool W = BIT(op, 21);
    bool L = BIT(op, 20);
    uint32_t Rn = BITS(op, 16, 19);
    uint32_t Rd = BITS(op, 12, 15);
    uint32_t imm = BITS(op, 0, 11);

    bool T = !P && W;

    s += (L ? "ldr" : "str");
    if (B) s += "b";
    if (T) s += "t";
    print_arm_condition(s, op);
    s += " ";
    print_register(s, Rd);
    s += ", [";
    print_register(s, Rn);
    s += (P ? ", " : "], ");
    print_immediate_signed(s, imm, !U);
    if (P) {
        s += "]";
        if (W) s += "!";
    }
}

int arm_load_store_word_or_byte_immediate(uint32_t op) {
    bool P = BIT(op, 24);
    bool U = BIT(op, 23);
    bool B = BIT(op, 22);
    bool W = BIT(op, 21);
    bool L = BIT(op, 20);
    uint32_t Rn = BITS(op, 16, 19);
    uint32_t Rd = BITS(op, 12, 15);
    uint32_t imm = BITS(op, 0, 11);

    bool T = !P && W;
    assert(!T);

    uint32_t n = r[Rn];
    if (Rn == REG_PC) n &= ~3;
    if (P) n += (U ? imm : -imm);
    if (L) {
        if (B) {
            r[Rd] = memory_read_byte(n);
        } else {
            r[Rd] = align_word(n, memory_read_word(n));
        }
        if (Rd == REG_PC) branch_taken = true;
    } else {
        uint32_t d = r[Rd];
        if (Rd == REG_PC) d += SIZEOF_INSTR;
        if (B) {
            memory_write_byte(n, (uint8_t) d);
        } else {
            memory_write_word(n, d);
        }
    }
    if (!P) n += (U ? imm : -imm);
    if ((!P || W) && (!L || Rd != Rn)) r[Rn] = n;

    return 1;
}

void arm_load_store_multiple_disasm(uint32_t address, uint32_t op, std::string &s) {
    UNUSED(address);

    bool P = BIT(op, 24);
    bool U = BIT(op, 23);
    bool S = BIT(op, 22);
    bool W = BIT(op, 21);
    bool L = BIT(op, 20);
    uint32_t Rn = BITS(op, 16, 19);
    uint32_t rlist = BITS(op, 0, 15);

    s += (L ? "ldm" : "stm");
    if (!P && !U) {
        s += "da";
    } else if (!P && U) {
        s += "ia";
    } else if (P && !U) {
        s += "db";
    } else if (P && U) {
        s += "ib";
    }
    print_arm_condition(s, op);
    s += " ";
    print_register(s, Rn);
    if (W) s += "!";
    s += ", {";
    print_arm_rlist(s, rlist);
    s += "}";
    if (S) s += "^";
}

int arm_load_store_multiple(uint32_t op) {
    bool P = BIT(op, 24);
    bool U = BIT(op, 23);
    bool S = BIT(op, 22);
    bool W = BIT(op, 21);
    bool L = BIT(op, 20);
    uint32_t Rn = BITS(op, 16, 19);
    uint32_t rlist = BITS(op, 0, 15);

    uint32_t count = std::popcount(rlist);
    if (rlist == 0) {  // Empty rlist
        rlist |= 1 << REG_PC;
        count = 16;
    }
    uint32_t old_base = r[Rn];
    uint32_t new_base = old_base + (U ? 4 : -4) * count;
    uint32_t address = old_base;
    if (!U) address -= 4 * count;
    if (U == P) address += 4;
    if (S) {
        assert((cpsr & PSR_MODE) != PSR_MODE_USR && (cpsr & PSR_MODE) != PSR_MODE_SYS);
        mode_change(cpsr & PSR_MODE, PSR_MODE_USR);
    }
    for (uint32_t i = 0; i < 16; i++) {
        if (BIT(rlist, i)) {
            if (L) {
                if (i == Rn) W = false;
                r[i] = memory_read_word(address);
                if (i == REG_PC) {  // PC altered
                    r[i] &= ~1;
                    branch_taken = true;
                    if (S) write_cpsr(read_spsr());
                }
            } else {
                if (i == REG_PC) {
                    memory_write_word(address, r[i] + SIZEOF_INSTR);
                } else if (i == Rn) {
                    if (std::countr_zero(rlist) == (int) Rn) {
                        memory_write_word(address, old_base);
                    } else {
                        memory_write_word(address, new_base);
                    }
                } else {
                    memory_write_word(address, r[i]);
                }
            }
            address += 4;
        }
    }
    //assert(!W || !S);
    if (S) mode_change(PSR_MODE_USR, cpsr & PSR_MODE);
    if (W) r[Rn] = new_base;  // FIXME before or after mode change?

    return 1;
}

void arm_branch_disasm(uint32_t address, uint32_t op, std::string &s) {
    bool L = BIT(op, 24);
    uint32_t imm = BITS(op, 0, 23);
    SIGN_EXTEND(imm, 23);

    s += (L ? "bl" : "b");
    print_arm_condition(s, op);
    s += " ";
    print_address(s, address + 8 + (imm << 2));
}

int arm_branch(uint32_t op) {
    bool L = BIT(op, 24);
    uint32_t imm = BITS(op, 0, 23);
    SIGN_EXTEND(imm, 23);

    if (L) r[REG_LR] = r[REG_PC] - 4;
    r[REG_PC] += imm << 2;
    branch_taken = true;

    return 1;
}

void arm_software_interrupt_disasm(uint32_t address, uint32_t op, std::string &s) {
    UNUSED(address);

    uint32_t imm = BITS(op, 0, 23);

    s += "swi";
    print_arm_condition(s, op);
    s += " ";
    print_bios_function_name(s, imm >> 16);
}

int arm_software_interrupt(uint32_t op) {
    UNUSED(op);

    r14_svc = r[REG_PC] - SIZEOF_INSTR;  // ARM: PC + 4, Thumb: PC + 2
    spsr_svc = cpsr;
    branch_taken = true;
    write_cpsr((cpsr & ~(PSR_T | PSR_MODE)) | PSR_I | PSR_MODE_SVC);
    r[REG_PC] = VEC_SWI;

    return 1;
}

int arm_hardware_interrupt() {
    r14_irq = r[REG_PC] - (FLAG_T() ? 0 : 4);  // ARM: PC + 4, Thumb: PC + 4
    spsr_irq = cpsr;
    branch_taken = true;
    write_cpsr((cpsr & ~(PSR_T | PSR_MODE)) | PSR_I | PSR_MODE_IRQ);
    r[REG_PC] = VEC_IRQ;

    return 1;
}

void arm_multiply_disasm(uint32_t address, uint32_t op, std::string &s) {
    UNUSED(address);

    bool A = BIT(op, 21);
    bool S = BIT(op, 20);
    uint32_t Rd = BITS(op, 16, 19);
    uint32_t Rn = BITS(op, 12, 15);
    uint32_t Rs = BITS(op, 8, 11);
    uint32_t Rm = BITS(op, 0, 3);

    s += (A ? "mla" : "mul");
    if (S) s += "s";
    print_arm_condition(s, op);
    s += " ";
    print_register(s, Rd);
    s += ", ";
    print_register(s, Rm);
    s += ", ";
    print_register(s, Rs);
    if (A) {
        s += ", ";
        print_register(s, Rn);
    }
}

int arm_multiply(uint32_t op) {
    bool A = BIT(op, 21);
    bool S = BIT(op, 20);
    uint32_t Rd = BITS(op, 16, 19);
    uint32_t Rn = BITS(op, 12, 15);
    uint32_t Rs = BITS(op, 8, 11);
    uint32_t Rm = BITS(op, 0, 3);

    if (!A) assert(Rn == 0);
    assert(Rd != REG_PC && Rm != REG_PC && Rs != REG_PC);

    uint32_t result = r[Rm] * r[Rs];
    if (A) result += r[Rn];
    r[Rd] = result;

    if (S) {
        ASSIGN_N(BIT(result, 31));
        ASSIGN_Z(result == 0);
    }

    return 1;
}

void arm_multiply_long_disasm(uint32_t address, uint32_t op, std::string &s) {
    UNUSED(address);

    bool U = BIT(op, 22);
    bool A = BIT(op, 21);
    bool S = BIT(op, 20);
    uint32_t RdHi = BITS(op, 16, 19);
    uint32_t RdLo = BITS(op, 12, 15);
    uint32_t Rs = BITS(op, 8, 11);
    uint32_t Rm = BITS(op, 0, 3);

    if (U) {
        s += (A ? "smlal" : "smull");
    } else {
        s += (A ? "umlal" : "umull");
    }
    if (S) s += "s";
    print_arm_condition(s, op);
    s += " ";
    print_register(s, RdLo);
    s += ", ";
    print_register(s, RdHi);
    s += ", ";
    print_register(s, Rm);
    s += ", ";
    print_register(s, Rs);
}

int arm_multiply_long(uint32_t op) {
    bool U = BIT(op, 22);
    bool A = BIT(op, 21);
    bool S = BIT(op, 20);
    uint32_t RdHi = BITS(op, 16, 19);
    uint32_t RdLo = BITS(op, 12, 15);
    uint32_t Rs = BITS(op, 8, 11);
    uint32_t Rm = BITS(op, 0, 3);

    assert(RdHi != REG_PC && RdLo != REG_PC && Rm != REG_PC && Rs != REG_PC);
    assert(RdHi != RdLo);

    uint64_t m = r[Rm];
    uint64_t s = r[Rs];
    if (U) {
        SIGN_EXTEND(m, 31);
        SIGN_EXTEND(s, 31);
    }
    uint64_t result = m * s;
    if (A) result += (uint64_t) r[RdLo] | (uint64_t) r[RdHi] << 32;
    r[RdLo] = (uint32_t) result;
    r[RdHi] = (uint32_t) (result >> 32);

    if (S) {
        ASSIGN_N(BIT(r[RdHi], 31));
        ASSIGN_Z(r[RdHi] == 0 && r[RdLo] == 0);
    }

    return 1;
}

void arm_load_store_halfword_register_disasm(uint32_t address, uint32_t op, std::string &s) {
    UNUSED(address);

    bool P = BIT(op, 24);
    bool U = BIT(op, 23);
    bool W = BIT(op, 21);
    bool L = BIT(op, 20);
    uint32_t Rn = BITS(op, 16, 19);
    uint32_t Rd = BITS(op, 12, 15);
    uint32_t Rm = BITS(op, 0, 3);

    s += (L ? "ldrh" : "strh");
    print_arm_condition(s, op);
    s += " ";
    print_register(s, Rd);
    s += ", [";
    print_register(s, Rn);
    s += (P ? ", " : "], ");
    if (!U) s += "-";
    print_register(s, Rm);
    if (P) {
        s += "]";
        if (W) s += "!";
    }
}

int arm_load_store_halfword_register(uint32_t op) {
    bool P = BIT(op, 24);
    bool U = BIT(op, 23);
    bool I = BIT(op, 22);
    bool W = BIT(op, 21);
    bool L = BIT(op, 20);
    uint32_t Rn = BITS(op, 16, 19);
    uint32_t Rd = BITS(op, 12, 15);
    uint32_t sbz = BITS(op, 8, 11);
    uint32_t opc = BITS(op, 4, 7);
    uint32_t Rm = BITS(op, 0, 3);

    bool T = !P && W;
    assert(!T);

    assert(!I);
    assert(sbz == 0);
    assert(opc == 0xb);

    uint32_t m = r[Rm];
    uint32_t n = r[Rn];
    if (Rn == REG_PC) n &= ~3;
    if (P) n += (U ? m : -m);
    if (L) {
        r[Rd] = align_halfword(n, memory_read_halfword(n));
        if (Rd == REG_PC) branch_taken = true;
    } else {
        uint32_t d = r[Rd];
        if (Rd == REG_PC) d += SIZEOF_INSTR;
        memory_write_halfword(n, (uint16_t) d);
    }
    if (!P) n += (U ? m : -m);
    if ((!P || W) && (!L || Rd != Rn)) r[Rn] = n;

    return 1;
}

void arm_load_store_halfword_immediate_disasm(uint32_t address, uint32_t op, std::string &s) {
    UNUSED(address);

    bool P = BIT(op, 24);
    bool U = BIT(op, 23);
    bool W = BIT(op, 21);
    bool L = BIT(op, 20);
    uint32_t Rn = BITS(op, 16, 19);
    uint32_t Rd = BITS(op, 12, 15);
    uint32_t imm = BITS(op, 8, 11) << 4 | BITS(op, 0, 3);

    s += (L ? "ldrh" : "strh");
    print_arm_condition(s, op);
    s += " ";
    print_register(s, Rd);
    s += ", [";
    print_register(s, Rn);
    s += (P ? ", " : "], ");
    print_immediate_signed(s, imm, !U);
    if (P) {
        s += "]";
        if (W) s += "!";
    }
}

int arm_load_store_halfword_immediate(uint32_t op) {
    bool P = BIT(op, 24);
    bool U = BIT(op, 23);
    bool I = BIT(op, 22);
    bool W = BIT(op, 21);
    bool L = BIT(op, 20);
    uint32_t Rn = BITS(op, 16, 19);
    uint32_t Rd = BITS(op, 12, 15);
    uint32_t opc = BITS(op, 4, 7);
    uint32_t imm = BITS(op, 8, 11) << 4 | BITS(op, 0, 3);

    bool T = !P && W;
    assert(!T);

    assert(I);
    assert(opc == 0xb);

    uint32_t n = r[Rn];
    if (Rn == REG_PC) n &= ~3;
    if (P) n += (U ? imm : -imm);
    if (L) {
        r[Rd] = align_halfword(n, memory_read_halfword(n));
        if (Rd == REG_PC) branch_taken = true;
    } else {
        uint32_t d = r[Rd];
        if (Rd == REG_PC) d += SIZEOF_INSTR;
        memory_write_halfword(n, (uint16_t) d);
    }
    if (!P) n += (U ? imm : -imm);
    if ((!P || W) && (!L || Rd != Rn)) r[Rn] = n;

    return 1;
}

void arm_load_signed_halfword_or_signed_byte_register_disasm(uint32_t address, uint32_t op, std::string &s) {
    UNUSED(address);

    bool P = BIT(op, 24);
    bool U = BIT(op, 23);
    bool W = BIT(op, 21);
    uint32_t Rn = BITS(op, 16, 19);
    uint32_t Rd = BITS(op, 12, 15);
    uint32_t opc = BITS(op, 4, 7);
    uint32_t Rm = BITS(op, 0, 3);

    switch (opc) {
        case 0xd: s += "ldrsb"; break;
        case 0xf: s += "ldrsh"; break;
        default: std::abort();
    }
    print_arm_condition(s, op);
    s += " ";
    print_register(s, Rd);
    s += ", [";
    print_register(s, Rn);
    s += (P ? ", " : "], ");
    if (!U) s += "-";
    print_register(s, Rm);
    if (P) {
        s += "]";
        if (W) s += "!";
    }
}

int arm_load_signed_halfword_or_signed_byte_register(uint32_t op) {
    bool P = BIT(op, 24);
    bool U = BIT(op, 23);
    bool I = BIT(op, 22);
    bool W = BIT(op, 21);
    bool L = BIT(op, 20);
    uint32_t Rn = BITS(op, 16, 19);
    uint32_t Rd = BITS(op, 12, 15);
    uint32_t sbz = BITS(op, 8, 11);
    uint32_t opc = BITS(op, 4, 7);
    uint32_t Rm = BITS(op, 0, 3);

    bool T = !P && W;
    assert(!T);

    assert(!I);
    assert(L);
    assert(sbz == 0);
    assert(opc == 0xd || opc == 0xf);

    uint32_t m = r[Rm];
    uint32_t n = r[Rn];
    if (Rn == REG_PC) n &= ~3;
    if (P) n += (U ? m : -m);
    if (opc == 0xd) {
        r[Rd] = memory_read_byte(n);
        if (r[Rd] & 0x80) r[Rd] |= ~0xff;
    } else if (opc == 0xf) {
        r[Rd] = align_halfword(n, memory_read_halfword(n));
        if ((n & 1) != 0) {
            r[Rd] &= 0xff;
            SIGN_EXTEND(r[Rd], 7);
        } else {
            r[Rd] &= 0xffff;
            SIGN_EXTEND(r[Rd], 15);
        }
    } else {
        assert(false);
    }
    if (Rd == REG_PC) branch_taken = true;
    if (!P) n += (U ? m : -m);
    if ((!P || W) && (!L || Rd != Rn)) r[Rn] = n;

    return 1;
}

void arm_load_signed_halfword_or_signed_byte_immediate_disasm(uint32_t address, uint32_t op, std::string &s) {
    UNUSED(address);

    bool P = BIT(op, 24);
    bool U = BIT(op, 23);
    bool W = BIT(op, 21);
    uint32_t Rn = BITS(op, 16, 19);
    uint32_t Rd = BITS(op, 12, 15);
    uint32_t opc = BITS(op, 4, 7);
    uint32_t imm = BITS(op, 8, 11) << 4 | BITS(op, 0, 3);

    switch (opc) {
        case 0xd: s += "ldrsb"; break;
        case 0xf: s += "ldrsh"; break;
        default: std::abort();
    }
    print_arm_condition(s, op);
    s += " ";
    print_register(s, Rd);
    s += ", [";
    print_register(s, Rn);
    s += (P ? ", " : "], ");
    print_immediate_signed(s, imm, !U);
    if (P) {
        s += "]";
        if (W) s += "!";
    }
}

int arm_load_signed_halfword_or_signed_byte_immediate(uint32_t op) {
    bool P = BIT(op, 24);
    bool U = BIT(op, 23);
    bool I = BIT(op, 22);
    bool W = BIT(op, 21);
    bool L = BIT(op, 20);
    uint32_t Rn = BITS(op, 16, 19);
    uint32_t Rd = BITS(op, 12, 15);
    uint32_t opc = BITS(op, 4, 7);
    uint32_t imm = BITS(op, 8, 11) << 4 | BITS(op, 0, 3);

    bool T = !P && W;
    assert(!T);

    assert(I);
    assert(L);
    assert(opc == 0xd || opc == 0xf);

    uint32_t n = r[Rn];
    if (Rn == REG_PC) n &= ~3;
    if (P) n += (U ? imm : -imm);
    if (opc == 0xd) {
        r[Rd] = memory_read_byte(n);
        if (r[Rd] & 0x80) r[Rd] |= ~0xff;
    } else if (opc == 0xf) {
        r[Rd] = align_halfword(n, memory_read_halfword(n));
        if ((n & 1) != 0) {
            r[Rd] &= 0xff;
            SIGN_EXTEND(r[Rd], 7);
        } else {
            r[Rd] &= 0xffff;
            SIGN_EXTEND(r[Rd], 15);
        }
    } else {
        assert(false);
    }
    if (Rd == REG_PC) branch_taken = true;
    if (!P) n += (U ? imm : -imm);
    if ((!P || W) && (!L || Rd != Rn)) r[Rn] = n;

    return 1;
}

void arm_special_data_processing_register_disasm(uint32_t address, uint32_t op, std::string &s) {
    UNUSED(address);

    bool R = BIT(op, 22);
    bool b21 = BIT(op, 21);
    uint32_t mask_type = BITS(op, 16, 19);
    uint32_t Rd = BITS(op, 12, 15);
    uint32_t Rm = BITS(op, 0, 3);

    s += (b21 ? "msr" : "mrs");
    print_arm_condition(s, op);
    s += " ";
    if (b21) {
        s += (R ? "spsr" : "cpsr");
        s += "_";
        if (mask_type == 0) s += "none";
        if (mask_type & 8) s += "f";  // Flags
        if (mask_type & 4) s += "s";  // Status
        if (mask_type & 2) s += "x";  // Extension
        if (mask_type & 1) s += "c";  // Control
        s += ", ";
        print_register(s, Rm);
    } else {
        print_register(s, Rd);
        s += ", ";
        s += (R ? "spsr" : "cpsr");
    }
}

int arm_special_data_processing_register(uint32_t op) {
    bool R = BIT(op, 22);
    bool b21 = BIT(op, 21);
    uint32_t mask_type = BITS(op, 16, 19);
    uint32_t Rd = BITS(op, 12, 15);
    uint32_t Rm = BITS(op, 0, 3);
    uint32_t sbz = BITS(op, 4, 11);

    if (b21) {
        assert(Rd == 0xf);
        assert(sbz == 0);
    } else {
        assert(mask_type == 0xf);
        assert(sbz == 0 && Rm == 0);
    }

    if (b21) {
        uint32_t mask = 0;
        if (mask_type & 8) mask |= 0xf0000000;
        if (mask_type & 1) mask |= 0x000000ff;
        if (R) {
            write_spsr((read_spsr() & ~mask) | (r[Rm] & mask));
        } else {
            write_cpsr((cpsr & ~mask) | (r[Rm] & mask));
        }
    } else {
        if (R) {
            r[Rd] = read_spsr();
        } else {
            r[Rd] = cpsr;
        }
        assert(Rd != REG_PC);
    }

    return 1;
}

void arm_special_data_processing_immediate_disasm(uint32_t address, uint32_t op, std::string &s) {
    UNUSED(address);

    bool R = BIT(op, 22);
    uint32_t mask_type = BITS(op, 16, 19);
    uint32_t rot = BITS(op, 8, 11);
    uint32_t imm = ROR(BITS(op, 0, 7), 2 * rot);

    s += "msr";
    print_arm_condition(s, op);
    s += " ";
    s += (R ? "spsr" : "cpsr");
    s += "_";
    if (mask_type == 0) s += "none";
    if (mask_type & 8) s += "f";  // Flags
    if (mask_type & 4) s += "s";  // Status
    if (mask_type & 2) s += "x";  // Extension
    if (mask_type & 1) s += "c";  // Control
    s += ", ";
    print_immediate(s, imm);
}

int arm_special_data_processing_immediate(uint32_t op) {
    bool R = BIT(op, 22);
    uint32_t mask_type = BITS(op, 16, 19);
    uint32_t sbo = BITS(op, 12, 15);
    uint32_t rot = BITS(op, 8, 11);
    uint32_t imm = ROR(BITS(op, 0, 7), 2 * rot);

    assert(sbo == 0xf);

    uint32_t mask = 0;
    if (mask_type & 8) mask |= 0xf0000000;
    if (mask_type & 1) mask |= 0x000000ff;
    if (R) {
        write_spsr((read_spsr() & ~mask) | (imm & mask));
    } else {
        write_cpsr((cpsr & ~mask) | (imm & mask));
    }

    return 1;
}

void arm_swap_disasm(uint32_t address, uint32_t op, std::string &s) {
    UNUSED(address);

    bool B = BIT(op, 22);
    uint32_t Rn = BITS(op, 16, 19);
    uint32_t Rd = BITS(op, 12, 15);
    uint32_t Rm = BITS(op, 0, 3);

    s += "swp";
    if (B) s += "b";
    print_arm_condition(s, op);
    s += " ";
    print_register(s, Rd);
    s += ", ";
    print_register(s, Rm);
    s += ", [";
    print_register(s, Rn);
    s += "]";
}

int arm_swap(uint32_t op) {
    bool B = BIT(op, 22);
    uint32_t Rn = BITS(op, 16, 19);
    uint32_t Rd = BITS(op, 12, 15);
    uint32_t sbz = BITS(op, 8, 11);
    uint32_t Rm = BITS(op, 0, 3);

    assert(sbz == 0);

    if (B) {
        uint8_t temp = memory_read_byte(r[Rn]);
        memory_write_byte(r[Rn], r[Rm]);
        r[Rd] = temp;
    } else {
        uint32_t temp = align_word(r[Rn], memory_read_word(r[Rn]));
        memory_write_word(r[Rn], r[Rm]);
        r[Rd] = temp;
    }
    assert(Rd != REG_PC);

    return 1;
}

void arm_branch_and_exchange_disasm(uint32_t address, uint32_t op, std::string &s) {
    UNUSED(address);

    uint32_t Rm = BITS(op, 0, 3);

    s += "bx";
    print_arm_condition(s, op);
    s += " ";
    print_register(s, Rm);
}

int arm_branch_and_exchange(uint32_t op) {
    uint32_t sbo = BITS(op, 8, 19);
    uint32_t Rm = BITS(op, 0, 3);

    assert(sbo == 0xfff);

    ASSIGN_T(BIT(r[Rm], 0));
    if (FLAG_T()) {
        r[REG_PC] = r[Rm] & ~1;
    } else {
        r[REG_PC] = r[Rm] & ~3;
    }
    branch_taken = true;

    return 1;
}

void arm_coprocessor_load_store_disasm(uint32_t address, uint32_t op, std::string &s) {
    UNUSED(address);

    bool L = BIT(op, 20);

    s += (L ? "ldc" : "stc");
    print_arm_condition(s, op);
    s += " ???";
}

int arm_coprocessor_load_store(uint32_t op) {
    UNUSED(op);

    assert(false);
    return 1;
}

void arm_coprocessor_data_processing_disasm(uint32_t address, uint32_t op, std::string &s) {
    UNUSED(address);

    bool L = BIT(op, 20);

    if (BIT(op, 4)) {
        s += (L ? "mrc" : "mcr");
    } else {
        s += "cdp";
    }
    print_arm_condition(s, op);
    s += " ???";
}

int arm_coprocessor_data_processing(uint32_t op) {
    UNUSED(op);

    assert(false);
    return 1;
}

void arm_undefined_instruction_disasm(uint32_t address, uint32_t op, std::string &s) {
    UNUSED(address);
    UNUSED(op);

    s += "undefined";
}

int arm_undefined_instruction(uint32_t op) {
    UNUSED(op);

    assert(false);

    r14_und = r[REG_PC] - SIZEOF_INSTR;  // ARM: PC + 4, Thumb: PC + 2
    spsr_und = cpsr;
    write_cpsr((cpsr & ~(PSR_T | PSR_MODE)) | PSR_I | PSR_MODE_UND);
    r[REG_PC] = VEC_UNDEF;
    branch_taken = true;

    return 1;
}
