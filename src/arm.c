#include "arm.h"

static inline void _ARMSetMode(struct ARMCore*, enum ExecutionMode);
static ARMInstruction _ARMLoadInstructionARM(struct ARMMemory*, uint32_t address, uint32_t* opcodeOut);
static ARMInstruction _ARMLoadInstructionThumb(struct ARMMemory*, uint32_t address, uint32_t* opcodeOut);

static inline void _ARMReadCPSR(struct ARMCore* cpu) {
	_ARMSetMode(cpu, cpu->cpsr.t);
}

static inline int _ARMModeHasSPSR(enum PrivilegeMode mode) {
	return mode != MODE_SYSTEM && mode != MODE_USER;
}

static const ARMInstruction armTable[0xF000];

static inline void _ARMSetMode(struct ARMCore* cpu, enum ExecutionMode executionMode) {
	if (executionMode == cpu->executionMode) {
		return;
	}

	cpu->executionMode = executionMode;
	switch (executionMode) {
	case MODE_ARM:
		cpu->cpsr.t = 0;
		cpu->instructionWidth = WORD_SIZE_ARM;
		cpu->loadInstruction = _ARMLoadInstructionARM;
		break;
	case MODE_THUMB:
		cpu->cpsr.t = 1;
		cpu->instructionWidth = WORD_SIZE_THUMB;
		cpu->loadInstruction = _ARMLoadInstructionThumb;
	}
}

static ARMInstruction _ARMLoadInstructionARM(struct ARMMemory* memory, uint32_t address, uint32_t* opcodeOut) {
	uint32_t opcode = memory->load32(memory, address);
	*opcodeOut = opcode;
	return 0;
}

static ARMInstruction _ARMLoadInstructionThumb(struct ARMMemory* memory, uint32_t address, uint32_t* opcodeOut) {
	uint16_t opcode = memory->loadU16(memory, address);
	*opcodeOut = opcode;
	return 0;
}

void ARMInit(struct ARMCore* cpu) {
	int i;
	for (i = 0; i < 16; ++i) {
		cpu->gprs[i] = 0;
	}

	cpu->cpsr.packed = MODE_SYSTEM;
	cpu->spsr.packed = 0;

	cpu->cyclesToEvent = 0;

	cpu->shifterOperand = 0;
	cpu->shifterCarryOut = 0;

	cpu->memory = 0;
	cpu->board = 0;

	cpu->executionMode = MODE_THUMB;
	_ARMSetMode(cpu, MODE_ARM);
}

void ARMAssociateMemory(struct ARMCore* cpu, struct ARMMemory* memory) {
	cpu->memory = memory;
}

inline void ARMCycle(struct ARMCore* cpu) {
	// TODO
	uint32_t opcode;
	ARMInstruction instruction = cpu->loadInstruction(cpu->memory, cpu->gprs[ARM_PC] - cpu->instructionWidth, &opcode);
	cpu->gprs[ARM_PC] += cpu->instructionWidth;
	instruction(cpu, opcode);
}

// Instruction definitions
// Beware pre-processor antics

#define ARM_CARRY_FROM(M, N, D) ((((M) | (N)) >> 31) && !((D) >> 31))
#define ARM_BORROW_FROM(M, N, D) (((uint32_t) (M)) >= ((uint32_t) (N)))
#define ARM_V_ADDITION(M, N, D) (!(((M) ^ (N)) >> 31) && (((M) ^ (D)) >> 31) && (((N) ^ (D)) >> 31))
#define ARM_V_SUBTRACTION(M, N, D) ((((M) ^ (N)) >> 31) && (((M) ^ (D)) >> 31))

#define ARM_COND_EQ (cpu->cpsr.z)
#define ARM_COND_NE (!cpu->cpsr.z)
#define ARM_COND_CS (cpu->cpsr.c)
#define ARM_COND_CC (!cpu->cpsr.c)
#define ARM_COND_MI (cpu->cpsr.n)
#define ARM_COND_PL (!cpu->cpsr.n)
#define ARM_COND_VS (cpu->cpsr.v)
#define ARM_COND_VC (!cpu->cpsr.v)
#define ARM_COND_HI (cpu->cpsr.c && !cpu->cpsr.z)
#define ARM_COND_LS (!cpu->cpsr.c || cpu->cpsr.z)
#define ARM_COND_GE (!cpu->cpsr.n == !cpu->cpsr.v)
#define ARM_COND_LT (!cpu->cpsr.n != !cpu->cpsr.v)
#define ARM_COND_GT (!cpu->cpsr.z && !cpu->cpsr.n == !cpu->cpsr.v)
#define ARM_COND_LE (cpu->cpsr.z || !cpu->cpsr.n != !cpu->cpsr.v)
#define ARM_COND_AL 1

