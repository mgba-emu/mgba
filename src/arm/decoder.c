/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/arm/decoder.h>

#include <mgba/internal/arm/decoder-inlines.h>

#define ADVANCE(AMOUNT) \
	if (AMOUNT >= blen) { \
		buffer[blen - 1] = '\0'; \
		return total; \
	} \
	total += AMOUNT; \
	buffer += AMOUNT; \
	blen -= AMOUNT;

static int _decodeRegister(int reg, char* buffer, int blen);
static int _decodeRegisterList(int list, char* buffer, int blen);
static int _decodePSR(int bits, char* buffer, int blen);
static int _decodePCRelative(uint32_t address, uint32_t pc, char* buffer, int blen);
static int _decodeMemory(struct ARMMemoryAccess memory, int pc, char* buffer, int blen);
static int _decodeShift(union ARMOperand operand, bool reg, char* buffer, int blen);

static const char* _armConditions[] = {
	"eq",
	"ne",
	"cs",
	"cc",
	"mi",
	"pl",
	"vs",
	"vc",
	"hi",
	"ls",
	"ge",
	"lt",
	"gt",
	"le",
	"al",
	"nv"
};

static int _decodeRegister(int reg, char* buffer, int blen) {
	switch (reg) {
	case ARM_SP:
		strncpy(buffer, "sp", blen - 1);
		return 2;
	case ARM_LR:
		strncpy(buffer, "lr", blen - 1);
		return 2;
	case ARM_PC:
		strncpy(buffer, "pc", blen - 1);
		return 2;
	case ARM_CPSR:
		strncpy(buffer, "cpsr", blen - 1);
		return 4;
	case ARM_SPSR:
		strncpy(buffer, "spsr", blen - 1);
		return 4;
	default:
		return snprintf(buffer, blen - 1, "r%i", reg);
	}
}

static int _decodeRegisterList(int list, char* buffer, int blen) {
	if (blen <= 0) {
		return 0;
	}
	int total = 0;
	strncpy(buffer, "{", blen - 1);
	ADVANCE(1);
	int i;
	int start = -1;
	int end = -1;
	int written;
	for (i = 0; i <= ARM_PC; ++i) {
		if (list & 1) {
			if (start < 0) {
				start = i;
				end = i;
			} else if (end + 1 == i) {
				end = i;
			} else {
				if (end > start) {
					written = _decodeRegister(start, buffer, blen);
					ADVANCE(written);
					strncpy(buffer, "-", blen - 1);
					ADVANCE(1);
				}
				written = _decodeRegister(end, buffer, blen);
				ADVANCE(written);
				strncpy(buffer, ",", blen - 1);
				ADVANCE(1);
				start = i;
				end = i;
			}
		}
		list >>= 1;
	}
	if (start >= 0) {
		if (end > start) {
			written = _decodeRegister(start, buffer, blen);
			ADVANCE(written);
			strncpy(buffer, "-", blen - 1);
			ADVANCE(1);
		}
		written = _decodeRegister(end, buffer, blen);
		ADVANCE(written);
	}
	strncpy(buffer, "}", blen - 1);
	ADVANCE(1);
	return total;
}

static int _decodePSR(int psrBits, char* buffer, int blen) {
	if (!psrBits) {
		return 0;
	}
	int total = 0;
	strncpy(buffer, "_", blen - 1);
	ADVANCE(1);
	if (psrBits & ARM_PSR_C) {
		strncpy(buffer, "c", blen - 1);
		ADVANCE(1);
	}
	if (psrBits & ARM_PSR_X) {
		strncpy(buffer, "x", blen - 1);
		ADVANCE(1);
	}
	if (psrBits & ARM_PSR_S) {
		strncpy(buffer, "s", blen - 1);
		ADVANCE(1);
	}
	if (psrBits & ARM_PSR_F) {
		strncpy(buffer, "f", blen - 1);
		ADVANCE(1);
	}
	return total;
}

static int _decodePCRelative(uint32_t address, uint32_t pc, char* buffer, int blen) {
	return snprintf(buffer, blen - 1, "$%08X", address + pc);
}

