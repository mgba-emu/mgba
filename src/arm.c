#include "arm.h"

#define ARM_SIGN(I) ((I) >> 31)
#define ARM_ROR(I, ROTATE) (((I) >> ROTATE) | (I << (32 - ROTATE)))

static inline void _ARMSetMode(struct ARMCore*, enum ExecutionMode);
static ARMInstruction _ARMLoadInstructionARM(struct ARMMemory*, uint32_t address, uint32_t* opcodeOut);
static ARMInstruction _ARMLoadInstructionThumb(struct ARMMemory*, uint32_t address, uint32_t* opcodeOut);

static inline void _ARMReadCPSR(struct ARMCore* cpu) {
	_ARMSetMode(cpu, cpu->cpsr.t);
}

static inline int _ARMModeHasSPSR(enum PrivilegeMode mode) {
	return mode != MODE_SYSTEM && mode != MODE_USER;
}

static inline void _barrelShift(struct ARMCore* cpu, uint32_t opcode) {
	// TODO
}

static inline void _immediate(struct ARMCore* cpu, uint32_t opcode) {
	int rotate = (opcode & 0x00000F00) >> 7;
	int immediate = opcode & 0x000000FF;
	if (!rotate) {
		cpu->shifterOperand = immediate;
		cpu->shifterCarryOut = cpu->cpsr.c;
	} else {
		cpu->shifterOperand = ARM_ROR(immediate, rotate);
		cpu->shifterCarryOut = ARM_SIGN(cpu->shifterOperand);
	}
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

#define ARM_CARRY_FROM(M, N, D) ((ARM_SIGN((M) | (N))) && !(ARM_SIGN(D)))
#define ARM_BORROW_FROM(M, N, D) (((uint32_t) (M)) >= ((uint32_t) (N)))
#define ARM_V_ADDITION(M, N, D) (!(ARM_SIGN((M) ^ (N))) && (ARM_SIGN((M) ^ (D))) && (ARM_SIGN((N) ^ (D))))
#define ARM_V_SUBTRACTION(M, N, D) ((ARM_SIGN((M) ^ (N))) && (ARM_SIGN((M) ^ (D))))

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
		cpu->cpsr.n = ARM_SIGN(D); \
		cpu->cpsr.z = !(D); \
		cpu->cpsr.c = ARM_CARRY_FROM(M, N, D); \
		cpu->cpsr.v = ARM_V_ADDITION(M, N, D); \
	}

#define ARM_SUBTRACTION_S(M, N, D) \
	if (rd == ARM_PC && _ARMModeHasSPSR(cpu->cpsr.priv)) { \
		cpu->cpsr = cpu->spsr; \
		_ARMReadCPSR(cpu); \
	} else { \
		cpu->cpsr.n = ARM_SIGN(D); \
		cpu->cpsr.z = !(D); \
		cpu->cpsr.c = ARM_BORROW_FROM(M, N, D); \
		cpu->cpsr.v = ARM_V_SUBTRACTION(M, N, D); \
	}

#define ARM_NEUTRAL_S(M, N, D) \
	if (rd == ARM_PC && _ARMModeHasSPSR(cpu->cpsr.priv)) { \
		cpu->cpsr = cpu->spsr; \
		_ARMReadCPSR(cpu); \
	} else { \
		cpu->cpsr.n = ARM_SIGN(D); \
		cpu->cpsr.z = !(D); \
		cpu->cpsr.c = cpu->shifterCarryOut; \
	}

#define DEFINE_INSTRUCTION_EX_ARM(NAME, COND, COND_BODY, BODY) \
	static void _ARMInstruction ## NAME ## COND (struct ARMCore* cpu, uint32_t opcode) { \
		if (!COND_BODY) { \
			return; \
		} \
		BODY; \
	}

