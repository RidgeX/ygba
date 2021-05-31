#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "algorithms.h"
#include "cpu.h"

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
        default: abort();
    }
}

static void arm_alu_flags(uint32_t opc, uint32_t n, uint32_t m, uint64_t result) {
    uint32_t result_lo = (uint32_t) result;
    uint32_t result_hi = (uint32_t)(result >> 32);
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

static uint32_t arm_shifter_op(uint32_t m, uint32_t s, uint32_t shop, uint32_t shamt, bool shreg, bool update_carry) {
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
            if (!shreg && shamt == 0) {  // ROR #0 -> RRX
                bool C = FLAG_C();
                if (update_carry) {
                    ASSIGN_C(BIT(m, 0));
                }
                return m >> 1 | (C ? (1 << 31) : 0);
            }
            return ROR(m, s & 31);

        default:
            abort();
    }
}

static void arm_shifter_flags(uint32_t shop, uint32_t s, uint32_t m) {
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
            abort();
    }
}

// AND{<cond>}{S} Rd, Rn, Rm{, <shifter_operand>}
// EOR{<cond>}{S} Rd, Rn, Rm{, <shifter_operand>}
// SUB{<cond>}{S} Rd, Rn, Rm{, <shifter_operand>}
// RSB{<cond>}{S} Rd, Rn, Rm{, <shifter_operand>}
// ADD{<cond>}{S} Rd, Rn, Rm{, <shifter_operand>}
// ADC{<cond>}{S} Rd, Rn, Rm{, <shifter_operand>}
// SBC{<cond>}{S} Rd, Rn, Rm{, <shifter_operand>}
// RSC{<cond>}{S} Rd, Rn, Rm{, <shifter_operand>}
// TST{<cond>}{P} Rn, Rm{, <shifter_operand>}
// TEQ{<cond>}{P} Rn, Rm{, <shifter_operand>}
// CMP{<cond>}{P} Rn, Rm{, <shifter_operand>}
// CMN{<cond>}{P} Rn, Rm{, <shifter_operand>}
// ORR{<cond>}{S} Rd, Rn, Rm{, <shifter_operand>}
// MOV{<cond>}{S} Rd, Rm{, <shifter_operand>}
// BIC{<cond>}{S} Rd, Rn, Rm{, <shifter_operand>}
// MVN{<cond>}{S} Rd, Rm{, <shifter_operand>}
int arm_data_processing_register(void) {
    uint32_t opc = BITS(arm_op, 21, 24);
    bool S = BIT(arm_op, 20);
    uint32_t Rn = BITS(arm_op, 16, 19);
    uint32_t Rd = BITS(arm_op, 12, 15);
    uint32_t Rs = BITS(arm_op, 8, 11);
    uint32_t shamt = BITS(arm_op, 7, 11);
    uint32_t shop = BITS(arm_op, 5, 6);
    bool shreg = BIT(arm_op, 4);
    uint32_t Rm = BITS(arm_op, 0, 3);

    if (!shreg && (shop == SHIFT_LSR || shop == SHIFT_ASR) && shamt == 0) {
        shamt = 32;  // LSR #0 -> LSR #32, ASR #0 -> ASR #32
    }

    bool is_test_or_compare = (opc == ARM_TST || opc == ARM_TEQ || opc == ARM_CMP || opc == ARM_CMN);
    bool is_move = (opc == ARM_MOV || opc == ARM_MVN);

#ifdef DEBUG
    if (log_instructions && log_arm_instructions && !FLAG_T()) {
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
            case ARM_TST: print_mnemonic(Rd == REG_PC ? "tstp" : "tst"); break;
            case ARM_TEQ: print_mnemonic(Rd == REG_PC ? "teqp" : "teq"); break;
            case ARM_CMP: print_mnemonic(Rd == REG_PC ? "cmpp" : "cmp"); break;
            case ARM_CMN: print_mnemonic(Rd == REG_PC ? "cmnp" : "cmn"); break;
            case ARM_ORR: print_mnemonic(S ? "orrs" : "orr"); break;
            case ARM_MOV: print_mnemonic(S ? "movs" : "mov"); break;
            case ARM_BIC: print_mnemonic(S ? "bics" : "bic"); break;
            case ARM_MVN: print_mnemonic(S ? "mvns" : "mvn"); break;
        }
        if (!is_test_or_compare) {
            print_register(Rd);
            printf(", ");
        }
        if (!is_move) {
            print_register(Rn);
            printf(", ");
        }
        print_register(Rm);
        arm_print_shifter_op(shop, shamt, shreg, Rs);
        printf("\n");
    }
