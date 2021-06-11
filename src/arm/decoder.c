/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/arm/decoder.h>

#include <mgba/internal/arm/decoder-inlines.h>
#include <mgba-util/string.h>

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
		strlcpy(buffer, "sp", blen);
		return 2;
	case ARM_LR:
		strlcpy(buffer, "lr", blen);
		return 2;
	case ARM_PC:
		strlcpy(buffer, "pc", blen);
		return 2;
	case ARM_CPSR:
		strlcpy(buffer, "cpsr", blen);
		return 4;
	case ARM_SPSR:
		strlcpy(buffer, "spsr", blen);
		return 4;
	default:
		return snprintf(buffer, blen, "r%i", reg);
	}
}

static int _decodeRegisterList(int list, char* buffer, int blen) {
	if (blen <= 0) {
		return 0;
	}
	int total = 0;
	strlcpy(buffer, "{", blen);
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
					strlcpy(buffer, "-", blen);
					ADVANCE(1);
				}
				written = _decodeRegister(end, buffer, blen);
				ADVANCE(written);
				strlcpy(buffer, ",", blen);
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
			strlcpy(buffer, "-", blen);
			ADVANCE(1);
		}
		written = _decodeRegister(end, buffer, blen);
		ADVANCE(written);
	}
	strlcpy(buffer, "}", blen);
	ADVANCE(1);
	return total;
}

static int _decodePSR(int psrBits, char* buffer, int blen) {
	if (!psrBits) {
		return 0;
	}
	int total = 0;
	strlcpy(buffer, "_", blen);
	ADVANCE(1);
	if (psrBits & ARM_PSR_C) {
		strlcpy(buffer, "c", blen);
		ADVANCE(1);
	}
	if (psrBits & ARM_PSR_X) {
		strlcpy(buffer, "x", blen);
		ADVANCE(1);
	}
	if (psrBits & ARM_PSR_S) {
		strlcpy(buffer, "s", blen);
		ADVANCE(1);
	}
	if (psrBits & ARM_PSR_F) {
		strlcpy(buffer, "f", blen);
		ADVANCE(1);
	}
	return total;
}

static int _decodePCRelative(uint32_t address, uint32_t pc, char* buffer, int blen) {
	return snprintf(buffer, blen, "$%08X", address + pc);
}

static int _decodeMemory(struct ARMMemoryAccess memory, int pc, char* buffer, int blen) {
	if (blen <= 1) {
		return 0;
	}
	int total = 0;
	strlcpy(buffer, "[", blen);
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
				strlcpy(buffer, ", ", blen);
				ADVANCE(2);
			}
		}
	}
	if (memory.format & ARM_MEMORY_POST_INCREMENT) {
		strlcpy(buffer, "], ", blen);
		ADVANCE(3);
	}
	if (memory.format & ARM_MEMORY_IMMEDIATE_OFFSET && memory.baseReg != ARM_PC) {
		if (memory.format & ARM_MEMORY_OFFSET_SUBTRACT) {
			written = snprintf(buffer, blen, "#-%i", memory.offset.immediate);
			ADVANCE(written);
		} else {
			written = snprintf(buffer, blen, "#%i", memory.offset.immediate);
			ADVANCE(written);
		}
	} else if (memory.format & ARM_MEMORY_REGISTER_OFFSET) {
		if (memory.format & ARM_MEMORY_OFFSET_SUBTRACT) {
			strlcpy(buffer, "-", blen);
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
		strlcpy(buffer, "]", blen);
		ADVANCE(1);
	}
	if ((memory.format & (ARM_MEMORY_PRE_INCREMENT | ARM_MEMORY_WRITEBACK)) == (ARM_MEMORY_PRE_INCREMENT | ARM_MEMORY_WRITEBACK)) {
		strlcpy(buffer, "!", blen);
		ADVANCE(1);
	}
	return total;
}