static int _decodeMemory(struct ARMMemoryAccess memory, int pc, char* buffer, int blen) {
	if (blen <= 1) {
		return 0;
	}
	int total = 0;
	strncpy(buffer, "[", blen - 1);
	ADVANCE(1);
	int written;
	if (memory.format & ARM_MEMORY_REGISTER_BASE) {
		if (memory.baseReg == ARM_PC && memory.format & ARM_MEMORY_IMMEDIATE_OFFSET) {
			written = _decodePCRelative(memory.format & ARM_MEMORY_OFFSET_SUBTRACT ? -memory.offset.immediate : memory.offset.immediate, pc & 0xFFFFFFFC, buffer, blen);
			ADVANCE(written);
		} else {
			written = _decodeRegister(memory.baseReg, buffer, blen);
			ADVANCE(written);
			if (memory.format & (ARM_MEMORY_REGISTER_OFFSET | ARM_MEMORY_IMMEDIATE_OFFSET) && !(memory.format & ARM_MEMORY_POST_INCREMENT)) {
				strncpy(buffer, ", ", blen - 1);
				ADVANCE(2);
			}
		}
	}
	if (memory.format & ARM_MEMORY_POST_INCREMENT) {
		strncpy(buffer, "], ", blen - 1);
		ADVANCE(3);
	}
	if (memory.format & ARM_MEMORY_IMMEDIATE_OFFSET && memory.baseReg != ARM_PC) {
		if (memory.format & ARM_MEMORY_OFFSET_SUBTRACT) {
			written = snprintf(buffer, blen - 1, "#-%i", memory.offset.immediate);
			ADVANCE(written);
		} else {
			written = snprintf(buffer, blen - 1, "#%i", memory.offset.immediate);
			ADVANCE(written);
		}
	} else if (memory.format & ARM_MEMORY_REGISTER_OFFSET) {
		if (memory.format & ARM_MEMORY_OFFSET_SUBTRACT) {
			strncpy(buffer, "-", blen - 1);
			ADVANCE(1);
		}
		written = _decodeRegister(memory.offset.reg, buffer, blen);
		ADVANCE(written);
	}
	if (memory.format & ARM_MEMORY_SHIFTED_OFFSET) {
		written = _decodeShift(memory.offset, false, buffer, blen);
		ADVANCE(written);
	}

	if (!(memory.format & ARM_MEMORY_POST_INCREMENT)) {
		strncpy(buffer, "]", blen - 1);
		ADVANCE(1);
	}
	if ((memory.format & (ARM_MEMORY_PRE_INCREMENT | ARM_MEMORY_WRITEBACK)) == (ARM_MEMORY_PRE_INCREMENT | ARM_MEMORY_WRITEBACK)) {
		strncpy(buffer, "!", blen - 1);
		ADVANCE(1);
	}
	return total;
}

static int _decodeShift(union ARMOperand op, bool reg, char* buffer, int blen) {
	if (blen <= 1) {
		return 0;
	}
	int total = 0;
	strncpy(buffer, ", ", blen - 1);
	ADVANCE(2);
	int written;
	switch (op.shifterOp) {
	case ARM_SHIFT_LSL:
		strncpy(buffer, "lsl ", blen - 1);
		ADVANCE(4);
		break;
	case ARM_SHIFT_LSR:
		strncpy(buffer, "lsr ", blen - 1);
		ADVANCE(4);
		break;
	case ARM_SHIFT_ASR:
		strncpy(buffer, "asr ", blen - 1);
		ADVANCE(4);
		break;
	case ARM_SHIFT_ROR:
		strncpy(buffer, "ror ", blen - 1);
		ADVANCE(4);
		break;
	case ARM_SHIFT_RRX:
		strncpy(buffer, "rrx", blen - 1);
		ADVANCE(3);
		return total;
	}
	if (!reg) {
		written = snprintf(buffer, blen - 1, "#%i", op.shifterImm);
	} else {
		written = _decodeRegister(op.shifterReg, buffer, blen);
	}
	ADVANCE(written);
	return total;
}

static const char* _armMnemonicStrings[] = {
	"ill",
	"adc",
	"add",
	"and",
	"asr",
	"b",
	"bic",
	"bkpt",
	"bl",
	"bx",
	"cmn",
	"cmp",
	"eor",
	"ldm",
	"ldr",
	"lsl",
	"lsr",
	"mla",
	"mov",
	"mrs",
	"msr",
	"mul",
	"mvn",
	"neg",
	"orr",
	"ror",
	"rsb",
	"rsc",
	"sbc",
	"smlal",
	"smull",
	"stm",
	"str",
	"sub",
	"swi",
	"swp",
	"teq",
	"tst",
	"umlal",
	"umull",

	"ill"
};

static const char* _armDirectionStrings[] = {
	"da",
	"ia",
	"db",
	"ib"
};

static const char* _armAccessTypeStrings[] = {
	"",
	"b",
	"h",
	"",
	"",
	"",
	"",
	"",

	"",
	"sb",
	"sh",
	"",
	"",
	"",
	"",
	"",

	"",
	"bt",
	"",
	"",
	"t",
	"",
	"",
	""
};