#endif

    if (is_test_or_compare) {
        assert(S == 1);
        assert(Rd == 0 || Rd == REG_PC);
    } else if (is_move) {
        assert(Rn == 0);
    }

    uint32_t s = (shreg ? (uint8_t) r[Rs] : shamt);  // Use least significant byte of Rs
    uint32_t m = r[Rm];
    if (Rm == REG_PC && shreg) m += SIZEOF_INSTR;  // PC ahead if shift by register
    m = arm_shifter_op(m, s, shop, shamt, shreg, true);
    uint32_t n = r[Rn];
    if (Rn == REG_PC && shreg) n += SIZEOF_INSTR;  // PC ahead if shift by register
    uint64_t result = arm_alu_op(opc, n, m);
    if (S) {
        arm_shifter_flags(shop, s, r[Rm]);
        arm_alu_flags(opc, n, m, result);
    }
    if (!is_test_or_compare) {
        r[Rd] = (uint32_t) result;
        if (Rd == REG_PC) {  // PC altered
            r[Rd] &= ~1;
            if (S) write_cpsr(read_spsr());
            branch_taken = true;
        }
    } else {
        if (Rd == REG_PC) {  // ARMv2 mode change (obsolete)
            write_cpsr(read_spsr());  // Restores SPSR on ARMv4
        }
    }

    return 1;
}

// AND{<cond>}{S} Rd, Rn, #<immediate>
// EOR{<cond>}{S} Rd, Rn, #<immediate>
// SUB{<cond>}{S} Rd, Rn, #<immediate>
// RSB{<cond>}{S} Rd, Rn, #<immediate>
// ADD{<cond>}{S} Rd, Rn, #<immediate>
// ADC{<cond>}{S} Rd, Rn, #<immediate>
// SBC{<cond>}{S} Rd, Rn, #<immediate>
// RSC{<cond>}{S} Rd, Rn, #<immediate>
// TST{<cond>} Rn, #<immediate>
// TEQ{<cond>} Rn, #<immediate>
// CMP{<cond>} Rn, #<immediate>
// CMN{<cond>} Rn, #<immediate>
// ORR{<cond>}{S} Rd, Rn, #<immediate>
// MOV{<cond>}{S} Rd, #<immediate>
// BIC{<cond>}{S} Rd, Rn, #<immediate>
// MVN{<cond>}{S} Rd, #<immediate>
int arm_data_processing_immediate(void) {
    uint32_t opc = BITS(arm_op, 21, 24);
    bool S = BIT(arm_op, 20);
    uint32_t Rn = BITS(arm_op, 16, 19);
    uint32_t Rd = BITS(arm_op, 12, 15);
    uint32_t rot = BITS(arm_op, 8, 11);
    uint32_t imm = ROR(BITS(arm_op, 0, 7), 2 * rot);

    bool is_test_or_compare = (opc == ARM_TST || opc == ARM_TEQ || opc == ARM_CMP || opc == ARM_CMN);
    bool is_move = (opc == ARM_MOV || opc == ARM_MVN);

#ifdef DEBUG
    if (log_instructions && log_arm_instructions && !FLAG_T()) {
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
        }
        if (!is_test_or_compare) {
            print_register(Rd);
            printf(", ");
        }
        if (!is_move) {
            print_register(Rn);
            printf(", ");
        }
        print_immediate(imm);
        printf("\n");
    }
#endif

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
            if (S) write_cpsr(read_spsr());
            branch_taken = true;
        }
    }

    return 1;
}

