#include "arm.h"

#include "isa-arm.h"
#include "isa-inlines.h"
#include "isa-thumb.h"

#include <limits.h>

static inline enum RegisterBank _ARMSelectBank(enum PrivilegeMode);

void ARMSetPrivilegeMode(struct ARMCore* cpu, enum PrivilegeMode mode) {
	if (mode == cpu->privilegeMode) {
		// Not switching modes after all
		return;
	}

	enum RegisterBank newBank = _ARMSelectBank(mode);
	enum RegisterBank oldBank = _ARMSelectBank(cpu->privilegeMode);
	if (newBank != oldBank) {
		// Switch banked registers
		if (mode == MODE_FIQ || cpu->privilegeMode == MODE_FIQ) {
			int oldFIQBank = oldBank == BANK_FIQ;
			int newFIQBank = newBank == BANK_FIQ;
			cpu->bankedRegisters[oldFIQBank][2] = cpu->gprs[8];
			cpu->bankedRegisters[oldFIQBank][3] = cpu->gprs[9];
			cpu->bankedRegisters[oldFIQBank][4] = cpu->gprs[10];
			cpu->bankedRegisters[oldFIQBank][5] = cpu->gprs[11];
			cpu->bankedRegisters[oldFIQBank][6] = cpu->gprs[12];
			cpu->gprs[8] = cpu->bankedRegisters[newFIQBank][2];
			cpu->gprs[9] = cpu->bankedRegisters[newFIQBank][3];
			cpu->gprs[10] = cpu->bankedRegisters[newFIQBank][4];
			cpu->gprs[11] = cpu->bankedRegisters[newFIQBank][5];
			cpu->gprs[12] = cpu->bankedRegisters[newFIQBank][6];
		}
		cpu->bankedRegisters[oldBank][0] = cpu->gprs[ARM_SP];
		cpu->bankedRegisters[oldBank][1] = cpu->gprs[ARM_LR];
		cpu->gprs[ARM_SP] = cpu->bankedRegisters[newBank][0];
		cpu->gprs[ARM_LR] = cpu->bankedRegisters[newBank][1];

		cpu->bankedSPSRs[oldBank] = cpu->spsr.packed;
		cpu->spsr.packed = cpu->bankedSPSRs[newBank];

	}
	cpu->privilegeMode = mode;
}

static inline enum RegisterBank _ARMSelectBank(enum PrivilegeMode mode) {
	switch (mode) {
		case MODE_USER:
		case MODE_SYSTEM:
			// No banked registers
			return BANK_NONE;
		case MODE_FIQ:
			return BANK_FIQ;
		case MODE_IRQ:
			return BANK_IRQ;
		case MODE_SUPERVISOR:
			return BANK_SUPERVISOR;
		case MODE_ABORT:
			return BANK_ABORT;
		case MODE_UNDEFINED:
			return BANK_UNDEFINED;
		default:
			// This should be unreached
			return BANK_NONE;
	}
}

void ARMInit(struct ARMCore* cpu) {
	cpu->memory = 0;
	cpu->board = 0;
}

void ARMAssociateMemory(struct ARMCore* cpu, struct ARMMemory* memory) {
	cpu->memory = memory;
}

void ARMAssociateBoard(struct ARMCore* cpu, struct ARMBoard* board) {
	cpu->board = board;
	board->cpu = cpu;
}

void ARMReset(struct ARMCore* cpu) {
	int i;
	for (i = 0; i < 16; ++i) {
		cpu->gprs[i] = 0;
	}
	for (i = 0; i < 6; ++i) {
		cpu->bankedRegisters[i][0] = 0;
		cpu->bankedRegisters[i][1] = 0;
		cpu->bankedRegisters[i][2] = 0;
		cpu->bankedRegisters[i][3] = 0;
		cpu->bankedRegisters[i][4] = 0;
		cpu->bankedRegisters[i][5] = 0;
		cpu->bankedRegisters[i][6] = 0;
		cpu->bankedSPSRs[i] = 0;
	}

	cpu->privilegeMode = MODE_SYSTEM;
	cpu->cpsr.packed = MODE_SYSTEM;
	cpu->spsr.packed = 0;

	cpu->shifterOperand = 0;
	cpu->shifterCarryOut = 0;

	cpu->executionMode = MODE_THUMB;
	_ARMSetMode(cpu, MODE_ARM);

	cpu->currentPC = 0;
	int currentCycles = 0;
	ARM_WRITE_PC;

	cpu->cycles = 0;
	cpu->nextEvent = 0;

	cpu->board->reset(cpu->board);
}

void ARMRaiseIRQ(struct ARMCore* cpu) {
	if (cpu->cpsr.i) {
		return;
	}
	union PSR cpsr = cpu->cpsr;
	int instructionWidth;
	if (cpu->executionMode == MODE_THUMB) {
		instructionWidth = WORD_SIZE_THUMB;
	} else {
		instructionWidth = WORD_SIZE_ARM;
	}
	ARMSetPrivilegeMode(cpu, MODE_IRQ);
	cpu->cpsr.priv = MODE_IRQ;
	cpu->gprs[ARM_LR] = cpu->gprs[ARM_PC] - instructionWidth + WORD_SIZE_ARM;
	cpu->gprs[ARM_PC] = BASE_IRQ + WORD_SIZE_ARM;
	cpu->memory->setActiveRegion(cpu->memory, cpu->gprs[ARM_PC]);
	_ARMSetMode(cpu, MODE_ARM);
	cpu->spsr = cpsr;
	cpu->cpsr.i = 1;
}

