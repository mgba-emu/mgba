#include "gba-bios.h"

#include "gba.h"
#include "gba-memory.h"

static void _CpuSet(struct GBA* gba) {
	uint32_t source = gba->cpu.gprs[0];
	uint32_t dest = gba->cpu.gprs[1];
	uint32_t mode = gba->cpu.gprs[2];
	int count = mode & 0x000FFFFF;
	int fill = mode & 0x01000000;
	int wordsize = (mode & 0x04000000) ? 4 : 2;
	int i;
	if (fill) {
		if (wordsize == 4) {
			source &= 0xFFFFFFFC;
			dest &= 0xFFFFFFFC;
			int32_t word = GBALoad32(&gba->memory.d, source);
			for (i = 0; i < count; ++i) {
				GBAStore32(&gba->memory.d, dest + (i << 2), word);
			}
		} else {
			source &= 0xFFFFFFFE;
			dest &= 0xFFFFFFFE;
			uint16_t word = GBALoad16(&gba->memory.d, source);
			for (i = 0; i < count; ++i) {
				GBAStore16(&gba->memory.d, dest + (i << 1), word);
			}
		}
	} else {
		if (wordsize == 4) {
			source &= 0xFFFFFFFC;
			dest &= 0xFFFFFFFC;
			for (i = 0; i < count; ++i) {
				int32_t word = GBALoad32(&gba->memory.d, source + (i << 2));
				GBAStore32(&gba->memory.d, dest + (i << 2), word);
			}
		} else {
			source &= 0xFFFFFFFE;
			dest &= 0xFFFFFFFE;
			for (i = 0; i < count; ++i) {
				uint16_t word = GBALoad16(&gba->memory.d, source + (i << 1));
				GBAStore16(&gba->memory.d, dest + (i << 1), word);
			}
		}
	}
}

void GBASwi16(struct ARMBoard* board, int immediate) {
	struct GBA* gba = ((struct GBABoard*) board)->p;
	switch (immediate) {
	case 0xB:
		_CpuSet(gba);
		break;
	default:
		GBALog(GBA_LOG_STUB, "Stub software interrupt: %02x", immediate);
	}
}

void GBASwi32(struct ARMBoard* board, int immediate) {
	GBASwi32(board, immediate >> 8);
}