// STR{<cond>} Rd, <addressing_mode>
// STR{<cond>}B Rd, <addressing_mode>
// LDR{<cond>} Rd, <addressing_mode>
// LDR{<cond>}B Rd, <addressing_mode>
int arm_load_store_word_or_byte_register(void) {
    bool P = BIT(arm_op, 24);
    bool U = BIT(arm_op, 23);
    bool B = BIT(arm_op, 22);
    bool W = BIT(arm_op, 21);
    bool L = BIT(arm_op, 20);
    uint32_t Rn = BITS(arm_op, 16, 19);
    uint32_t Rd = BITS(arm_op, 12, 15);
    uint32_t shamt = BITS(arm_op, 7, 11);
    uint32_t shop = BITS(arm_op, 5, 6);
    bool shreg = BIT(arm_op, 4);
    uint32_t Rm = BITS(arm_op, 0, 3);

    bool T = !P && W;
    assert(!T);

    assert(!shreg);
    if ((shop == SHIFT_LSR || shop == SHIFT_ASR) && shamt == 0) {
        shamt = 32;  // LSR #0 -> LSR #32, ASR #0 -> ASR #32
    }

#ifdef DEBUG
    if (log_instructions && log_arm_instructions && !FLAG_T()) {
        arm_print_opcode();
        if (L) {
            print_mnemonic(B ? "ldrb" : "ldr");
        } else {
            print_mnemonic(B ? "strb" : "str");
        }
        print_register(Rd);
        printf(", [");
        print_register(Rn);
        printf(P ? ", " : "], ");
        if (!U) printf("-");
        print_register(Rm);
        arm_print_shifter_op(shop, shamt, false, 0);
        if (P) printf("]");
        if (W) printf("!");
        printf("\n");
    }
#endif

    uint32_t m = arm_shifter_op(r[Rm], shamt, shop, shamt, false, false);
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
        // FIXME Rd == REG_PC?
        if (B) {
            memory_write_byte(n, (uint8_t) r[Rd]);
        } else {
            memory_write_word(n, r[Rd]);
        }
    }
    if (!P) n += (U ? m : -m);
    if ((!P || W) && (!L || Rd != Rn)) r[Rn] = n;

    return 1;
}

int arm_load_store_word_or_byte_immediate(void) {
    bool P = BIT(arm_op, 24);
    bool U = BIT(arm_op, 23);
    bool B = BIT(arm_op, 22);
    bool W = BIT(arm_op, 21);
    bool L = BIT(arm_op, 20);
    uint32_t Rn = BITS(arm_op, 16, 19);
    uint32_t Rd = BITS(arm_op, 12, 15);
    uint32_t imm = BITS(arm_op, 0, 11);

    bool T = !P && W;
    assert(!T);

#ifdef DEBUG
    if (log_instructions && log_arm_instructions && !FLAG_T()) {
        arm_print_opcode();
        if (L) {
            print_mnemonic(B ? "ldrb" : "ldr");
        } else {
            print_mnemonic(B ? "strb" : "str");
        }
        print_register(Rd);
        if (P && U && !W && Rn == REG_PC) {
            uint32_t pc_rel_address = r[Rn] - SIZEOF_INSTR + imm;  // FIXME
            printf(", =0x%08X  @ 0x%08X", memory_read_word(pc_rel_address), pc_rel_address);
        } else {
            printf(", [");
            print_register(Rn);
            printf(P ? ", " : "], ");
            if (!U) printf("-");
            print_immediate(imm);
            if (P) printf("]");
            if (W) printf("!");
        }
        printf("\n");
    }
#endif

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
        if (B) {
            memory_write_byte(n, (uint8_t) r[Rd]);
        } else {
            if (Rd == REG_PC) {
                memory_write_word(n, r[Rd] + 4);
            } else {
                memory_write_word(n, r[Rd]);
            }
        }
    }
    if (!P) n += (U ? imm : -imm);
    if ((!P || W) && (!L || Rd != Rn)) r[Rn] = n;

    return 1;
}