void ARMRaiseSWI(struct ARMCore* cpu) {
	union PSR cpsr = cpu->cpsr;
	int instructionWidth;
	if (cpu->executionMode == MODE_THUMB) {
		instructionWidth = WORD_SIZE_THUMB;
	} else {
		instructionWidth = WORD_SIZE_ARM;
	}
	ARMSetPrivilegeMode(cpu, MODE_SUPERVISOR);
	cpu->cpsr.priv = MODE_SUPERVISOR;
	cpu->gprs[ARM_LR] = cpu->gprs[ARM_PC] - instructionWidth;
	cpu->gprs[ARM_PC] = BASE_SWI + WORD_SIZE_ARM;
	cpu->memory->setActiveRegion(cpu->memory, cpu->gprs[ARM_PC]);
	_ARMSetMode(cpu, MODE_ARM);
	cpu->spsr = cpsr;
	cpu->cpsr.i = 1;
}

static inline ARMInstruction _ARMLoadInstructionARM(struct ARMMemory* memory, uint32_t address, uint32_t* opcodeOut) {
	uint32_t opcode;
	LOAD_32(opcode, address & memory->activeMask, memory->activeRegion);
	*opcodeOut = opcode;
	return _armTable[((opcode >> 16) & 0xFF0) | ((opcode >> 4) & 0x00F)];
}

static inline void ARMStep(struct ARMCore* cpu) {
	uint32_t opcode;
	cpu->currentPC = cpu->gprs[ARM_PC] - WORD_SIZE_ARM;
	ARMInstruction instruction = _ARMLoadInstructionARM(cpu->memory, cpu->currentPC, &opcode);
	cpu->gprs[ARM_PC] += WORD_SIZE_ARM;

	int condition = opcode >> 28;
	if (condition == 0xE) {
		instruction(cpu, opcode);
		return;
	} else {
		switch (condition) {
		case 0x0:
			if (!ARM_COND_EQ) {
				cpu->cycles += ARM_PREFETCH_CYCLES;
				return;
			}
			break;
		case 0x1:
			if (!ARM_COND_NE) {
				cpu->cycles += ARM_PREFETCH_CYCLES;
				return;
			}
			break;
		case 0x2:
			if (!ARM_COND_CS) {
				cpu->cycles += ARM_PREFETCH_CYCLES;
				return;
			}
			break;
		case 0x3:
			if (!ARM_COND_CC) {
				cpu->cycles += ARM_PREFETCH_CYCLES;
				return;
			}
			break;
		case 0x4:
			if (!ARM_COND_MI) {
				cpu->cycles += ARM_PREFETCH_CYCLES;
				return;
			}
			break;
		case 0x5:
			if (!ARM_COND_PL) {
				cpu->cycles += ARM_PREFETCH_CYCLES;
				return;
			}
			break;
		case 0x6:
			if (!ARM_COND_VS) {
				cpu->cycles += ARM_PREFETCH_CYCLES;
				return;
			}
			break;
		case 0x7:
			if (!ARM_COND_VC) {
				cpu->cycles += ARM_PREFETCH_CYCLES;
				return;
			}
			break;
		case 0x8:
			if (!ARM_COND_HI) {
				cpu->cycles += ARM_PREFETCH_CYCLES;
				return;
			}
			break;
		case 0x9:
			if (!ARM_COND_LS) {
				cpu->cycles += ARM_PREFETCH_CYCLES;
				return;
			}
			break;
		case 0xA:
			if (!ARM_COND_GE) {
				cpu->cycles += ARM_PREFETCH_CYCLES;
				return;
			}
			break;
		case 0xB:
			if (!ARM_COND_LT) {
				cpu->cycles += ARM_PREFETCH_CYCLES;
				return;
			}
			break;
		case 0xC:
			if (!ARM_COND_GT) {
				cpu->cycles += ARM_PREFETCH_CYCLES;
				return;
			}
			break;
		case 0xD:
			if (!ARM_COND_LE) {
				cpu->cycles += ARM_PREFETCH_CYCLES;
				return;
			}
			break;
		default:
			break;
		}
	}
	instruction(cpu, opcode);
}

static inline void ThumbStep(struct ARMCore* cpu) {
	cpu->currentPC = cpu->gprs[ARM_PC] - WORD_SIZE_THUMB;
	cpu->gprs[ARM_PC] += WORD_SIZE_THUMB;
	uint16_t opcode;
	LOAD_16(opcode, cpu->currentPC & cpu->memory->activeMask, cpu->memory->activeRegion);
	ThumbInstruction instruction = _thumbTable[opcode >> 6];
	instruction(cpu, opcode);
}

void ARMRun(struct ARMCore* cpu) {
	if (cpu->executionMode == MODE_THUMB) {
		ThumbStep(cpu);
	} else {
		ARMStep(cpu);
	}
	if (cpu->cycles >= cpu->nextEvent) {
		cpu->board->processEvents(cpu->board);
	}
}
