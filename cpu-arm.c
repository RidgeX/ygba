#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "cpu.h"

static uint64_t arm_alu_op(uint32_t opc, uint64_t n, uint64_t m) {
    switch (opc) {
        case ARM_AND: return n & m;
        case ARM_EOR: return n ^ m;
        case ARM_SUB: return n - m;
        case ARM_RSB: return m - n;
        case ARM_ADD: return n + m;
        case ARM_ADC: return n + m + (cpsr & PSR_C ? 1 : 0);
        case ARM_SBC: return n - m - (cpsr & PSR_C ? 0 : 1);
        case ARM_RSC: return m - n - (cpsr & PSR_C ? 0 : 1);
        case ARM_TST: return n & m;
        case ARM_TEQ: return n ^ m;
        case ARM_CMP: return n - m;
        case ARM_CMN: return n + m;
        case ARM_ORR: return n | m;
        case ARM_MOV: return m;
        case ARM_BIC: return n & ~m;
        case ARM_MVN: return ~m;
    }
    assert(false);
    return 0;
}

static void arm_alu_flags(uint32_t opc, uint64_t result, uint32_t n, uint32_t m) {
    if ((result & 0x80000000) != 0) { cpsr |= PSR_N; } else { cpsr &= ~PSR_N; }
    if ((result & 0xffffffff) == 0) { cpsr |= PSR_Z; } else { cpsr &= ~PSR_Z; }
    if (opc == ARM_SUB || opc == ARM_SBC || opc == ARM_RSB || opc == ARM_RSC || opc == ARM_CMP) {
        if ((result & 0xffffffff00000000LL) != 0) { cpsr &= ~PSR_C; } else { cpsr |= PSR_C; }
    } else if (opc == ARM_ADD || opc == ARM_ADC || opc == ARM_CMN) {
        if ((result & 0xffffffff00000000LL) != 0) { cpsr |= PSR_C; } else { cpsr &= ~PSR_C; }
    }
    if (opc == ARM_RSB || opc == ARM_RSC) {
        if ((n >> 31 != m >> 31) && (n >> 31 == (uint32_t) result >> 31)) { cpsr |= PSR_V; } else { cpsr &= ~PSR_V; }
    } else if (opc == ARM_SUB || opc == ARM_SBC || opc == ARM_CMP) {
        if ((n >> 31 != m >> 31) && (m >> 31 == (uint32_t) result >> 31)) { cpsr |= PSR_V; } else { cpsr &= ~PSR_V; }
    } else if (opc == ARM_ADD || opc == ARM_ADC || opc == ARM_CMN) {
        if ((n >> 31 == m >> 31) && (n >> 31 != (uint32_t) result >> 31)) { cpsr |= PSR_V; } else { cpsr &= ~PSR_V; }
    }
}

