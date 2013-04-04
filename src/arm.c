#include "arm.h"

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
}

void ARMCycle(struct ARMCore* cpu) {
	// TODO
}