#define DEFINE_INSTRUCTION_ARM(NAME, BODY) \
	DEFINE_INSTRUCTION_EX_ARM(NAME, EQ, ARM_COND_EQ, BODY) \
	DEFINE_INSTRUCTION_EX_ARM(NAME, NE, ARM_COND_NE, BODY) \
	DEFINE_INSTRUCTION_EX_ARM(NAME, CS, ARM_COND_CS, BODY) \
	DEFINE_INSTRUCTION_EX_ARM(NAME, CC, ARM_COND_CC, BODY) \
	DEFINE_INSTRUCTION_EX_ARM(NAME, MI, ARM_COND_MI, BODY) \
	DEFINE_INSTRUCTION_EX_ARM(NAME, PL, ARM_COND_PL, BODY) \
	DEFINE_INSTRUCTION_EX_ARM(NAME, VS, ARM_COND_VS, BODY) \
	DEFINE_INSTRUCTION_EX_ARM(NAME, VC, ARM_COND_VC, BODY) \
	DEFINE_INSTRUCTION_EX_ARM(NAME, HI, ARM_COND_HI, BODY) \
	DEFINE_INSTRUCTION_EX_ARM(NAME, LS, ARM_COND_LS, BODY) \
	DEFINE_INSTRUCTION_EX_ARM(NAME, GE, ARM_COND_GE, BODY) \
	DEFINE_INSTRUCTION_EX_ARM(NAME, LT, ARM_COND_LT, BODY) \
	DEFINE_INSTRUCTION_EX_ARM(NAME, GT, ARM_COND_GT, BODY) \
	DEFINE_INSTRUCTION_EX_ARM(NAME, LE, ARM_COND_LE, BODY) \
	DEFINE_INSTRUCTION_EX_ARM(NAME, AL, ARM_COND_AL, BODY)

#define DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, S_BODY, SHIFTER, BODY, POST_BODY) \
	DEFINE_INSTRUCTION_ARM(NAME, \
		int rd = (opcode >> 12) & 0xF; \
		int rn = (opcode >> 16) & 0xF; \
		SHIFTER(cpu, opcode); \
		BODY; \
		S_BODY; \
		POST_BODY;)

#define DEFINE_ALU_INSTRUCTION_ARM(NAME, S_BODY, BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, , _barrelShift, BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## S, S_BODY, _barrelShift, BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## I, , _immediate, BODY, POST_BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## SI, S_BODY, _immediate, BODY, POST_BODY)

// Begin ALU definitions

DEFINE_ALU_INSTRUCTION_ARM(ADD, ARM_ADDITION_S(cpu->gprs[rn], cpu->shifterOperand, cpu->gprs[rd]), \
	cpu->gprs[rd] = cpu->gprs[rn] + cpu->shifterOperand;, )

DEFINE_ALU_INSTRUCTION_ARM(ADC, ARM_ADDITION_S(cpu->gprs[rn], shifterOperand, cpu->gprs[rd]), \
	int32_t shifterOperand = cpu->shifterOperand + cpu->cpsr.c; \
	cpu->gprs[rd] = cpu->gprs[rn] + shifterOperand;, )

DEFINE_ALU_INSTRUCTION_ARM(AND, ARM_NEUTRAL_S(cpu->gprs[rn], cpu->shifterOperand, cpu->gprs[rd]), \
	cpu->gprs[rd] = cpu->gprs[rn] & cpu->shifterOperand;, )

DEFINE_ALU_INSTRUCTION_ARM(BIC, ARM_NEUTRAL_S(cpu->gprs[rn], cpu->shifterOperand, cpu->gprs[rd]), \
	cpu->gprs[rd] = cpu->gprs[rn] & ~cpu->shifterOperand;, )

DEFINE_ALU_INSTRUCTION_ARM(CMN, ARM_ADDITION_S(cpu->gprs[rn], cpu->shifterOperand, aluOut), \
	int32_t aluOut = cpu->gprs[rn] + cpu->shifterOperand;, )

