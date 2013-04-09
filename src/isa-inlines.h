#ifndef ISA_INLINES_H
#define ISA_INLINES_H

#define UNUSED(V) (void)(V)

#define DO_8(DIRECTIVE) \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE, \
	DIRECTIVE

#define DO_256(DIRECTIVE) \
	DO_8(DO_8(DIRECTIVE)), \
	DO_8(DO_8(DIRECTIVE)), \
	DO_8(DO_8(DIRECTIVE)), \
	DO_8(DO_8(DIRECTIVE))

#define DO_INTERLACE(LEFT, RIGHT) \
	LEFT, \
	RIGHT

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

#define ARM_SIGN(I) ((I) >> 31)
#define ARM_ROR(I, ROTATE) (((I) >> ROTATE) | (I << (32 - ROTATE)))

#define ARM_CARRY_FROM(M, N, D) ((ARM_SIGN((M) | (N))) && !(ARM_SIGN(D)))
#define ARM_BORROW_FROM(M, N, D) (((uint32_t) (M)) >= ((uint32_t) (N)))
#define ARM_V_ADDITION(M, N, D) (!(ARM_SIGN((M) ^ (N))) && (ARM_SIGN((M) ^ (D))) && (ARM_SIGN((N) ^ (D))))
#define ARM_V_SUBTRACTION(M, N, D) ((ARM_SIGN((M) ^ (N))) && (ARM_SIGN((M) ^ (D))))

static inline int _ARMModeHasSPSR(enum PrivilegeMode mode) {
	return mode != MODE_SYSTEM && mode != MODE_USER;
}

static inline void _ARMSetMode(struct ARMCore* cpu, enum ExecutionMode executionMode) {
	if (executionMode == cpu->executionMode) {
		return;
	}

	cpu->executionMode = executionMode;
	switch (executionMode) {
	case MODE_ARM:
		cpu->cpsr.t = 0;
		break;
	case MODE_THUMB:
		cpu->cpsr.t = 1;
	}
}

static inline void _ARMReadCPSR(struct ARMCore* cpu) {
	_ARMSetMode(cpu, cpu->cpsr.t);
	ARMSetPrivilegeMode(cpu, cpu->cpsr.priv);
}

#endif