#define ARM_ADDITION_S(M, N, D) \
	if (rd == ARM_PC && _ARMModeHasSPSR(cpu->cpsr.priv)) { \
		cpu->cpsr = cpu->spsr; \
		_ARMReadCPSR(cpu); \
	} else { \
		cpu->cpsr.n = (D) >> 31; \
		cpu->cpsr.z = !(D); \
		cpu->cpsr.c = ARM_CARRY_FROM(M, N, D); \
		cpu->cpsr.v = ARM_V_ADDITION(M, N, D); \
	}

#define ARM_SUBTRACTION_S(M, N, D) \
	if (rd == ARM_PC && _ARMModeHasSPSR(cpu->cpsr.priv)) { \
		cpu->cpsr = cpu->spsr; \
		_ARMReadCPSR(cpu); \
	} else { \
		cpu->cpsr.n = (D) >> 31; \
		cpu->cpsr.z = !(D); \
		cpu->cpsr.c = ARM_BORROW_FROM(M, N, D); \
		cpu->cpsr.v = ARM_V_SUBTRACTION(M, N, D); \
	}

#define ARM_NEUTRAL_S(M, N, D) \
	if (rd == ARM_PC && _ARMModeHasSPSR(cpu->cpsr.priv)) { \
		cpu->cpsr = cpu->spsr; \
		_ARMReadCPSR(cpu); \
	} else { \
		cpu->cpsr.n = (D) >> 31; \
		cpu->cpsr.z = !(D); \
		cpu->cpsr.c = cpu->shifterCarryOut; \
	}

// TODO: shifter
#define DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, COND, COND_BODY, S, S_BODY, BODY, POST_BODY) \
	static void _ARMInstruction ## NAME ## S ## COND (struct ARMCore* cpu, uint32_t opcode) { \
		if (!COND_BODY) { \
			return; \
		} \
		int rd = (opcode >> 12) & 0xF; \
		int rn = (opcode >> 16) & 0xF; \
		BODY; \
		S_BODY; \
		POST_BODY; \
	}

#define DEFINE_ALU_INSTRUCTION_ARM(NAME, S_BODY, BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, EQ, ARM_COND_EQ, , , BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, NE, ARM_COND_NE, , , BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, CS, ARM_COND_CS, , , BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, CC, ARM_COND_CC, , , BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, MI, ARM_COND_MI, , , BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, PL, ARM_COND_PL, , , BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, VS, ARM_COND_VS, , , BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, VC, ARM_COND_VC, , , BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, HI, ARM_COND_HI, , , BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, LS, ARM_COND_LS, , , BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, GE, ARM_COND_GE, , , BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, LT, ARM_COND_LT, , , BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, GT, ARM_COND_GT, , , BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, LE, ARM_COND_LE, , , BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, AL, ARM_COND_AL, , , BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, EQ, ARM_COND_EQ, S, S_BODY, BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, NE, ARM_COND_NE, S, S_BODY, BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, CS, ARM_COND_CS, S, S_BODY, BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, CC, ARM_COND_CC, S, S_BODY, BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, MI, ARM_COND_MI, S, S_BODY, BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, PL, ARM_COND_PL, S, S_BODY, BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, VS, ARM_COND_VS, S, S_BODY, BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, VC, ARM_COND_VC, S, S_BODY, BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, HI, ARM_COND_HI, S, S_BODY, BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, LS, ARM_COND_LS, S, S_BODY, BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, GE, ARM_COND_GE, S, S_BODY, BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, LT, ARM_COND_LT, S, S_BODY, BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, GT, ARM_COND_GT, S, S_BODY, BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, LE, ARM_COND_LE, S, S_BODY, BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, AL, ARM_COND_AL, S, S_BODY, BODY, POST_BODY)

// Begin ALU definitions

DEFINE_ALU_INSTRUCTION_ARM(ADD, ARM_ADDITION_S(cpu->gprs[rn], cpu->shifterOperand, cpu->gprs[rd]), \
	cpu->gprs[rd] = cpu->gprs[rn] + cpu->shifterOperand;, )

DEFINE_ALU_INSTRUCTION_ARM(ADC, ARM_ADDITION_S(cpu->gprs[rn], shifterOperand, cpu->gprs[rd]), \
	int32_t shifterOperand = cpu->shifterOperand + cpu->cpsr.c; \
	cpu->gprs[rd] = cpu->gprs[rn] + shifterOperand;, )

