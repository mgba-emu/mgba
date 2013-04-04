#include "arm.h"

static void _ARMSetMode(struct ARMCore*, enum ExecutionMode);
static ARMInstruction _ARMLoadInstructionARM(struct ARMMemory*, uint32_t address);
static ARMInstruction _ARMLoadInstructionThumb(struct ARMMemory*, uint32_t address);

static void _ARMSetMode(struct ARMCore* cpu, enum ExecutionMode executionMode) {
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

static ARMInstruction _ARMLoadInstructionARM(struct ARMMemory* memory, uint32_t address) {
	int32_t opcode = memory->load32(memory, address);
	return 0;
}

static ARMInstruction _ARMLoadInstructionThumb(struct ARMMemory* memory, uint32_t address) {
	uint16_t opcode = memory->loadU16(memory, address);
	return 0;
}

void ARMInit(struct ARMCore* cpu) {
	int i;
	for (i = 0; i < 16; ++i) {
		cpu->gprs[i] = 0;
	}

	cpu->cpsr.packed = 0;
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

void ARMCycle(struct ARMCore* cpu) {
	// TODO
	ARMInstruction instruction = cpu->loadInstruction(cpu->memory, cpu->gprs[ARM_PC] - cpu->instructionWidth);
}