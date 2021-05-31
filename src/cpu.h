#ifndef CPU_H
#define CPU_H

#define BIT(x, i) (((x) >> (i)) & 1)
#define BITS(x, i, j) (((x) >> (i)) & ((1 << ((j) - (i) + 1)) - 1))

#define ASR(x, i) (((x) & (1 << 31)) != 0 ? ~(~(x) >> (i)) : (x) >> (i))
#define ROR(x, i) (((x) >> (i)) | ((x) << (32 - (i))))
#define ZERO_EXTEND(x, i) if ((x) & (1 << (i))) { (x) |= ~((1 << ((i) + 1)) - 1); }

#define FLAG_C() (cpsr & PSR_C ? true : false)
#define FLAG_T() (cpsr & PSR_T ? true : false)

#define ASSIGN_N(x) if ((x)) { cpsr |= PSR_N; } else { cpsr &= ~PSR_N; }
#define ASSIGN_Z(x) if ((x)) { cpsr |= PSR_Z; } else { cpsr &= ~PSR_Z; }
#define ASSIGN_C(x) if ((x)) { cpsr |= PSR_C; } else { cpsr &= ~PSR_C; }
#define ASSIGN_V(x) if ((x)) { cpsr |= PSR_V; } else { cpsr &= ~PSR_V; }
#define ASSIGN_T(x) if ((x)) { cpsr |= PSR_T; } else { cpsr &= ~PSR_T; }

#define CLEAR_C() cpsr &= ~PSR_C

#define SIZEOF_INSTR (cpsr & PSR_T ? 2 : 4)

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

#define REG_SP 13
#define REG_LR 14
#define REG_PC 15

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

#define VEC_RESET          0
#define VEC_UNDEF          4
#define VEC_SWI            8
#define VEC_ABORT_PREFETCH 0xc
#define VEC_ABORT_DATA     0x10
#define VEC_IRQ            0x18
#define VEC_FIQ            0x1c

extern bool log_instructions;
extern bool log_arm_instructions;
extern bool log_thumb_instructions;
extern bool log_registers;
extern uint64_t instruction_count;
extern uint32_t r[16];
extern uint32_t r14_irq;
extern uint32_t r14_svc;
extern uint32_t cpsr;
extern uint32_t spsr_irq, spsr_svc;
extern bool branch_taken;
extern uint32_t arm_op;
extern uint32_t arm_pipeline[2];
extern uint16_t thumb_op;
extern uint16_t thumb_pipeline[2];

uint8_t memory_read_byte(uint32_t address);
void memory_write_byte(uint32_t address, uint8_t value);
uint16_t memory_read_halfword(uint32_t address);
void memory_write_halfword(uint32_t address, uint16_t value);
uint32_t memory_read_word(uint32_t address);
void memory_write_word(uint32_t address, uint32_t value);

void arm_init_registers(bool skip_bios);
uint32_t align_word(uint32_t address, uint32_t value);
uint32_t align_halfword(uint32_t address, uint16_t value);
void mode_change(uint32_t old_mode, uint32_t new_mode);
bool condition_passed(uint32_t cond);
void write_cpsr(uint32_t psr);
uint32_t read_spsr();
void write_spsr(uint32_t psr);
int arm_step(void);
int thumb_step(void);
void arm_init_lookup(void);
void thumb_init_lookup(void);
void print_psr(uint32_t psr);
void print_all_registers(void);
void arm_print_opcode(void);
void thumb_print_opcode(void);
void print_mnemonic(const char *s);
void print_mnemonic_d(const char *s, char *t);
void print_register(uint32_t i);
void print_immediate(uint32_t i);
void print_address(uint32_t i);
void print_shift_amount(uint32_t i);
void arm_print_shifter_op(uint32_t shop, uint32_t shamt, uint32_t shreg, uint32_t Rs);
bool arm_print_rlist(uint32_t rlist);
bool thumb_print_rlist(uint32_t rlist);

// cpu-arm.c
int arm_data_processing_register(void);
int arm_data_processing_immediate(void);
int arm_load_store_word_or_byte_register(void);
int arm_load_store_word_or_byte_immediate(void);
int arm_load_store_multiple(void);
int arm_branch(void);
int arm_software_interrupt(void);
int arm_hardware_interrupt(void);
int arm_multiply(void);
int arm_multiply_long(void);
int arm_load_store_halfword_register(void);
int arm_load_store_halfword_immediate(void);
int arm_load_signed_halfword_or_signed_byte_register(void);
int arm_load_signed_halfword_or_signed_byte_immediate(void);
int arm_special_data_processing_register(void);
int arm_special_data_processing_immediate(void);
int arm_swap(void);
int arm_branch_and_exchange(void);

// cpu-thumb.c
int thumb_shift_by_immediate(void);
int thumb_add_or_subtract_register(void);
int thumb_add_or_subtract_immediate(void);
int thumb_add_subtract_compare_or_move_immediate(void);
int thumb_data_processing_register(void);
int thumb_special_data_processing(void);
int thumb_branch_and_exchange(void);
int thumb_load_from_literal_pool(void);
int thumb_load_store_register(void);
int thumb_load_store_word_or_byte_immediate(void);
int thumb_load_store_halfword_immediate(void);
int thumb_load_store_to_or_from_stack(void);
int thumb_add_to_sp_or_pc(void);
int thumb_adjust_stack_pointer(void);
int thumb_push_or_pop_register_list(void);
int thumb_load_store_multiple(void);
int thumb_conditional_branch(void);
int thumb_software_interrupt(void);
int thumb_unconditional_branch(void);
int thumb_branch_with_link_prefix(void);
int thumb_branch_with_link_suffix(void);

#endif  // CPU_H