DEFINE_ALU_INSTRUCTION_ARM(CMP, ARM_SUBTRACTION_S(cpu->gprs[rn], cpu->shifterOperand, aluOut), \
	int32_t aluOut = cpu->gprs[rn] - cpu->shifterOperand;, )

DEFINE_ALU_INSTRUCTION_ARM(EOR, ARM_NEUTRAL_S(cpu->gprs[rn], cpu->shifterOperand, cpu->gprs[rd]), \
	cpu->gprs[rd] = cpu->gprs[rn] ^ cpu->shifterOperand;, )

DEFINE_ALU_INSTRUCTION_ARM(MOV, ARM_NEUTRAL_S(cpu->gprs[rn], cpu->shifterOperand, cpu->gprs[rd]), \
	cpu->gprs[rd] = cpu->shifterOperand;, )

DEFINE_ALU_INSTRUCTION_ARM(MVN, ARM_NEUTRAL_S(cpu->gprs[rn], cpu->shifterOperand, cpu->gprs[rd]), \
	cpu->gprs[rd] = ~cpu->shifterOperand;, )

DEFINE_ALU_INSTRUCTION_ARM(ORR, ARM_NEUTRAL_S(cpu->gprs[rn], cpu->shifterOperand, cpu->gprs[rd]), \
	cpu->gprs[rd] = cpu->gprs[rn] | cpu->shifterOperand;, )

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

DEFINE_ALU_INSTRUCTION_ARM(TEQ, ARM_NEUTRAL_S(cpu->gprs[rn], cpu->shifterOperand, aluOut), \
	int32_t aluOut = cpu->gprs[rn] ^ cpu->shifterOperand;, )

DEFINE_ALU_INSTRUCTION_ARM(TST, ARM_NEUTRAL_S(cpu->gprs[rn], cpu->shifterOperand, aluOut), \
	int32_t aluOut = cpu->gprs[rn] & cpu->shifterOperand;, )

// End ALU definitions

// Begin multiply definitions

DEFINE_INSTRUCTION_ARM(MLA,)
DEFINE_INSTRUCTION_ARM(MLAS,)
DEFINE_INSTRUCTION_ARM(MUL,)
DEFINE_INSTRUCTION_ARM(MULS,)
DEFINE_INSTRUCTION_ARM(SMLAL,)
DEFINE_INSTRUCTION_ARM(SMLALS,)
DEFINE_INSTRUCTION_ARM(SMULL,)
DEFINE_INSTRUCTION_ARM(SMULLS,)
DEFINE_INSTRUCTION_ARM(UMLAL,)
DEFINE_INSTRUCTION_ARM(UMLALS,)
DEFINE_INSTRUCTION_ARM(UMULL,)
DEFINE_INSTRUCTION_ARM(UMULLS,)

// End multiply definitions

// Begin load/store definitions

DEFINE_INSTRUCTION_ARM(LDR,)
DEFINE_INSTRUCTION_ARM(LDRB,)
DEFINE_INSTRUCTION_ARM(LDRH,)
DEFINE_INSTRUCTION_ARM(LDRSB,)
DEFINE_INSTRUCTION_ARM(LDRSH,)
DEFINE_INSTRUCTION_ARM(STR,)
DEFINE_INSTRUCTION_ARM(STRB,)
DEFINE_INSTRUCTION_ARM(STRH,)

DEFINE_INSTRUCTION_ARM(SWP,)
DEFINE_INSTRUCTION_ARM(SWPB,)

// End load/store definitions

// TODO
DEFINE_INSTRUCTION_ARM(ILL,) // Illegal opcode
DEFINE_INSTRUCTION_ARM(MSR,)
DEFINE_INSTRUCTION_ARM(MRS,)
DEFINE_INSTRUCTION_ARM(MSRI,)
DEFINE_INSTRUCTION_ARM(MRSI,)

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