DEFINE_ALU_INSTRUCTION_ARM(AND, ARM_NEUTRAL_S(cpu->gprs[rn], cpu->shifterOperand, cpu->gprs[rd]), \
	cpu->gprs[rd] = cpu->gprs[rn] & cpu->shifterOperand;, )

DEFINE_ALU_INSTRUCTION_ARM(EOR, ARM_NEUTRAL_S(cpu->gprs[rn], cpu->shifterOperand, cpu->gprs[rd]), \
	cpu->gprs[rd] = cpu->gprs[rn] ^ cpu->shifterOperand;, )

DEFINE_ALU_INSTRUCTION_ARM(RSB, ARM_SUBTRACTION_S(cpu->shifterOperand, cpu->gprs[rn], d), \
	int32_t d = cpu->shifterOperand - cpu->gprs[rn];, cpu->gprs[rd] = d)

DEFINE_ALU_INSTRUCTION_ARM(RSC, ARM_SUBTRACTION_S(cpu->shifterOperand, n, d), \
	int32_t n = cpu->gprs[rn] + !cpu->cpsr.c; \
	int32_t d = cpu->shifterOperand - n;, cpu->gprs[rd] = d)

DEFINE_ALU_INSTRUCTION_ARM(SBC, ARM_SUBTRACTION_S(cpu->gprs[rn], shifterOperand, d), \
	int32_t shifterOperand = cpu->shifterOperand + !cpu->cpsr.c; \
	int32_t d = cpu->gprs[rn] - shifterOperand;, cpu->gprs[rd] = d)

DEFINE_ALU_INSTRUCTION_ARM(SUB, ARM_SUBTRACTION_S(cpu->gprs[rn], cpu->shifterOperand, d), \
	int32_t d = cpu->gprs[rn] - cpu->shifterOperand;, cpu->gprs[rd] = d)

// End ALU definitions

#define DECLARE_INSTRUCTION_ARM(COND, NAME) \
	_ARMInstruction ## NAME ## COND

#define DO_8(DIRECTIVE) \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE \

// TODO: MUL
#define DECLARE_ARM_ALU_BLOCK(COND, ALU, EX1, EX2, EX3, EX4) \
	DO_8(DECLARE_INSTRUCTION_ARM(COND, ALU)), \
	DECLARE_INSTRUCTION_ARM(COND, ALU), \
	0, \
	DECLARE_INSTRUCTION_ARM(COND, ALU), \
	0, \
	DECLARE_INSTRUCTION_ARM(COND, ALU), \
	0, \
	DECLARE_INSTRUCTION_ARM(COND, ALU), \
	0

#define DECLARE_COND_BLOCK(COND) \
	DECLARE_ARM_ALU_BLOCK(COND, AND, MUL, STRH, 0, 0), \
	DECLARE_ARM_ALU_BLOCK(COND, ANDS, MULS, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, EOR, MLA, STRH, 0, 0), \
	DECLARE_ARM_ALU_BLOCK(COND, EORS, MLAS, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, SUB, 0, STRH, 0, 0), \
	DECLARE_ARM_ALU_BLOCK(COND, SUBS, 0, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, RSB, 0, STRH, 0, 0), \
	DECLARE_ARM_ALU_BLOCK(COND, RSBS, 0, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, ADD, UMULL, STRH, 0, 0), \
	DECLARE_ARM_ALU_BLOCK(COND, ADDS, UMULLS, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, ADC, UMLAL, STRH, 0, 0), \
	DECLARE_ARM_ALU_BLOCK(COND, ADCS, UMLALS, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, SBC, SMULL, STRH, 0, 0), \
	DECLARE_ARM_ALU_BLOCK(COND, SBCS, SMULLS, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, RSC, SMLAL, STRH, 0, 0), \
	DECLARE_ARM_ALU_BLOCK(COND, RSCS, SMLALS, LDRH, LDRSB, LDRSH)

static const ARMInstruction armTable[0xF000] = {
	DECLARE_COND_BLOCK(EQ),
	DECLARE_COND_BLOCK(NE),
	DECLARE_COND_BLOCK(CS),
	DECLARE_COND_BLOCK(CC),
	DECLARE_COND_BLOCK(MI),
	DECLARE_COND_BLOCK(PL),
	DECLARE_COND_BLOCK(VS),
	DECLARE_COND_BLOCK(VC),
	DECLARE_COND_BLOCK(HI),
	DECLARE_COND_BLOCK(LS),
	DECLARE_COND_BLOCK(GE),
	DECLARE_COND_BLOCK(LT),
	DECLARE_COND_BLOCK(GT),
	DECLARE_COND_BLOCK(LE),
	DECLARE_COND_BLOCK(AL)
};