int arm_load_store_multiple(void) {
    bool P = BIT(arm_op, 24);
    bool U = BIT(arm_op, 23);
    bool S = BIT(arm_op, 22);
    bool W = BIT(arm_op, 21);
    bool L = BIT(arm_op, 20);
    uint32_t Rn = BITS(arm_op, 16, 19);
    uint32_t rlist = BITS(arm_op, 0, 15);

#ifdef DEBUG
    if (log_instructions && log_arm_instructions && !FLAG_T()) {
        arm_print_opcode();
        if (L) {
            if (!P && !U) {
                print_mnemonic_d("ldm", "da");
            } else if (!P && U) {
                print_mnemonic_d("ldm", "ia");
            } else if (P && !U) {
                print_mnemonic_d("ldm", "db");
            } else if (P && U) {
                print_mnemonic_d("ldm", "ib");
            }
        } else {
            if (!P && !U) {
                print_mnemonic_d("stm", "da");
            } else if (!P && U) {
                print_mnemonic_d("stm", "ia");
            } else if (P && !U) {
                print_mnemonic_d("stm", "db");
            } else if (P && U) {
                print_mnemonic_d("stm", "ib");
            }
        }
        print_register(Rn);
        if (W) printf("!");
        printf(", {");
        arm_print_rlist(rlist);
        printf("}\n");
    }
#endif

    uint32_t count = bits_popcount(rlist);
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
    }
    if (S) mode_change(cpsr & PSR_MODE, PSR_MODE_USR);
    for (uint32_t i = 0; i < 16; i++) {
        if (rlist & (1 << i)) {
            if (L) {
                if (i == Rn) W = false;
                r[i] = memory_read_word(address);
                if (i == REG_PC) {  // PC altered
                    r[i] &= ~1;
                    if (S) write_cpsr(read_spsr());
                    branch_taken = true;
                }
            } else {
                if (i == REG_PC) {
                    memory_write_word(address, r[i] + SIZEOF_INSTR);
                } else if (i == Rn) {
                    if (bits_ctz(rlist) == Rn) {
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

int arm_branch(void) {
    bool L = BIT(arm_op, 24);
    uint32_t imm = BITS(arm_op, 0, 23);
    ZERO_EXTEND(imm, 23);

#ifdef DEBUG
    if (log_instructions && log_arm_instructions && !FLAG_T()) {
        arm_print_opcode();
        print_mnemonic(L ? "bl" : "b");
        print_address(r[REG_PC] - SIZEOF_INSTR + (imm << 2));  // FIXME
        printf("\n");
    }
#endif

    if (L) r[REG_LR] = r[REG_PC] - 4;
    r[REG_PC] += imm << 2;
    branch_taken = true;

    return 1;
}

int arm_software_interrupt(void) {
#ifdef DEBUG
    if (log_instructions && log_arm_instructions && !FLAG_T()) {
        arm_print_opcode();
        print_mnemonic("swi");
        print_address(BITS(arm_op, 0, 23));
        printf("\n");
    }
#endif

    r14_svc = r[REG_PC] - SIZEOF_INSTR;  // ARM: PC + 4, Thumb: PC + 2
    spsr_svc = cpsr;
    write_cpsr((cpsr & ~(PSR_T | PSR_MODE)) | PSR_I | PSR_MODE_SVC);
    r[REG_PC] = VEC_SWI;
    branch_taken = true;

    return 1;
}

int arm_hardware_interrupt(void) {
    r14_irq = r[REG_PC] - (FLAG_T() ? 0 : 4);  // ARM: PC + 4, Thumb: PC + 4
    spsr_irq = cpsr;
    write_cpsr((cpsr & ~(PSR_T | PSR_MODE)) | PSR_I | PSR_MODE_IRQ);
    r[REG_PC] = VEC_IRQ;
    branch_taken = true;

    return 1;
}

int arm_multiply(void) {
    bool A = BIT(arm_op, 21);
    bool S = BIT(arm_op, 20);
    uint32_t Rd = BITS(arm_op, 16, 19);
    uint32_t Rn = BITS(arm_op, 12, 15);
    uint32_t Rs = BITS(arm_op, 8, 11);
    uint32_t Rm = BITS(arm_op, 0, 3);

#ifdef DEBUG
    if (log_instructions && log_arm_instructions && !FLAG_T()) {
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

int arm_multiply_long(void) {
    bool U = BIT(arm_op, 22);
    bool A = BIT(arm_op, 21);
    bool S = BIT(arm_op, 20);
    uint32_t RdHi = BITS(arm_op, 16, 19);
    uint32_t RdLo = BITS(arm_op, 12, 15);
    uint32_t Rs = BITS(arm_op, 8, 11);
    uint32_t Rm = BITS(arm_op, 0, 3);

#ifdef DEBUG
    if (log_instructions && log_arm_instructions && !FLAG_T()) {
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

    assert(RdHi != REG_PC && RdLo != REG_PC && Rm != REG_PC && Rs != REG_PC);
    assert(RdHi != Rm && RdLo != Rm && RdHi != RdLo);

    uint64_t m = r[Rm];
    uint64_t s = r[Rs];
    if (U) {
        if (BIT(m, 31)) m |= ~0xffffffffLL;
        if (BIT(s, 31)) s |= ~0xffffffffLL;
    }
    uint64_t result = m * s;
    if (A) result += (uint64_t) r[RdLo] | (uint64_t) r[RdHi] << 32;
    r[RdLo] = (uint32_t) result;
    r[RdHi] = (uint32_t)(result >> 32);

    if (S) {
        ASSIGN_N(BIT(r[RdHi], 31));
        ASSIGN_Z(r[RdHi] == 0 && r[RdLo] == 0);
    }

    return 1;
}

int arm_load_store_halfword_register(void) {
    bool P = BIT(arm_op, 24);
    bool U = BIT(arm_op, 23);
    bool I = BIT(arm_op, 22);
    bool W = BIT(arm_op, 21);
    bool L = BIT(arm_op, 20);
    uint32_t Rn = BITS(arm_op, 16, 19);
    uint32_t Rd = BITS(arm_op, 12, 15);
    uint32_t sbz = BITS(arm_op, 8, 11);
    uint32_t opc = BITS(arm_op, 4, 7);
    uint32_t Rm = BITS(arm_op, 0, 3);

    bool T = !P && W;
    assert(!T);

#ifdef DEBUG
    if (log_instructions && log_arm_instructions && !FLAG_T()) {
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
    if (Rn == REG_PC) n &= ~3;
    if (P) n += (U ? m : -m);
    if (L) {
        r[Rd] = align_halfword(n, memory_read_halfword(n & ~1));
        if (Rd == REG_PC) branch_taken = true;
    } else {
        memory_write_halfword(n, (uint16_t) r[Rd]);
    }
    if (!P) n += (U ? m : -m);
    if ((!P || W) && (!L || Rd != Rn)) r[Rn] = n;

    return 1;
}

int arm_load_store_halfword_immediate(void) {
    bool P = BIT(arm_op, 24);
    bool U = BIT(arm_op, 23);
    bool I = BIT(arm_op, 22);
    bool W = BIT(arm_op, 21);
    bool L = BIT(arm_op, 20);
    uint32_t Rn = BITS(arm_op, 16, 19);
    uint32_t Rd = BITS(arm_op, 12, 15);
    uint32_t opc = BITS(arm_op, 4, 7);
    uint32_t imm = BITS(arm_op, 8, 11) << 4 | BITS(arm_op, 0, 3);

    bool T = !P && W;
    assert(!T);

#ifdef DEBUG
    if (log_instructions && log_arm_instructions && !FLAG_T()) {
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
    if (Rn == REG_PC) n &= ~3;
    if (P) n += (U ? imm : -imm);
    if (L) {
        r[Rd] = align_halfword(n, memory_read_halfword(n & ~1));
        if (Rd == REG_PC) branch_taken = true;
    } else {
        memory_write_halfword(n, (uint16_t) r[Rd]);
    }
    if (!P) n += (U ? imm : -imm);
    if ((!P || W) && (!L || Rd != Rn)) r[Rn] = n;

    return 1;
}

int arm_load_signed_halfword_or_signed_byte_register(void) {
    bool P = BIT(arm_op, 24);
    bool U = BIT(arm_op, 23);
    bool I = BIT(arm_op, 22);
    bool W = BIT(arm_op, 21);
    bool L = BIT(arm_op, 20);
    uint32_t Rn = BITS(arm_op, 16, 19);
    uint32_t Rd = BITS(arm_op, 12, 15);
    uint32_t sbz = BITS(arm_op, 8, 11);
    uint32_t opc = BITS(arm_op, 4, 7);
    uint32_t Rm = BITS(arm_op, 0, 3);

    bool T = !P && W;
    assert(!T);

#ifdef DEBUG
    if (log_instructions && log_arm_instructions && !FLAG_T()) {
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
    if (Rn == REG_PC) n &= ~3;
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
    if (Rd == REG_PC) branch_taken = true;
    if (!P) n += (U ? m : -m);
    if ((!P || W) && (!L || Rd != Rn)) r[Rn] = n;

    return 1;
}

int arm_load_signed_halfword_or_signed_byte_immediate(void) {
    bool P = BIT(arm_op, 24);
    bool U = BIT(arm_op, 23);
    bool I = BIT(arm_op, 22);
    bool W = BIT(arm_op, 21);
    bool L = BIT(arm_op, 20);
    uint32_t Rn = BITS(arm_op, 16, 19);
    uint32_t Rd = BITS(arm_op, 12, 15);
    uint32_t opc = BITS(arm_op, 4, 7);
    uint32_t imm = BITS(arm_op, 8, 11) << 4 | BITS(arm_op, 0, 3);

    bool T = !P && W;
    assert(!T);

#ifdef DEBUG
    if (log_instructions && log_arm_instructions && !FLAG_T()) {
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
    if (Rn == REG_PC) n &= ~3;
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
    if (Rd == REG_PC) branch_taken = true;
    if (!P) n += (U ? imm : -imm);
    if ((!P || W) && (!L || Rd != Rn)) r[Rn] = n;

    return 1;
}

int arm_special_data_processing_register(void) {
    bool R = BIT(arm_op, 22);
    bool b21 = BIT(arm_op, 21);
    uint32_t mask_type = BITS(arm_op, 16, 19);
    uint32_t Rd = BITS(arm_op, 12, 15);
    uint32_t Rm = BITS(arm_op, 0, 3);
    uint32_t sbz = BITS(arm_op, 4, 11);

#ifdef DEBUG
    if (log_instructions && log_arm_instructions && !FLAG_T()) {
        arm_print_opcode();
        if (b21) {
            print_mnemonic("msr");
            printf(R ? "spsr" : "cpsr");
            printf("_");
            switch (mask_type) {
                case 1: printf("c"); break;
                case 8: printf("f"); break;
                case 9: printf("cf"); break;
                default: assert(false); break;
            }
            printf(", ");
            print_register(Rm);
        } else {
            print_mnemonic("mrs");
            print_register(Rd);
            printf(", ");
            printf(R ? "spsr" : "cpsr");
        }
        printf("\n");
    }
#endif

    if (b21) {
        assert(Rd == 0xf);
        assert(sbz == 0);
    } else {
        assert(mask_type == 0xf);
        assert(sbz == 0 && Rm == 0);
    }

    if (b21) {
        uint32_t mask = 0;
        switch (mask_type) {
            case 1: mask = 0x000000ff; break;  // Allow bits T, M[4] to be set?
            case 8: mask = 0xf0000000; break;
            case 9: mask = 0xf00000ff; break;  // Allow bits T, M[4] to be set?
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
        assert(Rd != REG_PC);
    }

    return 1;
}

int arm_special_data_processing_immediate(void) {
    bool R = BIT(arm_op, 22);
    uint32_t mask_type = BITS(arm_op, 16, 19);
    uint32_t sbo = BITS(arm_op, 12, 15);
    uint32_t rot = BITS(arm_op, 8, 11);
    uint32_t imm = ROR(BITS(arm_op, 0, 7), 2 * rot);

#ifdef DEBUG
    if (log_instructions && log_arm_instructions && !FLAG_T()) {
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
        case 1: mask = 0x000000ff; break;  // Allow bits T, M[4] to be set?
        case 8: mask = 0xf0000000; break;
        case 9: mask = 0xf00000ff; break;  // Allow bits T, M[4] to be set?
        default: assert(false); break;
    }
    if (R) {
        write_spsr((read_spsr() & ~mask) | (imm & mask));
    } else {
        write_cpsr((cpsr & ~mask) | (imm & mask));
    }

    return 1;
}

int arm_swap(void) {
    bool B = BIT(arm_op, 22);
    uint32_t Rn = BITS(arm_op, 16, 19);
    uint32_t Rd = BITS(arm_op, 12, 15);
    uint32_t sbz = BITS(arm_op, 8, 11);
    uint32_t Rm = BITS(arm_op, 0, 3);

#ifdef DEBUG
    if (log_instructions && log_arm_instructions && !FLAG_T()) {
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
    assert(Rd != REG_PC);

    return 1;
}

int arm_branch_and_exchange(void) {
    uint32_t sbo = BITS(arm_op, 8, 19);
    uint32_t Rm = BITS(arm_op, 0, 3);

#ifdef DEBUG
    if (log_instructions && log_arm_instructions && !FLAG_T()) {
        arm_print_opcode();
        print_mnemonic("bx");
        print_register(Rm);
        //printf("  ; Rm = 0x%x", r[Rm]);
        printf("\n");
    }
#endif

    assert(sbo == 0xfff);
    assert(BITS(r[Rm], 0, 1) != 2);  // FIXME

    ASSIGN_T(BIT(r[Rm], 0));
    r[REG_PC] = r[Rm] & ~1;
    branch_taken = true;

    return 1;
}