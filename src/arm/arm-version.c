/* Copyright (c) 2013-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef VERSION
#error VERSION is undeclared
#else
// The C preprocessor is kinda bad at when it expands variables, so the indirection is needed
#define CONCAT(X, Y, Z) X ## Y ## Z
#define PREFIX_ARM(B, C) CONCAT(ARMv, B, C)
#define PREFIX_THUMB(B, C) CONCAT(Thumbv, B, C)
#define ARM(SUFFIX) PREFIX_ARM(VERSION, SUFFIX)
#define Thumb(SUFFIX) PREFIX_THUMB(VERSION, SUFFIX)
#endif

static inline void ARM(Step)(struct ARMCore* cpu) {
	uint32_t opcode = cpu->prefetch[0];
	cpu->prefetch[0] = cpu->prefetch[1];
	cpu->gprs[ARM_PC] += WORD_SIZE_ARM;
	LOAD_32(cpu->prefetch[1], cpu->gprs[ARM_PC] & cpu->memory.activeMask, cpu->memory.activeRegion);

	ARMInstruction instruction;
	unsigned condition = opcode >> 28;
	if (condition != 0xE) {
		bool conditionMet = false;
		switch (condition) {
		case 0x0:
			conditionMet = ARM_COND_EQ;
			break;
		case 0x1:
			conditionMet = ARM_COND_NE;
			break;
		case 0x2:
			conditionMet = ARM_COND_CS;
			break;
		case 0x3:
			conditionMet = ARM_COND_CC;
			break;
		case 0x4:
			conditionMet = ARM_COND_MI;
			break;
		case 0x5:
			conditionMet = ARM_COND_PL;
			break;
		case 0x6:
			conditionMet = ARM_COND_VS;
			break;
		case 0x7:
			conditionMet = ARM_COND_VC;
			break;
		case 0x8:
			conditionMet = ARM_COND_HI;
			break;
		case 0x9:
			conditionMet = ARM_COND_LS;
			break;
		case 0xA:
			conditionMet = ARM_COND_GE;
			break;
		case 0xB:
			conditionMet = ARM_COND_LT;
			break;
		case 0xC:
			conditionMet = ARM_COND_GT;
			break;
		case 0xD:
			conditionMet = ARM_COND_LE;
			break;
		default:
#if VERSION > 4
			instruction = ARM(FInstructionTable)[((opcode >> 16) & 0xFF0) | ((opcode >> 4) & 0x00F)];
			instruction(cpu, opcode);
#endif
			return;
		}
		if (!conditionMet) {
			cpu->cycles += ARM_PREFETCH_CYCLES;
			return;
		}
	}
	instruction = ARM(InstructionTable)[((opcode >> 16) & 0xFF0) | ((opcode >> 4) & 0x00F)];
	instruction(cpu, opcode);
}

static inline void Thumb(Step)(struct ARMCore* cpu) {
	uint32_t opcode = cpu->prefetch[0];
	cpu->prefetch[0] = cpu->prefetch[1];
	cpu->gprs[ARM_PC] += WORD_SIZE_THUMB;
	LOAD_16(cpu->prefetch[1], cpu->gprs[ARM_PC] & cpu->memory.activeMask, cpu->memory.activeRegion);
	ThumbInstruction instruction = Thumb(InstructionTable)[opcode >> 6];
	instruction(cpu, opcode);
}

void ARM(Run)(struct ARMCore* cpu) {
	while (cpu->cycles >= cpu->nextEvent) {
		cpu->irqh.processEvents(cpu);
	}
	if (cpu->executionMode == MODE_THUMB) {
		Thumb(Step)(cpu);
	} else {
		ARM(Step)(cpu);
	}
}

void ARM(RunLoop)(struct ARMCore* cpu) {
	if (cpu->executionMode == MODE_THUMB) {
		while (cpu->cycles < cpu->nextEvent) {
			Thumb(Step)(cpu);
		}
	} else {
		while (cpu->cycles < cpu->nextEvent) {
			ARM(Step)(cpu);
		}
	}
	cpu->irqh.processEvents(cpu);
}

#undef ARM
#undef Thumb
#undef PREFIX_ARM
#undef PREFIX_THUMB
#undef CONCAT
#undef VERSION