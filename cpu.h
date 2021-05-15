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

extern bool log_instructions;
extern bool log_arm_instructions;
extern bool log_thumb_instructions;
extern bool log_registers;
extern bool halted;
extern uint64_t instruction_count;
extern uint32_t r[16];
extern uint32_t r14_svc;
extern uint32_t cpsr;
extern uint32_t spsr_svc;
extern bool branch_taken;
extern uint32_t arm_op;
extern uint16_t thumb_op;

uint8_t memory_read_byte(uint32_t address);
void memory_write_byte(uint32_t address, uint8_t value);
uint16_t memory_read_halfword(uint32_t address);
void memory_write_halfword(uint32_t address, uint16_t value);
uint32_t memory_read_word(uint32_t address);
void memory_write_word(uint32_t address, uint32_t value);

void arm_init(void);
uint32_t asr(uint32_t x, uint32_t n);
uint32_t ror(uint32_t x, uint32_t n);
uint32_t align_word(uint32_t address, uint32_t value);
uint32_t align_halfword(uint32_t address, uint16_t value);
uint32_t bit_count(uint32_t x);
uint32_t lowest_set_bit(uint32_t x);
void mode_change(uint32_t old_mode, uint32_t new_mode);
bool condition_passed(uint32_t cond);
void write_cpsr(uint32_t psr);
uint32_t read_spsr();
void write_spsr(uint32_t psr);
void arm_step(void);
void thumb_step(void);
void arm_init_lookup(void);
void thumb_init_lookup(void);
void arm_hardware_interrupt(void);
void print_psr(uint32_t psr);
void print_all_registers(void);
void arm_print_opcode(void);
void thumb_print_opcode(void);
void print_mnemonic(char *s);
void print_register(uint32_t i);
void print_immediate(uint32_t i);
void print_address(uint32_t i);
void print_shift_amount(uint32_t i);
void arm_print_shifter_op(uint32_t shop, uint32_t shamt, uint32_t shreg, uint32_t Rs);
bool arm_print_rlist(uint32_t rlist);
bool thumb_print_rlist(uint32_t rlist);

// cpu-arm.c
void arm_data_processing_register(void);
void arm_data_processing_immediate(void);
void arm_load_store_word_or_byte_immediate(void);
void arm_load_store_word_or_byte_register(void);
void arm_load_store_multiple(void);
void arm_branch(void);
void arm_software_interrupt(void);
void arm_multiply(void);
void arm_multiply_long(void);
void arm_load_store_halfword_register(void);
void arm_load_store_halfword_immediate(void);
void arm_load_signed_halfword_or_signed_byte_register(void);
void arm_load_signed_halfword_or_signed_byte_immediate(void);
void arm_special_data_processing_register(void);
void arm_special_data_processing_immediate(void);
void arm_swap(void);
void arm_branch_and_exchange(void);

// cpu-thumb.c
void thumb_shift_by_immediate(void);
void thumb_add_or_subtract_register(void);
void thumb_add_or_subtract_immediate(void);
void thumb_add_subtract_compare_or_move_immediate(void);
void thumb_data_processing_register(void);
void thumb_special_data_processing(void);
void thumb_branch_and_exchange(void);
void thumb_load_from_literal_pool(void);
void thumb_load_store_register(void);
void thumb_load_store_word_or_byte_immediate(void);
void thumb_load_store_halfword_immediate(void);
void thumb_load_store_to_or_from_stack(void);
void thumb_add_to_sp_or_pc(void);
void thumb_adjust_stack_pointer(void);
void thumb_push_or_pop_register_list(void);
void thumb_load_store_multiple(void);
void thumb_conditional_branch(void);
void thumb_software_interrupt(void);
void thumb_unconditional_branch(void);
void thumb_branch_with_link_prefix(void);
void thumb_branch_with_link_suffix(void);
