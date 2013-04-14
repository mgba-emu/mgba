#include "gba-io.h"

void GBAIOWrite(struct GBA* gba, uint32_t address, uint16_t value) {
	switch (address) {
	default:
		GBALog(GBA_LOG_STUB, "Stub I/O register: %03x", address);
		break;
	}
}
