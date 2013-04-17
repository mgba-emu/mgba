#include "gba-io.h"

#include "gba-video.h"

void GBAIOWrite(struct GBA* gba, uint32_t address, uint16_t value) {
	switch (address) {
	case REG_DISPSTAT:
		GBAVideoWriteDISPSTAT(&gba->video, value);
		break;
	case REG_DMA0CNT_LO:
		GBAMemoryWriteDMACNT_LO(&gba->memory, 0, value);
		break;
	case REG_DMA0CNT_HI:
		GBAMemoryWriteDMACNT_HI(&gba->memory, 0, value);
		break;
	case REG_DMA1CNT_LO:
		GBAMemoryWriteDMACNT_LO(&gba->memory, 1, value);
		break;
	case REG_DMA1CNT_HI:
		GBAMemoryWriteDMACNT_HI(&gba->memory, 1, value);
		break;
	case REG_DMA2CNT_LO:
		GBAMemoryWriteDMACNT_LO(&gba->memory, 2, value);
		break;
	case REG_DMA2CNT_HI:
		GBAMemoryWriteDMACNT_HI(&gba->memory, 2, value);
		break;
	case REG_DMA3CNT_LO:
		GBAMemoryWriteDMACNT_LO(&gba->memory, 3, value);
		break;
	case REG_DMA3CNT_HI:
		GBAMemoryWriteDMACNT_HI(&gba->memory, 3, value);
		break;

	case REG_WAITCNT:
		GBAAdjustWaitstates(&gba->memory, value);
		break;
	case REG_IE:
		GBAWriteIE(gba, value);
		break;
	case REG_IME:
		GBAWriteIME(gba, value);
		break;
	default:
		GBALog(GBA_LOG_STUB, "Stub I/O register write: %03x", address);
		break;
	}
	gba->memory.io[address >> 1] = value;
}

void GBAIOWrite32(struct GBA* gba, uint32_t address, uint32_t value) {
	switch (address) {
	case REG_DMA0SAD_LO:
		GBAMemoryWriteDMASAD(&gba->memory, 0, value);
		break;
	case REG_DMA0DAD_LO:
		GBAMemoryWriteDMADAD(&gba->memory, 0, value);
		break;
	case REG_DMA1SAD_LO:
		GBAMemoryWriteDMASAD(&gba->memory, 1, value);
		break;
	case REG_DMA1DAD_LO:
		GBAMemoryWriteDMADAD(&gba->memory, 1, value);
		break;
	case REG_DMA2SAD_LO:
		GBAMemoryWriteDMASAD(&gba->memory, 2, value);
		break;
	case REG_DMA2DAD_LO:
		GBAMemoryWriteDMADAD(&gba->memory, 2, value);
		break;
	case REG_DMA3SAD_LO:
		GBAMemoryWriteDMASAD(&gba->memory, 3, value);
		break;
	case REG_DMA3DAD_LO:
		GBAMemoryWriteDMADAD(&gba->memory, 3, value);
		break;
	default:
		GBAIOWrite(gba, address, value & 0xFFFF);
		GBAIOWrite(gba, address | 2, value >> 16);
		break;
	}
}

uint16_t GBAIORead(struct GBA* gba, uint32_t address) {
	switch (address) {
	case REG_DISPSTAT:
		return GBAVideoReadDISPSTAT(&gba->video);
		break;
	case REG_IE:
	case REG_WAITCNT:
	case REG_IME:
		// Handled transparently by registers
		break;
	default:
		GBALog(GBA_LOG_STUB, "Stub I/O register read: %03x", address);
		break;
	}
	return gba->memory.io[address >> 1];
}
