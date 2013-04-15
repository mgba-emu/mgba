#include "gba-io.h"

void GBAIOWrite(struct GBA* gba, uint32_t address, uint16_t value) {
	switch (address) {
	default:
		GBALog(GBA_LOG_STUB, "Stub I/O register write: %03x", address);
		break;
	}
}

uint16_t GBAIORead(struct GBA* gba, uint32_t address) {
	switch (address) {
	default:
		GBALog(GBA_LOG_STUB, "Stub I/O register read: %03x", address);
		break;
	}
	return 0;
}