static uint32_t arm_shifter_op(uint32_t m, uint32_t s, uint32_t shop, uint32_t shamt, bool shreg, bool update_flags) {
    switch (shop) {
        case SHIFT_LSL:
            if (s >= 32) { m = 0; } else { m <<= s; }
            break;

        case SHIFT_LSR:
            if (s >= 32) { m = 0; } else { m >>= s; }
            break;

        case SHIFT_ASR:
            if (s >= 32) { m = (m & (1 << 31) ? ~0 : 0); } else { m = asr(m, s); }
            break;

        case SHIFT_ROR:
            if (!shreg && shamt == 0) {
                bool C = (cpsr & PSR_C) != 0;
                if (update_flags) {
                    if ((m & 1) != 0) { cpsr |= PSR_C; } else { cpsr &= ~PSR_C; }
                }
                m = m >> 1 | (C ? 1 << 31 : 0);
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

static void arm_shifter_flags(uint32_t shop, uint32_t s, uint32_t Rm) {
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
}

// AND{<cond>}{S} Rd, Rn, <shifter_operand>
// EOR{<cond>}{S} Rd, Rn, <shifter_operand>
// SUB{<cond>}{S} Rd, Rn, <shifter_operand>
// RSB{<cond>}{S} Rd, Rn, <shifter_operand>
// ADD{<cond>}{S} Rd, Rn, <shifter_operand>
// ADC{<cond>}{S} Rd, Rn, <shifter_operand>
// SBC{<cond>}{S} Rd, Rn, <shifter_operand>
// RSC{<cond>}{S} Rd, Rn, <shifter_operand>
// TST{<cond>}{P} Rn, <shifter_operand>
// TEQ{<cond>}{P} Rn, <shifter_operand>
// CMP{<cond>}{P} Rn, <shifter_operand>
// CMN{P}
// ORR{S}
// MOV{S}
// BIC{S}
// MVN{S}
void arm_data_processing_register(void) {
    uint32_t opc = (arm_op >> 21) & 0xf;
    bool S = (arm_op >> 20) & 1;
    uint32_t Rn = (arm_op >> 16) & 0xf;
    uint32_t Rd = (arm_op >> 12) & 0xf;
    uint32_t Rs = (arm_op >> 8) & 0xf;
    uint32_t shamt = (arm_op >> 7) & 0x1f;
    uint32_t shop = (arm_op >> 5) & 3;
    bool shreg = (arm_op >> 4) & 1;
    uint32_t Rm = (arm_op >> 0) & 0xf;

    bool is_test_or_compare = (opc == ARM_TST || opc == ARM_TEQ || opc == ARM_CMP || opc == ARM_CMN);
    bool is_move = (opc == ARM_MOV || opc == ARM_MVN);

#ifdef DEBUG
    if (log_instructions && log_arm_instructions) {
        arm_print_opcode();
        switch (opc) {
            case ARM_AND: print_mnemonic(S ? "ands" : "and"); break;
            case ARM_EOR: print_mnemonic(S ? "eors" : "eor"); break;
            case ARM_SUB: print_mnemonic(S ? "subs" : "sub"); break;
            case ARM_RSB: print_mnemonic(S ? "rsbs" : "rsb"); break;
            case ARM_ADD: print_mnemonic(S ? "adds" : "add"); break;
            case ARM_ADC: print_mnemonic(S ? "adcs" : "adc"); break;
            case ARM_SBC: print_mnemonic(S ? "sbcs" : "sbc"); break;
            case ARM_RSC: print_mnemonic(S ? "rscs" : "rsc"); break;
            case ARM_TST: print_mnemonic(Rd == 15 ? "tstp" : "tst"); break;
            case ARM_TEQ: print_mnemonic(Rd == 15 ? "teqp" : "teq"); break;
            case ARM_CMP: print_mnemonic(Rd == 15 ? "cmpp" : "cmp"); break;
            case ARM_CMN: print_mnemonic(Rd == 15 ? "cmnp" : "cmn"); break;
            case ARM_ORR: print_mnemonic(S ? "orrs" : "orr"); break;
            case ARM_MOV: print_mnemonic(S ? "movs" : "mov"); break;
            case ARM_BIC: print_mnemonic(S ? "bics" : "bic"); break;
            case ARM_MVN: print_mnemonic(S ? "mvns" : "mvn"); break;
        }
        if (!is_test_or_compare) {
            print_register(Rd);
            printf(",");
        }
        if (!is_move) {
            print_register(Rn);
            printf(",");
        }
        print_register(Rm);
        arm_print_shifter_op(shop, shamt, shreg, Rs);
        printf("\n");
    }
#endif

    if (is_test_or_compare) {
        assert(S == 1);
        assert(Rd == 0 || Rd == 15);
    }
    if (is_move) {
        assert(Rn == 0);
    }

    if (!shreg && (shop == SHIFT_LSR || shop == SHIFT_ASR) && shamt == 0) shamt = 32;
    uint32_t s = (shreg ? r[Rs] & 0xff : shamt);
    uint32_t n = r[Rn];
    if (Rn == 15 && shreg) n += 4;
    uint32_t m = r[Rm];
    if (Rm == 15 && shreg) m += 4;
    m = arm_shifter_op(m, s, shop, shamt, shreg, true);

    uint64_t result = arm_alu_op(opc, n, m);
    if (S) {
        arm_shifter_flags(shop, s, Rm);
        arm_alu_flags(opc, result, n, m);
    }
    if (!is_test_or_compare) {
        r[Rd] = (uint32_t) result;
        if (Rd == 15) {
            r[Rd] &= ~1;
            if (S) write_cpsr(read_spsr());
            branch_taken = true;
        }
    } else {
        if (Rd == 15) {
            write_cpsr(read_spsr());
        }
    }
}

void arm_data_processing_immediate(void) {
    uint32_t opc = (arm_op >> 21) & 0xf;
    bool S = (arm_op >> 20) & 1;
    uint32_t Rn = (arm_op >> 16) & 0xf;
    uint32_t Rd = (arm_op >> 12) & 0xf;
    uint32_t rot = (arm_op >> 8) & 0xf;
    uint32_t imm_unrotated = arm_op & 0xff;
    uint64_t imm = ror(imm_unrotated, 2 * rot);

    bool is_test_or_compare = (opc == ARM_TST || opc == ARM_TEQ || opc == ARM_CMP || opc == ARM_CMN);
    bool is_move = (opc == ARM_MOV || opc == ARM_MVN);

#ifdef DEBUG
    if (log_instructions && log_arm_instructions) {
        arm_print_opcode();
        switch (opc) {
            case ARM_AND: print_mnemonic(S ? "ands" : "and"); break;
            case ARM_EOR: print_mnemonic(S ? "eors" : "eor"); break;
            case ARM_SUB: print_mnemonic(S ? "subs" : "sub"); break;
            case ARM_RSB: print_mnemonic(S ? "rsbs" : "rsb"); break;
            case ARM_ADD: print_mnemonic(S ? "adds" : "add"); break;
            case ARM_ADC: print_mnemonic(S ? "adcs" : "adc"); break;
            case ARM_SBC: print_mnemonic(S ? "sbcs" : "sbc"); break;
            case ARM_RSC: print_mnemonic(S ? "rscs" : "rsc"); break;
            case ARM_TST: print_mnemonic("tst"); break;
            case ARM_TEQ: print_mnemonic("teq"); break;
            case ARM_CMP: print_mnemonic("cmp"); break;
            case ARM_CMN: print_mnemonic("cmn"); break;
            case ARM_ORR: print_mnemonic(S ? "orrs" : "orr"); break;
            case ARM_MOV: print_mnemonic(S ? "movs" : "mov"); break;
            case ARM_BIC: print_mnemonic(S ? "bics" : "bic"); break;
            case ARM_MVN: print_mnemonic(S ? "mvns" : "mvn"); break;
            default: assert(false); break;
        }
        if (!is_test_or_compare) {
            print_register(Rd);
            printf(",");
        }
        if (!is_move) {
            print_register(Rn);
            printf(",");
        }
        print_immediate(imm);
        printf("\n");
    }
#endif

    if (is_test_or_compare) {
        assert(S == 1);
        assert(Rd == 0);
    }
    if (is_move) {
        assert(Rn == 0);
    }

    uint32_t n = r[Rn];
    if (Rn == 15) n &= ~3;
    uint64_t result = arm_alu_op(opc, n, imm);
    if (S) {
        if (rot != 0) {
            if ((imm_unrotated & (1 << (2 * rot - 1))) != 0) { cpsr |= PSR_C; } else { cpsr &= ~PSR_C; }
        }
        arm_alu_flags(opc, result, n, imm);
    }
    if (!is_test_or_compare) {
        r[Rd] = (uint32_t) result;
        if (Rd == 15) {
            r[Rd] &= ~1;
            if (S) write_cpsr(read_spsr());
            branch_taken = true;
        }
    }
}

void arm_load_store_word_or_byte_immediate(void) {
    bool P = (arm_op >> 24) & 1;
    bool U = (arm_op >> 23) & 1;
    bool B = (arm_op >> 22) & 1;
    bool W = (arm_op >> 21) & 1;
    bool L = (arm_op >> 20) & 1;
    uint32_t Rn = (arm_op >> 16) & 0xf;
    uint32_t Rd = (arm_op >> 12) & 0xf;
    uint32_t imm = arm_op & 0xfff;

#ifdef DEBUG
    if (log_instructions) {
        arm_print_opcode();
        if (L) {
            print_mnemonic(B ? "ldrb" : "ldr");
        } else {
            print_mnemonic(B ? "strb" : "str");
        }
        print_register(Rd);
        printf(",[");
        print_register(Rn);
        printf(P ? "," : "],");
        if (!U) printf("-");
        print_immediate(imm);
        if (P) printf("]");
        if (W) printf("!");
        printf("\n");
    }
#endif

    uint32_t n = r[Rn];
    if (Rn == 15) n &= ~3;
    if (P) n += (U ? imm : -imm);
    if (L) {
        if (B) {
            r[Rd] = memory_read_byte(n);
        } else {
            r[Rd] = align_word(n, memory_read_word(n));
        }
        if (Rd == 15) branch_taken = true;
    } else {
        if (B) {
            memory_write_byte(n, (uint8_t) r[Rd]);
        } else {
            if (Rd == 15) {
                memory_write_word(n, r[Rd] + 4);
            } else {
                memory_write_word(n, r[Rd]);
            }
        }
    }
    if (!P) n += (U ? imm : -imm);
    if (((P && W) || !P) && !(L && Rd == Rn)) r[Rn] = n;
}

void arm_load_store_word_or_byte_register(void) {
    bool P = (arm_op >> 24) & 1;
    bool U = (arm_op >> 23) & 1;
    bool B = (arm_op >> 22) & 1;
    bool W = (arm_op >> 21) & 1;
    bool L = (arm_op >> 20) & 1;
    uint32_t Rn = (arm_op >> 16) & 0xf;
    uint32_t Rd = (arm_op >> 12) & 0xf;
    uint32_t shamt = (arm_op >> 7) & 0x1f;
    uint32_t shop = (arm_op >> 5) & 3;
    uint32_t Rm = arm_op & 0xf;

#ifdef DEBUG
    if (log_instructions) {
        arm_print_opcode();
        if (L) {
            print_mnemonic(B ? "ldrb" : "ldr");
        } else {
            print_mnemonic(B ? "strb" : "str");
        }
        print_register(Rd);
        printf(",[");
        print_register(Rn);
        printf(P ? "," : "],");
        if (!U) printf("-");
        print_register(Rm);
        arm_print_shifter_op(shop, shamt, false, 0);
        if (P) printf("]");
        if (W) printf("!");
        printf("\n");
    }
#endif

    assert((arm_op & (1 << 4)) == 0);

    if ((shop == SHIFT_LSR || shop == SHIFT_ASR) && shamt == 0) shamt = 32;
    uint32_t m = arm_shifter_op(r[Rm], shamt, shop, shamt, false, false);
    uint32_t n = r[Rn];
    if (Rn == 15) n &= ~3;
    if (P) n += (U ? m : -m);
    if (L) {
        if (B) {
            r[Rd] = memory_read_byte(n);
        } else {
            r[Rd] = align_word(n, memory_read_word(n));
        }
        if (Rd == 15) branch_taken = true;
    } else {
        // FIXME Rd == 15?
        if (B) {
            memory_write_byte(n, (uint8_t) r[Rd]);
        } else {
            memory_write_word(n, r[Rd]);
        }
    }
    if (!P) n += (U ? m : -m);
    if (((P && W) || !P) && !(L && Rd == Rn)) r[Rn] = n;
}

void arm_load_store_multiple(void) {
    bool P = (arm_op >> 24) & 1;
    bool U = (arm_op >> 23) & 1;
    bool S = (arm_op >> 22) & 1;
    bool W = (arm_op >> 21) & 1;
    bool L = (arm_op >> 20) & 1;
    uint32_t Rn = (arm_op >> 16) & 0xf;
    uint32_t rlist = arm_op & 0xffff;

#ifdef DEBUG
    if (log_instructions && log_arm_instructions) {
        arm_print_opcode();
        if (L) {
            if (!P && !U) {
                print_mnemonic("ldmda");
            } else if (!P && U) {
                print_mnemonic("ldmia");
            } else if (P && !U) {
                print_mnemonic("ldmdb");
            } else if (P && U) {
                print_mnemonic("ldmib");
            }
        } else {
            if (!P && !U) {
                print_mnemonic("stmda");
            } else if (!P && U) {
                print_mnemonic("stmia");
            } else if (P && !U) {
                print_mnemonic("stmdb");
            } else if (P && U) {
                print_mnemonic("stmib");
            }
        }
        print_register(Rn);
        if (W) printf("!");
        printf(",{");
        arm_print_rlist(rlist);
        printf("}\n");
    }
#endif

    uint32_t count = bit_count(rlist);
    if (rlist == 0) {
        rlist |= 1 << 15;
        count = 16;
    }
    uint32_t old_base = r[Rn];
    uint32_t new_base = old_base + (U ? 4 : -4) * count;
    uint32_t address = old_base;
    if (!U) address -= 4 * count;
    if (U == P) address += 4;
    if (S) mode_change(cpsr & PSR_MODE, PSR_MODE_USR);
    for (uint32_t i = 0; i < 16; i++) {
        if (rlist & (1 << i)) {
            if (L) {
                if (i == Rn) W = false;
                r[i] = memory_read_word(address);
                if (i == 15) {
                    r[i] &= ~1;
                    if (S) write_cpsr(read_spsr());  // FIXME?
                    branch_taken = true;
                }
            } else {
                if (i == 15) {
                    memory_write_word(address, r[i] + ((cpsr & PSR_T) != 0 ? 2 : 4));
                } else if (i == Rn) {
                    if (lowest_set_bit(rlist) == Rn) {
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
    if (S) mode_change(PSR_MODE_USR, cpsr & PSR_MODE);
    if (W) r[Rn] = new_base;
}

void arm_branch(void) {
    bool L = (arm_op >> 24) & 1;
    uint32_t imm = arm_op & 0xffffff;
    if (arm_op & 0x800000) imm |= ~0xffffff;

#ifdef DEBUG
    if (log_instructions && log_arm_instructions) {
        arm_print_opcode();
        print_mnemonic(L ? "bl" : "b");
        print_address(r[15] + (imm << 2));
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
        print_mnemonic("swi");
        print_address(arm_op & 0xffffff);
        printf("\n");
    }
#endif

    bool T = (cpsr & PSR_T) != 0;
    r14_svc = r[15] - (T ? 2 : 4);
    spsr_svc = cpsr;
    write_cpsr((cpsr & ~(PSR_T | PSR_MODE)) | PSR_I | PSR_MODE_SVC);
    r[15] = VEC_SWI;
    branch_taken = true;
}

void arm_multiply(void) {
    bool A = (arm_op >> 21) & 1;
    bool S = (arm_op >> 20) & 1;
    uint32_t Rd = (arm_op >> 16) & 0xf;
    uint32_t Rn = (arm_op >> 12) & 0xf;
    uint32_t Rs = (arm_op >> 8) & 0xf;
    uint32_t Rm = arm_op & 0xf;

#ifdef DEBUG
    if (log_instructions && log_arm_instructions) {
        arm_print_opcode();
        if (A) {
            print_mnemonic(S ? "mlas" : "mla");
        } else {
            print_mnemonic(S ? "muls" : "mul");
        }
        print_register(Rd);
        printf(",");
        print_register(Rm);
        printf(",");
        print_register(Rs);
        if (A) {
            printf(",");
            print_register(Rn);
        }
        printf("\n");
    }
#endif

    if (!A) assert(Rn == 0);
    assert(Rd != 15 && Rm != 15 && Rs != 15);

    uint32_t result = r[Rm] * r[Rs];
    if (A) result += r[Rn];
    r[Rd] = result;

    if (S) {
        if ((result & (1 << 31)) != 0) { cpsr |= PSR_N; } else { cpsr &= ~PSR_N; }
        if (result == 0) { cpsr |= PSR_Z; } else { cpsr &= ~PSR_Z; }
    }
}

void arm_multiply_long(void) {
    bool U = (arm_op >> 22) & 1;
    bool A = (arm_op >> 21) & 1;
    bool S = (arm_op >> 20) & 1;
    uint32_t RdHi = (arm_op >> 16) & 0xf;
    uint32_t RdLo = (arm_op >> 12) & 0xf;
    uint32_t Rs = (arm_op >> 8) & 0xf;
    uint32_t Rm = arm_op & 0xf;

#ifdef DEBUG
    if (log_instructions && log_arm_instructions) {
        arm_print_opcode();
        if (U) {
            if (A) {
                print_mnemonic(S ? "smlals" : "smlal");
            } else {
                print_mnemonic(S ? "smulls" : "smull");
            }
        } else {
            if (A) {
                print_mnemonic(S ? "umlals" : "umlal");
            } else {
                print_mnemonic(S ? "umulls" : "umull");
            }
        }
        print_register(RdLo);
        printf(",");
        print_register(RdHi);
        printf(",");
        print_register(Rm);
        printf(",");
        print_register(Rs);
        printf("\n");
    }
#endif

    assert(RdHi != 15 && RdLo != 15 && Rm != 15 && Rs != 15);
    assert(RdHi != Rm && RdLo != Rm && RdHi != RdLo);

    uint64_t m = r[Rm];
    uint64_t s = r[Rs];
    if (U) {
        if ((m & (1 << 31)) != 0) m |= ~0xffffffffLL;
        if ((s & (1 << 31)) != 0) s |= ~0xffffffffLL;
    }
    uint64_t result = m * s;
    if (A) result += (uint64_t) r[RdLo] | (uint64_t) r[RdHi] << 32;
    r[RdLo] = result & 0xffffffff;
    r[RdHi] = (result >> 32) & 0xffffffff;

    if (S) {
        if ((r[RdHi] & (1 << 31)) != 0) { cpsr |= PSR_N; } else { cpsr &= ~PSR_N; }
        if (r[RdHi] == 0 && r[RdLo] == 0) { cpsr |= PSR_Z; } else { cpsr &= ~PSR_Z; }
    }
}

void arm_load_store_halfword_register(void) {
    bool P = (arm_op >> 24) & 1;
    bool U = (arm_op >> 23) & 1;
    bool I = (arm_op >> 22) & 1;
    bool W = (arm_op >> 21) & 1;
    bool L = (arm_op >> 20) & 1;
    uint32_t Rn = (arm_op >> 16) & 0xf;
    uint32_t Rd = (arm_op >> 12) & 0xf;
    uint32_t sbz = (arm_op >> 8) & 0xf;
    uint32_t opc = (arm_op >> 4) & 0xf;
    uint32_t Rm = arm_op & 0xf;

#ifdef DEBUG
    if (log_instructions && log_arm_instructions) {
        arm_print_opcode();
        print_mnemonic(L ? "ldrh" : "strh");
        print_register(Rd);
        printf(",[");
        print_register(Rn);
        printf(P ? "," : "],");
        if (!U) printf("-");
        print_register(Rm);
        if (P) printf("]");
        if (W) printf("!");
        printf("\n");
    }
#endif

    assert(!I);
    assert(sbz == 0);
    assert(opc == 0xb);

    uint32_t m = r[Rm];
    uint32_t n = r[Rn];
    if (Rn == 15) n &= ~3;
    if (P) n += (U ? m : -m);
    if (L) {
        r[Rd] = align_halfword(n, memory_read_halfword(n & ~1));
        if (Rd == 15) branch_taken = true;
    } else {
        memory_write_halfword(n, (uint16_t) r[Rd]);
    }
    if (!P) n += (U ? m : -m);
    if (((P && W) || !P) && !(L && Rd == Rn)) r[Rn] = n;
}

void arm_load_store_halfword_immediate(void) {
    bool P = (arm_op >> 24) & 1;
    bool U = (arm_op >> 23) & 1;
    bool I = (arm_op >> 22) & 1;
    bool W = (arm_op >> 21) & 1;
    bool L = (arm_op >> 20) & 1;
    uint32_t Rn = (arm_op >> 16) & 0xf;
    uint32_t Rd = (arm_op >> 12) & 0xf;
    uint32_t imm = (arm_op & 0xf) | ((arm_op >> 4) & 0xf0);
    uint32_t opc = (arm_op >> 4) & 0xf;

#ifdef DEBUG
    if (log_instructions && log_arm_instructions) {
        arm_print_opcode();
        print_mnemonic(L ? "ldrh" : "strh");
        print_register(Rd);
        printf(",[");
        print_register(Rn);
        printf(P ? "," : "],");
        if (!U) printf("-");
        print_immediate(imm);
        if (P) printf("]");
        if (W) printf("!");
        printf("\n");
    }
#endif

    assert(I);
    assert(opc == 0xb);

    uint32_t n = r[Rn];
    if (Rn == 15) n &= ~3;
    if (P) n += (U ? imm : -imm);
    if (L) {
        r[Rd] = align_halfword(n, memory_read_halfword(n & ~1));
        if (Rd == 15) branch_taken = true;
    } else {
        memory_write_halfword(n, (uint16_t) r[Rd]);
    }
    if (!P) n += (U ? imm : -imm);
    if (((P && W) || !P) && !(L && Rd == Rn)) r[Rn] = n;
}

void arm_load_signed_halfword_or_signed_byte_register(void) {
    bool P = (arm_op >> 24) & 1;
    bool U = (arm_op >> 23) & 1;
    bool I = (arm_op >> 22) & 1;
    bool W = (arm_op >> 21) & 1;
    bool L = (arm_op >> 20) & 1;
    uint32_t Rn = (arm_op >> 16) & 0xf;
    uint32_t Rd = (arm_op >> 12) & 0xf;
    uint32_t sbz = (arm_op >> 8) & 0xf;
    uint32_t opc = (arm_op >> 4) & 0xf;
    uint32_t Rm = arm_op & 0xf;

#ifdef DEBUG
    if (log_instructions && log_arm_instructions) {
        arm_print_opcode();
        if (opc == 0xd) {
            print_mnemonic("ldrsb");
        } else if (opc == 0xf) {
            print_mnemonic("ldrsh");
        } else {
            assert(false);
        }
        print_register(Rd);
        printf(",[");
        print_register(Rn);
        printf(P ? "," : "],");
        if (!U) printf("-");
        print_register(Rm);
        if (P) printf("]");
        if (W) printf("!");
        printf("\n");
    }
#endif

    assert(!I);
    assert(L);
    assert(sbz == 0);
    assert(opc == 0xd || opc == 0xf);

    uint32_t m = r[Rm];
    uint32_t n = r[Rn];
    if (Rn == 15) n &= ~3;
    if (P) n += (U ? m : -m);
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
    if (Rd == 15) branch_taken = true;
    if (!P) n += (U ? m : -m);
    if (((P && W) || !P) && !(L && Rd == Rn)) r[Rn] = n;
}

void arm_load_signed_halfword_or_signed_byte_immediate(void) {
    bool P = (arm_op >> 24) & 1;
    bool U = (arm_op >> 23) & 1;
    bool I = (arm_op >> 22) & 1;
    bool W = (arm_op >> 21) & 1;
    bool L = (arm_op >> 20) & 1;
    uint32_t Rn = (arm_op >> 16) & 0xf;
    uint32_t Rd = (arm_op >> 12) & 0xf;
    uint32_t imm = (arm_op & 0xf) | ((arm_op >> 4) & 0xf0);
    uint32_t opc = (arm_op >> 4) & 0xf;

#ifdef DEBUG
    if (log_instructions && log_arm_instructions) {
        arm_print_opcode();
        if (opc == 0xd) {
            print_mnemonic("ldrsb");
        } else if (opc == 0xf) {
            print_mnemonic("ldrsh");
        } else {
            assert(false);
        }
        print_register(Rd);
        printf(",[");
        print_register(Rn);
        printf(P ? "," : "],");
        if (!U) printf("-");
        print_immediate(imm);
        if (P) printf("]");
        if (W) printf("!");
        printf("\n");
    }
#endif

    assert(I);
    assert(L);
    assert(opc == 0xd || opc == 0xf);

    uint32_t n = r[Rn];
    if (Rn == 15) n &= ~3;
    if (P) n += (U ? imm : -imm);
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
    if (Rd == 15) branch_taken = true;
    if (!P) n += (U ? imm : -imm);
    if (((P && W) || !P) && !(L && Rd == Rn)) r[Rn] = n;
}

void arm_special_data_processing_register(void) {
    bool R = (arm_op >> 22) & 1;
    bool b21 = (arm_op >> 21) & 1;
    uint32_t mask_type = (arm_op >> 16) & 0xf;
    uint32_t Rd = (arm_op >> 12) & 0xf;
    uint32_t Rm = arm_op & 0xf;

#ifdef DEBUG
    if (log_instructions && log_arm_instructions) {
        arm_print_opcode();
        if (b21) {
            print_mnemonic("msr");
            printf(R ? "spsr" : "cpsr");
            printf("_");
            switch (mask_type) {
                case 8: printf("f"); break;
                case 9: printf("cf"); break;
                default: assert(false); break;
            }
            printf(",");
            print_register(Rm);
        } else {
            print_mnemonic("mrs");
            print_register(Rd);
            printf(",");
            printf(R ? "spsr" : "cpsr");
        }
        printf("\n");
    }
#endif

    if (b21) {
        assert(Rd == 0xf);
        assert((arm_op & 0xff0) == 0);
    } else {
        assert(mask_type == 0xf);
        assert((arm_op & 0xfff) == 0);
    }

    if (b21) {
        uint32_t mask = 0;
        switch (mask_type) {
            case 8: mask = 0xf0000000; break;
            case 9: mask = 0xf000001f; break;  // Allow bit 4 to be set?
            default: assert(false); break;
        }
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
        assert(Rd != 15);
    }
}

void arm_special_data_processing_immediate(void) {
    bool R = (arm_op >> 22) & 1;
    uint32_t mask_type = (arm_op >> 16) & 0xf;
    uint32_t sbo = (arm_op >> 12) & 0xf;
    uint32_t rot = (arm_op >> 8) & 0xf;
    uint32_t imm = ror(arm_op & 0xff, 2 * rot);

#ifdef DEBUG
    if (log_instructions && log_arm_instructions) {
        arm_print_opcode();
        print_mnemonic("msr");
        printf(R ? "spsr" : "cpsr");
        printf("_");
        switch (mask_type) {
            case 1: printf("c"); break;
            case 8: printf("f"); break;
            case 9: printf("cf"); break;
            default: printf("none"); break;
        }
        printf(",");
        print_immediate(imm);
        printf("\n");
    }
#endif

    assert(sbo == 0xf);

    uint32_t mask = 0;
    switch (mask_type) {
        case 0: mask = 0x00000000; break;
        case 1: mask = 0x0000001f; break;  // Allow bit 4 to be set?
        case 8: mask = 0xf0000000; break;
        case 9: mask = 0xf000001f; break;  // Allow bit 4 to be set?
        default: assert(false); break;
    }
    if (R) {
        write_spsr((read_spsr() & ~mask) | (imm & mask));
    } else {
        write_cpsr((cpsr & ~mask) | (imm & mask));
    }
}

void arm_swap(void) {
    bool B = (arm_op >> 22) & 1;
    uint32_t Rn = (arm_op >> 16) & 0xf;
    uint32_t Rd = (arm_op >> 12) & 0xf;
    uint32_t sbz = (arm_op >> 8) & 0xf;
    uint32_t Rm = arm_op & 0xf;

#ifdef DEBUG
    if (log_instructions && log_arm_instructions) {
        arm_print_opcode();
        print_mnemonic(B ? "swpb" : "swp");
        print_register(Rd);
        printf(",");
        print_register(Rm);
        printf(",[");
        print_register(Rn);
        printf("]\n");
    }
#endif

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
    assert(Rd != 15);
}

void arm_branch_and_exchange(void) {
    uint32_t sbo = (arm_op >> 8) & 0xfff;
    uint32_t Rm = arm_op & 0xf;

#ifdef DEBUG
    if (log_instructions && log_arm_instructions) {
        arm_print_opcode();
        print_mnemonic("bx");
        print_register(Rm);
        printf("  ; Rm = 0x%x", r[Rm]);
        printf("\n");
    }
#endif

    assert(sbo == 0xfff);
    if ((r[Rm] & 1) == 0) {
        assert((r[Rm] & 2) == 0);
    }

    if ((r[Rm] & 1) != 0) { cpsr |= PSR_T; } else { cpsr &= ~PSR_T; }
    r[15] = r[Rm] & ~1;
    branch_taken = true;
}