static int _decodeShift(union ARMOperand op, bool reg, char* buffer, int blen) {
	if (blen <= 1) {
		return 0;
	}
	int total = 0;
	strlcpy(buffer, ", ", blen);
	ADVANCE(2);
	int written;
	switch (op.shifterOp) {
	case ARM_SHIFT_LSL:
		strlcpy(buffer, "lsl ", blen);
		ADVANCE(4);
		break;
	case ARM_SHIFT_LSR:
		strlcpy(buffer, "lsr ", blen);
		ADVANCE(4);
		break;
	case ARM_SHIFT_ASR:
		strlcpy(buffer, "asr ", blen);
		ADVANCE(4);
		break;
	case ARM_SHIFT_ROR:
		strlcpy(buffer, "ror ", blen);
		ADVANCE(4);
		break;
	case ARM_SHIFT_RRX:
		strlcpy(buffer, "rrx", blen);
		ADVANCE(3);
		return total;
	}
	if (!reg) {
		written = snprintf(buffer, blen, "#%i", op.shifterImm);
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
	"blx",
	"bx",
	"cdp",
	"clz",
	"cmn",
	"cmp",
	"eor",
	"ldc",
	"ldm",
	"ldr",
	"lsl",
	"lsr",
	"mcr",
	"mla",
	"mov",
	"mrc",
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
	"smlabb",
	"smlabt",
	"smlal",
	"smlatb",
	"smlatt",
	"smlawb",
	"smlawt",
	"smulbb",
	"smulbt",
	"smull",
	"smultb",
	"smultt",
	"smulwb",
	"smulwt",
	"stc",
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
	case ARM_MN_ASR:
	case ARM_MN_BIC:
	case ARM_MN_EOR:
	case ARM_MN_LSL:
	case ARM_MN_LSR:
	case ARM_MN_MLA:
	case ARM_MN_MOV:
	case ARM_MN_MUL:
	case ARM_MN_MVN:
	case ARM_MN_ORR:
	case ARM_MN_ROR:
	case ARM_MN_RSB:
	case ARM_MN_RSC:
	case ARM_MN_SBC:
	case ARM_MN_SMLAL:
	case ARM_MN_SMULL:
	case ARM_MN_SUB:
	case ARM_MN_UMLAL:
	case ARM_MN_UMULL:
		if (info->affectsCPSR && info->execMode == MODE_ARM) {
			flags = "s";
		}
		break;
	default:
		break;
	}
	written = snprintf(buffer, blen, "%s%s%s ", mnemonic, cond, flags);
	ADVANCE(written);

	switch (info->mnemonic) {
	case ARM_MN_LDM:
	case ARM_MN_STM:
		written = _decodeRegister(info->memory.baseReg, buffer, blen);
		ADVANCE(written);
		if (info->memory.format & ARM_MEMORY_WRITEBACK) {
			strlcpy(buffer, "!", blen);
			ADVANCE(1);
		}
		strlcpy(buffer, ", ", blen);
		ADVANCE(2);
		written = _decodeRegisterList(info->op1.immediate, buffer, blen);
		ADVANCE(written);
		if (info->memory.format & ARM_MEMORY_SPSR_SWAP) {
			strlcpy(buffer, "^", blen);
			ADVANCE(1);
		}
		break;
	case ARM_MN_B:
	case ARM_MN_BL:
	case ARM_MN_BLX:
		if (info->operandFormat & ARM_OPERAND_IMMEDIATE_1) {
			written = _decodePCRelative(info->op1.immediate, pc, buffer, blen);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_REGISTER_1) {
			written = _decodeRegister(info->op1.reg, buffer, blen);
			ADVANCE(written);
			if (info->op1.reg > ARM_PC) {
				written = _decodePSR(info->op1.psrBits, buffer, blen);
				ADVANCE(written);
			}
		}
		break;
	default:
		if (info->operandFormat & ARM_OPERAND_COPROCESSOR) {
			written = snprintf(buffer, blen - 1, "p%i, %i, ", info->cp.cp, info->cp.op1);
			ADVANCE(written);
		}
		if (info->operandFormat & ARM_OPERAND_IMMEDIATE_1) {
			written = snprintf(buffer, blen, "#%i", info->op1.immediate);
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
		} else if (info->operandFormat & ARM_OPERAND_COPROCESSOR_REG_1) {
			written = snprintf(buffer, blen - 1, "c%i", info->op1.reg);
		}
		if (info->operandFormat & ARM_OPERAND_SHIFT_REGISTER_1) {
			written = _decodeShift(info->op1, true, buffer, blen);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_SHIFT_IMMEDIATE_1) {
			written = _decodeShift(info->op1, false, buffer, blen);
			ADVANCE(written);
		}
		if (info->operandFormat & ARM_OPERAND_2) {
			strlcpy(buffer, ", ", blen);
			ADVANCE(2);
		}
		if (info->operandFormat & ARM_OPERAND_IMMEDIATE_2) {
			written = snprintf(buffer, blen, "#%i", info->op2.immediate);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_MEMORY_2) {
			written = _decodeMemory(info->memory, pc, buffer, blen);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_REGISTER_2) {
			written = _decodeRegister(info->op2.reg, buffer, blen);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_COPROCESSOR_REG_2) {
			written = snprintf(buffer, blen - 1, "c%i", info->op2.reg);
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
			strlcpy(buffer, ", ", blen);
			ADVANCE(2);
		}
		if (info->operandFormat & ARM_OPERAND_IMMEDIATE_3) {
			written = snprintf(buffer, blen, "#%i", info->op3.immediate);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_MEMORY_3) {
			written = _decodeMemory(info->memory, pc, buffer, blen);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_REGISTER_3) {
			written = _decodeRegister(info->op3.reg, buffer, blen);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_COPROCESSOR_REG_3) {
			written = snprintf(buffer, blen - 1, "c%i", info->op3.reg);
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
			strlcpy(buffer, ", ", blen);
			ADVANCE(2);
		}
		if (info->operandFormat & ARM_OPERAND_IMMEDIATE_4) {
			written = snprintf(buffer, blen, "#%i", info->op4.immediate);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_MEMORY_4) {
			written = _decodeMemory(info->memory, pc, buffer, blen);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_REGISTER_4) {
			written = _decodeRegister(info->op4.reg, buffer, blen);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_COPROCESSOR_REG_4) {
			written = snprintf(buffer, blen - 1, "c%i", info->op4.reg);
			ADVANCE(written);
		}
		if (info->operandFormat & ARM_OPERAND_SHIFT_REGISTER_4) {
			written = _decodeShift(info->op4, true, buffer, blen);
			ADVANCE(written);
		} else if (info->operandFormat & ARM_OPERAND_SHIFT_IMMEDIATE_4) {
			written = _decodeShift(info->op4, false, buffer, blen);
			ADVANCE(written);
		}
		if (info->cp.op2) {
			written = snprintf(buffer, blen - 1, ", %i", info->cp.op2);
			ADVANCE(written);
		}
		break;
	}
	buffer[blen - 1] = '\0';
	return total;
}

