#include "gba-bios.h"

#include "gba.h"

void GBASwi16(struct ARMBoard* board, int immediate) {
	switch (immediate) {
	default:
		GBALog(GBA_LOG_STUB, "Stub software interrupt: %02x", immediate);
	}
}

void GBASwi32(struct ARMBoard* board, int immediate) {
	GBASwi32(board, immediate >> 8);
}
