#include "gba-io.h"

#include "gba-video.h"

void GBAIOWrite(struct GBA* gba, uint32_t address, uint16_t value) {
	switch (address) {
	case REG_DISPSTAT:
		GBAVideoWriteDISPSTAT(&gba->video, value);
		break;
	case REG_WAITCNT:
		GBAAdjustWaitstates(&gba->memory, value);
		break;
	default:
		GBALog(GBA_LOG_STUB, "Stub I/O register write: %03x", address);
		break;
	}
	gba->memory.io[address >> 1] = value;
}

uint16_t GBAIORead(struct GBA* gba, uint32_t address) {
	switch (address) {
	case REG_DISPSTAT:
		return GBAVideoReadDISPSTAT(&gba->video);
		break;
	case REG_WAITCNT:
		// Handled transparently by registers
		break;
	default:
		GBALog(GBA_LOG_STUB, "Stub I/O register read: %03x", address);
		break;
	}
	return gba->memory.io[address >> 1];
}