#define DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, ALU) \
	DO_8(DECLARE_INSTRUCTION_ARM(COND, ALU ## I)), \
	DO_8(DECLARE_INSTRUCTION_ARM(COND, ALU ## I))

#define DECLARE_ARM_ALU_BLOCK(COND, ALU, EX1, EX2, EX3, EX4) \
	DO_8(DECLARE_INSTRUCTION_ARM(COND, ALU)), \
	DECLARE_INSTRUCTION_ARM(COND, ALU), \
	DECLARE_INSTRUCTION_ARM(COND, EX1), \
	DECLARE_INSTRUCTION_ARM(COND, ALU), \
	DECLARE_INSTRUCTION_ARM(COND, EX2), \
	DECLARE_INSTRUCTION_ARM(COND, ALU), \
	DECLARE_INSTRUCTION_ARM(COND, EX3), \
	DECLARE_INSTRUCTION_ARM(COND, ALU), \
	DECLARE_INSTRUCTION_ARM(COND, EX4)

#define DECLARE_COND_BLOCK(COND) \
	DECLARE_ARM_ALU_BLOCK(COND, AND, MUL, STRH, ILL, ILL), \
	DECLARE_ARM_ALU_BLOCK(COND, ANDS, MULS, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, EOR, MLA, STRH, ILL, ILL), \
	DECLARE_ARM_ALU_BLOCK(COND, EORS, MLAS, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, SUB, ILL, STRH, ILL, ILL), \
	DECLARE_ARM_ALU_BLOCK(COND, SUBS, ILL, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, RSB, ILL, STRH, ILL, ILL), \
	DECLARE_ARM_ALU_BLOCK(COND, RSBS, ILL, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, ADD, UMULL, STRH, ILL, ILL), \
	DECLARE_ARM_ALU_BLOCK(COND, ADDS, UMULLS, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, ADC, UMLAL, STRH, ILL, ILL), \
	DECLARE_ARM_ALU_BLOCK(COND, ADCS, UMLALS, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, SBC, SMULL, STRH, ILL, ILL), \
	DECLARE_ARM_ALU_BLOCK(COND, SBCS, SMULLS, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, RSC, SMLAL, STRH, ILL, ILL), \
	DECLARE_ARM_ALU_BLOCK(COND, RSCS, SMLALS, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, MRS, SWP, STRH, ILL, ILL), \
	DECLARE_ARM_ALU_BLOCK(COND, TST, ILL, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, MSR, ILL, STRH, ILL, ILL), \
	DECLARE_ARM_ALU_BLOCK(COND, TEQ, ILL, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, MRS, SWPB, STRH, ILL, ILL), \
	DECLARE_ARM_ALU_BLOCK(COND, CMP, ILL, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, MSR, ILL, STRH, ILL, ILL), \
	DECLARE_ARM_ALU_BLOCK(COND, CMN, ILL, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, ORR, SMLAL, STRH, ILL, ILL), \
	DECLARE_ARM_ALU_BLOCK(COND, ORRS, SMLALS, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, MOV, SMLAL, STRH, ILL, ILL), \
	DECLARE_ARM_ALU_BLOCK(COND, MOVS, SMLALS, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, BIC, SMLAL, STRH, ILL, ILL), \
	DECLARE_ARM_ALU_BLOCK(COND, BICS, SMLALS, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_BLOCK(COND, MVN, SMLAL, STRH, ILL, ILL), \
	DECLARE_ARM_ALU_BLOCK(COND, MVNS, SMLALS, LDRH, LDRSB, LDRSH), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, AND), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, ANDS), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, EOR), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, EORS), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, SUB), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, SUBS), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, RSB), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, RSBS), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, ADD), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, ADDS), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, ADC), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, ADCS), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, SBC), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, SBCS), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, RSC), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, RSCS), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, MRS), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, TST), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, MSR), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, TEQ), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, MRS), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, CMP), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, MSR), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, CMN), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, ORR), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, ORRS), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, MOV), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, MOVS), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, BIC), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, BICS), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, MVN), \
	DECLARE_ARM_ALU_IMMEDIATE_BLOCK(COND, MVNS)

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