uint32_t ARMResolveMemoryAccess(struct ARMInstructionInfo* info, struct ARMRegisterFile* regs, uint32_t pc) {
	uint32_t address = 0;
	int32_t offset = 0;
	if (info->memory.format & ARM_MEMORY_REGISTER_BASE) {
		if (info->memory.baseReg == ARM_PC && info->memory.format & ARM_MEMORY_IMMEDIATE_OFFSET) {
			address = pc;
		} else {
			address = regs->gprs[info->memory.baseReg];
		}
	}
	if (info->memory.format & ARM_MEMORY_POST_INCREMENT) {
		return address;
	}
	if (info->memory.format & ARM_MEMORY_IMMEDIATE_OFFSET) {
		offset = info->memory.offset.immediate;
	} else if (info->memory.format & ARM_MEMORY_REGISTER_OFFSET) {
		offset = info->memory.offset.reg == ARM_PC ? pc : regs->gprs[info->memory.offset.reg];
	}
	if (info->memory.format & ARM_MEMORY_SHIFTED_OFFSET) {
		uint8_t shiftSize = info->memory.offset.shifterImm;
		switch (info->memory.offset.shifterOp) {
			case ARM_SHIFT_LSL:
				offset <<= shiftSize;
				break;
			case ARM_SHIFT_LSR:
				offset = ((uint32_t) offset) >> shiftSize;
				break;
			case ARM_SHIFT_ASR:
				offset >>= shiftSize;
				break;
			case ARM_SHIFT_ROR:
				offset = ROR(offset, shiftSize);
				break;
			case ARM_SHIFT_RRX:
				offset = (regs->cpsr.c << 31) | ((uint32_t) offset >> 1);
				break;
			default:
				break;
		};
	}
	return address + (info->memory.format & ARM_MEMORY_OFFSET_SUBTRACT ? -offset : offset);
}