int ARMDisassemble(struct ARMInstructionInfo* info, uint32_t pc, char* buffer, int blen) {
	const char* mnemonic = _armMnemonicStrings[info->mnemonic];
	int written;
	int total = 0;
	const char* cond = "";
	if (info->condition != ARM_CONDITION_AL && info->condition < ARM_CONDITION_NV) {
		cond = _armConditions[info->condition];
	}
	const char* flags = "";
	switch (info->mnemonic) {
	case ARM_MN_LDM:
	case ARM_MN_STM:
		flags = _armDirectionStrings[MEMORY_FORMAT_TO_DIRECTION(info->memory.format)];
		break;
	case ARM_MN_LDR:
	case ARM_MN_STR:
	case ARM_MN_SWP:
		flags = _armAccessTypeStrings[info->memory.width];
		break;
	case ARM_MN_ADD:
	case ARM_MN_ADC:
	case ARM_MN_AND:
	case ARM_MN_BIC:
	case ARM_MN_EOR:
	case ARM_MN_MOV:
	case ARM_MN_MVN:
	case ARM_MN_ORR:
	case ARM_MN_RSB:
	case ARM_MN_RSC:
	case ARM_MN_SBC:
	case ARM_MN_SUB:
		if (info->affectsCPSR && info->execMode == MODE_ARM) {
			flags = "s";
		}
		break;
	default:
		break;
	}
	written = snprintf(buffer, blen - 1, "%s%s%s ", mnemonic, cond, flags);
	ADVANCE(written);

	switch (info->mnemonic) {
	case ARM_MN_LDM:
	case ARM_MN_STM:
		written = _decodeRegister(info->memory.baseReg, buffer, blen);
		ADVANCE(written);
		if (info->memory.format & ARM_MEMORY_WRITEBACK) {
			strncpy(buffer, "!", blen - 1);
			ADVANCE(1);
		}
		strncpy(buffer, ", ", blen - 1);
		ADVANCE(2);
		written = _decodeRegisterList(info->op1.immediate, buffer, blen);
		ADVANCE(written);
		if (info->memory.format & ARM_MEMORY_SPSR_SWAP) {
			strncpy(buffer, "^", blen - 1);
			ADVANCE(1);
		}
		break;
	case ARM_MN_B:
	case ARM_MN_BL:
		if (info->operandFormat & ARM_OPERAND_IMMEDIATE_1) {
			written = _decodePCRelative(info->op1.immediate, pc, buffer, blen);
			ADVANCE(written);
		}
		break;
	default:
		if (info->operandFormat & ARM_OPERAND_IMMEDIATE_1) {
			written = snprintf(buffer, blen - 1, "#%i", info->op1.immediate);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_MEMORY_1) {
			written = _decodeMemory(info->memory, pc, buffer, blen);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_REGISTER_1) {
			written = _decodeRegister(info->op1.reg, buffer, blen);
			ADVANCE(written);
			if (info->op1.reg > ARM_PC) {
				written = _decodePSR(info->op1.psrBits, buffer, blen);
				ADVANCE(written);
			}
		}
		if (info->operandFormat & ARM_OPERAND_SHIFT_REGISTER_1) {
			written = _decodeShift(info->op1, true, buffer, blen);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_SHIFT_IMMEDIATE_1) {
			written = _decodeShift(info->op1, false, buffer, blen);
			ADVANCE(written);
		}
		if (info->operandFormat & ARM_OPERAND_2) {
			strncpy(buffer, ", ", blen);
			ADVANCE(2);
		}
		if (info->operandFormat & ARM_OPERAND_IMMEDIATE_2) {
			written = snprintf(buffer, blen - 1, "#%i", info->op2.immediate);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_MEMORY_2) {
			written = _decodeMemory(info->memory, pc, buffer, blen);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_REGISTER_2) {
			written = _decodeRegister(info->op2.reg, buffer, blen);
			ADVANCE(written);
		}
		if (info->operandFormat & ARM_OPERAND_SHIFT_REGISTER_2) {
			written = _decodeShift(info->op2, true, buffer, blen);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_SHIFT_IMMEDIATE_2) {
			written = _decodeShift(info->op2, false, buffer, blen);
			ADVANCE(written);
		}
		if (info->operandFormat & ARM_OPERAND_3) {
			strncpy(buffer, ", ", blen - 1);
			ADVANCE(2);
		}
		if (info->operandFormat & ARM_OPERAND_IMMEDIATE_3) {
			written = snprintf(buffer, blen - 1, "#%i", info->op3.immediate);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_MEMORY_3) {
			written = _decodeMemory(info->memory, pc, buffer, blen);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_REGISTER_3) {
			written = _decodeRegister(info->op3.reg, buffer, blen);
			ADVANCE(written);
		}
		if (info->operandFormat & ARM_OPERAND_SHIFT_REGISTER_3) {
			written = _decodeShift(info->op3, true, buffer, blen);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_SHIFT_IMMEDIATE_3) {
			written = _decodeShift(info->op3, false, buffer, blen);
			ADVANCE(written);
		}
		if (info->operandFormat & ARM_OPERAND_4) {
			strncpy(buffer, ", ", blen - 1);
			ADVANCE(2);
		}
		if (info->operandFormat & ARM_OPERAND_IMMEDIATE_4) {
			written = snprintf(buffer, blen - 1, "#%i", info->op4.immediate);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_MEMORY_4) {
			written = _decodeMemory(info->memory, pc, buffer, blen);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_REGISTER_4) {
			written = _decodeRegister(info->op4.reg, buffer, blen);
			ADVANCE(written);
		}
		if (info->operandFormat & ARM_OPERAND_SHIFT_REGISTER_4) {
			written = _decodeShift(info->op4, true, buffer, blen);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_SHIFT_IMMEDIATE_4) {
			written = _decodeShift(info->op4, false, buffer, blen);
			ADVANCE(written);
		}
		break;
	}
	buffer[blen - 1] = '\0';
	return total;
}
