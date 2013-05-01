#include "gba-io.h"

#include "gba-video.h"

void GBAIOInit(struct GBA* gba) {
	gba->memory.io[REG_DISPCNT >> 1] = 0x0080;
	gba->memory.io[REG_KEYINPUT >> 1] = 0x3FF;
}

void GBAIOWrite(struct GBA* gba, uint32_t address, uint16_t value) {
	if (address < REG_SOUND1CNT_LO && address != REG_DISPSTAT) {
		gba->video.renderer->writeVideoRegister(gba->video.renderer, address, value);
	} else {
		switch (address) {
		// Video
		case REG_DISPSTAT:
			GBAVideoWriteDISPSTAT(&gba->video, value);
			break;

		// DMA
		case REG_DMA0SAD_LO:
		case REG_DMA0DAD_LO:
		case REG_DMA1SAD_LO:
		case REG_DMA1DAD_LO:
		case REG_DMA2SAD_LO:
		case REG_DMA2DAD_LO:
		case REG_DMA3SAD_LO:
		case REG_DMA3DAD_LO:
			GBAIOWrite32(gba, address, (gba->memory.io[(address >> 1) + 1] << 16) | value);
			break;

		case REG_DMA0SAD_HI:
		case REG_DMA0DAD_HI:
		case REG_DMA1SAD_HI:
		case REG_DMA1DAD_HI:
		case REG_DMA2SAD_HI:
		case REG_DMA2DAD_HI:
		case REG_DMA3SAD_HI:
		case REG_DMA3DAD_HI:
			GBAIOWrite32(gba, address - 2, gba->memory.io[(address >> 1) - 1] | (value << 16));
			break;

		case REG_DMA0CNT_LO:
			GBAMemoryWriteDMACNT_LO(&gba->memory, 0, value);
			break;
		case REG_DMA0CNT_HI:
			value = GBAMemoryWriteDMACNT_HI(&gba->memory, 0, value);
			break;
		case REG_DMA1CNT_LO:
			GBAMemoryWriteDMACNT_LO(&gba->memory, 1, value);
			break;
		case REG_DMA1CNT_HI:
			value = GBAMemoryWriteDMACNT_HI(&gba->memory, 1, value);
			break;
		case REG_DMA2CNT_LO:
			GBAMemoryWriteDMACNT_LO(&gba->memory, 2, value);
			break;
		case REG_DMA2CNT_HI:
			value = GBAMemoryWriteDMACNT_HI(&gba->memory, 2, value);
			break;
		case REG_DMA3CNT_LO:
			GBAMemoryWriteDMACNT_LO(&gba->memory, 3, value);
			break;
		case REG_DMA3CNT_HI:
			value = GBAMemoryWriteDMACNT_HI(&gba->memory, 3, value);
			break;

		// Timers
		case REG_TM0CNT_LO:
			GBATimerWriteTMCNT_LO(gba, 0, value);
			return;
		case REG_TM1CNT_LO:
			GBATimerWriteTMCNT_LO(gba, 1, value);
			return;
		case REG_TM2CNT_LO:
			GBATimerWriteTMCNT_LO(gba, 2, value);
			return;
		case REG_TM3CNT_LO:
			GBATimerWriteTMCNT_LO(gba, 3, value);
			return;

		case REG_TM0CNT_HI:
			value &= 0x00C7;
			GBATimerWriteTMCNT_HI(gba, 0, value);
			break;
		case REG_TM1CNT_HI:
			value &= 0x00C7;
			GBATimerWriteTMCNT_HI(gba, 1, value);
			break;
		case REG_TM2CNT_HI:
			value &= 0x00C7;
			GBATimerWriteTMCNT_HI(gba, 2, value);
			break;
		case REG_TM3CNT_HI:
			value &= 0x00C7;
			GBATimerWriteTMCNT_HI(gba, 3, value);
			break;

		// Interrupts and misc
		case REG_WAITCNT:
			GBAAdjustWaitstates(&gba->memory, value);
			break;
		case REG_IE:
			GBAWriteIE(gba, value);
			break;
		case REG_IF:
			value = gba->memory.io[REG_IF >> 1] & ~value;
			break;
		case REG_IME:
			GBAWriteIME(gba, value);
			break;
		case REG_HALTCNT:
			value &= 0x80;
			if (!value) {
				GBAHalt(gba);
			} else {
				GBALog(GBA_LOG_STUB, "Stop unimplemented");
			}
			return;
		default:
			GBALog(GBA_LOG_STUB, "Stub I/O register write: %03x", address);
			break;
		}
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
		return;
	}
	gba->memory.io[address >> 1] = value;
	gba->memory.io[(address >> 1) + 1] = value >> 16;
}

uint16_t GBAIORead(struct GBA* gba, uint32_t address) {
	switch (address) {
	case REG_DISPSTAT:
		return gba->memory.io[REG_DISPSTAT >> 1] | GBAVideoReadDISPSTAT(&gba->video);
		break;

	case REG_TM0CNT_LO:
		GBATimerUpdateRegister(gba, 0);
		break;
	case REG_TM1CNT_LO:
		GBATimerUpdateRegister(gba, 1);
		break;
	case REG_TM2CNT_LO:
		GBATimerUpdateRegister(gba, 2);
		break;
	case REG_TM3CNT_LO:
		GBATimerUpdateRegister(gba, 3);
		break;

	case REG_KEYINPUT:
		if (gba->keySource) {
			return 0x3FF ^ *gba->keySource;
		}
		break;

	case REG_DMA0CNT_LO:
	case REG_DMA1CNT_LO:
	case REG_DMA2CNT_LO:
	case REG_DMA3CNT_LO:
		// Write-only register
		return 0;
	case REG_VCOUNT:
	case REG_DMA0CNT_HI:
	case REG_DMA1CNT_HI:
	case REG_DMA2CNT_HI:
	case REG_DMA3CNT_HI:
	case REG_IE:
	case REG_IF:
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
