#ifndef ARM_DECODER_INLINES_H
#define ARM_DECODER_INLINES_H

#include "decoder.h"

#include "arm.h"

#include <stdio.h>
#include <string.h>

#define ADVANCE(AMOUNT) \
	if (AMOUNT > blen) { \
		buffer[blen - 1] = '\0'; \
		return total; \
	} \
	total += AMOUNT; \
	buffer += AMOUNT; \
	blen -= AMOUNT;

static int _decodeRegister(int reg, char* buffer, int blen);
static int _decodeRegisterList(int list, char* buffer, int blen);
static int _decodePCRelative(uint32_t address, uint32_t pc, char* buffer, int blen);
static int _decodeMemory(struct ARMMemoryAccess memory, int pc, char* buffer, int blen);

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
		strncpy(buffer, "sp", blen);
		return 2;
	case ARM_LR:
		strncpy(buffer, "lr", blen);
		return 2;
	case ARM_PC:
		strncpy(buffer, "pc", blen);
		return 2;
	default:
		return snprintf(buffer, blen, "r%i", reg);
	}
}

static int _decodeRegisterList(int list, char* buffer, int blen) {
	if (blen <= 0) {
		return 0;
	}
	int total = 0;
	strncpy(buffer, "{", blen);
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
					strncpy(buffer, "-", blen);
					ADVANCE(1);
				}
				written = _decodeRegister(end, buffer, blen);
				ADVANCE(written);
				strncpy(buffer, ",", blen);
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
			strncpy(buffer, "-", blen);
			ADVANCE(1);
		}
		written = _decodeRegister(end, buffer, blen);
		ADVANCE(written);
	}
	strncpy(buffer, "}", blen);
	ADVANCE(1);
	return total;
}

static int _decodePCRelative(uint32_t address, uint32_t pc, char* buffer, int blen) {
	return snprintf(buffer, blen, "$%08X", address + pc);
}

static int _decodeMemory(struct ARMMemoryAccess memory, int pc, char* buffer, int blen) {
	if (blen <= 0) {
		return 0;
	}
	int total = 0;
	strncpy(buffer, "[", blen);
	ADVANCE(1);
	int written;
	if (memory.format & ARM_MEMORY_REGISTER_BASE) {
		if (memory.baseReg == ARM_PC && memory.format & ARM_MEMORY_IMMEDIATE_OFFSET) {
			written = _decodePCRelative(memory.offset.immediate, pc, buffer, blen);
			ADVANCE(written);
		} else {
			written = _decodeRegister(memory.baseReg, buffer, blen);
			ADVANCE(written);
			if (memory.format & (ARM_MEMORY_REGISTER_OFFSET | ARM_MEMORY_IMMEDIATE_OFFSET) && !(memory.format & ARM_MEMORY_POST_INCREMENT)) {
				strncpy(buffer, ", ", blen);
				ADVANCE(2);
			}
		}
	}
	if (memory.format & ARM_MEMORY_POST_INCREMENT) {
		strncpy(buffer, "], ", blen);
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
			strncpy(buffer, "-", blen);
			ADVANCE(1);
		}
		written = _decodeRegister(memory.offset.reg, buffer, blen);
		ADVANCE(written);
	}
	// TODO: shifted registers

	if (!(memory.format & ARM_MEMORY_POST_INCREMENT)) {
		strncpy(buffer, "]", blen);
		ADVANCE(1);
	}
	if (memory.format & ARM_MEMORY_PRE_INCREMENT) {
		strncpy(buffer, "!", blen);
		ADVANCE(1);
	}
	return total;
}

#endif
