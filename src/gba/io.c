/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/io.h>

#include <mgba/internal/arm/macros.h>
#include <mgba/internal/gba/dma.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/rr/rr.h>
#include <mgba/internal/gba/serialize.h>

mLOG_DEFINE_CATEGORY(GBA_IO, "GBA I/O", "gba.io");

const char* const GBAIORegisterNames[] = {
	// Video
	"DISPCNT",
	0,
	"DISPSTAT",
	"VCOUNT",
	"BG0CNT",
	"BG1CNT",
	"BG2CNT",
	"BG3CNT",
	"BG0HOFS",
	"BG0VOFS",
	"BG1HOFS",
	"BG1VOFS",
	"BG2HOFS",
	"BG2VOFS",
	"BG3HOFS",
	"BG3VOFS",
	"BG2PA",
	"BG2PB",
	"BG2PC",
	"BG2PD",
	"BG2X_LO",
	"BG2X_HI",
	"BG2Y_LO",
	"BG2Y_HI",
	"BG3PA",
	"BG3PB",
	"BG3PC",
	"BG3PD",
	"BG3X_LO",
	"BG3X_HI",
	"BG3Y_LO",
	"BG3Y_HI",
	"WIN0H",
	"WIN1H",
	"WIN0V",
	"WIN1V",
	"WININ",
	"WINOUT",
	"MOSAIC",
	0,
	"BLDCNT",
	"BLDALPHA",
	"BLDY",
	0,
	0,
	0,
	0,
	0,

	// Sound
	"SOUND1CNT_LO",
	"SOUND1CNT_HI",
	"SOUND1CNT_X",
	0,
	"SOUND2CNT_LO",
	0,
	"SOUND2CNT_HI",
	0,
	"SOUND3CNT_LO",
	"SOUND3CNT_HI",
	"SOUND3CNT_X",
	0,
	"SOUND4CNT_LO",
	0,
	"SOUND4CNT_HI",
	0,
	"SOUNDCNT_LO",
	"SOUNDCNT_HI",
	"SOUNDCNT_X",
	0,
	"SOUNDBIAS",
	0,
	0,
	0,
	"WAVE_RAM0_LO",
	"WAVE_RAM0_HI",
	"WAVE_RAM1_LO",
	"WAVE_RAM1_HI",
	"WAVE_RAM2_LO",
	"WAVE_RAM2_HI",
	"WAVE_RAM3_LO",
	"WAVE_RAM3_HI",
	"FIFO_A_LO",
	"FIFO_A_HI",
	"FIFO_B_LO",
	"FIFO_B_HI",
	0,
	0,
	0,
	0,

	// DMA
	"DMA0SAD_LO",
	"DMA0SAD_HI",
	"DMA0DAD_LO",
	"DMA0DAD_HI",
	"DMA0CNT_LO",
	"DMA0CNT_HI",
	"DMA1SAD_LO",
	"DMA1SAD_HI",
	"DMA1DAD_LO",
	"DMA1DAD_HI",
	"DMA1CNT_LO",
	"DMA1CNT_HI",
	"DMA2SAD_LO",
	"DMA2SAD_HI",
	"DMA2DAD_LO",
	"DMA2DAD_HI",
	"DMA2CNT_LO",
	"DMA2CNT_HI",
	"DMA3SAD_LO",
	"DMA3SAD_HI",
	"DMA3DAD_LO",
	"DMA3DAD_HI",
	"DMA3CNT_LO",
	"DMA3CNT_HI",

	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,

	// Timers
	"TM0CNT_LO",
	"TM0CNT_HI",
	"TM1CNT_LO",
	"TM1CNT_HI",
	"TM2CNT_LO",
	"TM2CNT_HI",
	"TM3CNT_LO",
	"TM3CNT_HI",

	0, 0, 0, 0, 0, 0, 0, 0,

	// SIO
	"SIOMULTI0",
	"SIOMULTI1",
	"SIOMULTI2",
	"SIOMULTI3",
	"SIOCNT",
	"SIOMLT_SEND",
	0,
	0,
	"KEYINPUT",
	"KEYCNT",
	"RCNT",
	0,
	0,
	0,
	0,
	0,
	"JOYCNT",
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	"JOY_RECV_LO",
	"JOY_RECV_HI",
	"JOY_TRANS_LO",
	"JOY_TRANS_HI",
	"JOYSTAT",
	0,
	0,
	0,

	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,

	// Interrupts, etc
	"IE",
	"IF",
	"WAITCNT",
	0,
	"IME"
};

static const int _isValidRegister[REG_MAX >> 1] = {
	// Video
	1, 0, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 0,
	1, 1, 1, 0, 0, 0, 0, 0,
	// Audio
	1, 1, 1, 0, 1, 0, 1, 0,
	1, 1, 1, 0, 1, 0, 1, 0,
	1, 1, 1, 0, 1, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 0, 0, 0, 0,
	// DMA
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	// Timers
	1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0,
	// SIO
	1, 1, 1, 1, 1, 0, 0, 0,
	1, 1, 1, 0, 0, 0, 0, 0,
	1, 0, 0, 0, 0, 0, 0, 0,
	1, 0, 1, 0, 1, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	// Interrupts
	1, 1, 1, 0, 1
};

static const int _isRSpecialRegister[REG_MAX >> 1] = {
	// Video
	0, 0, 1, 1, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	// Audio
	0, 0, 1, 0, 0, 0, 1, 0,
	0, 0, 1, 0, 0, 0, 1, 0,
	0, 0, 0, 0, 1, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 0, 0, 0, 0,
	// DMA
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	// Timers
	1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0,
	// SIO
	1, 1, 1, 1, 1, 0, 0, 0,
	1, 1, 1, 0, 0, 0, 0, 0,
	1, 0, 0, 0, 0, 0, 0, 0,
	1, 0, 1, 0, 1, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	// Interrupts
};

static const int _isWSpecialRegister[REG_MAX >> 1] = {
	// Video
	0, 0, 1, 1, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	// Audio
	1, 1, 1, 0, 1, 0, 1, 0,
	1, 0, 1, 0, 1, 0, 1, 0,
	1, 0, 1, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 0, 0, 0, 0,
	// DMA
	0, 0, 0, 0, 0, 1, 0, 0,
	0, 0, 0, 1, 0, 0, 0, 0,
	0, 1, 0, 0, 0, 0, 0, 1,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	// Timers
	1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0,
	// SIO
	1, 1, 1, 1, 1, 0, 0, 0,
	1, 1, 1, 0, 0, 0, 0, 0,
	1, 0, 0, 0, 0, 0, 0, 0,
	1, 0, 1, 0, 1, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	// Interrupts
	1, 1, 0, 0, 1
};

void GBAIOInit(struct GBA* gba) {
	gba->memory.io[REG_DISPCNT >> 1] = 0x0080;
	gba->memory.io[REG_RCNT >> 1] = RCNT_INITIAL;
	gba->memory.io[REG_KEYINPUT >> 1] = 0x3FF;
	gba->memory.io[REG_SOUNDBIAS >> 1] = 0x200;
	gba->memory.io[REG_BG2PA >> 1] = 0x100;
	gba->memory.io[REG_BG2PD >> 1] = 0x100;
	gba->memory.io[REG_BG3PA >> 1] = 0x100;
	gba->memory.io[REG_BG3PD >> 1] = 0x100;

	if (!gba->biosVf) {
		gba->memory.io[REG_VCOUNT >> 1] = 0x7E;
		gba->memory.io[REG_POSTFLG >> 1] = 1;
	}
}

void GBAIOWrite(struct GBA* gba, uint32_t address, uint16_t value) {
	if (address < REG_SOUND1CNT_LO && (address > REG_VCOUNT || address == REG_DISPCNT)) {
		value = gba->video.renderer->writeVideoRegister(gba->video.renderer, address, value);
	} else {
		switch (address) {
		// Video
		case REG_DISPSTAT:
			value &= 0xFFF8;
			GBAVideoWriteDISPSTAT(&gba->video, value);
			return;

		case REG_VCOUNT:
			mLOG(GBA_IO, GAME_ERROR, "Write to read-only I/O register: %03X", address);
			return;

		// Audio
		case REG_SOUND1CNT_LO:
			GBAAudioWriteSOUND1CNT_LO(&gba->audio, value);
			value &= 0x007F;
			break;
		case REG_SOUND1CNT_HI:
			GBAAudioWriteSOUND1CNT_HI(&gba->audio, value);
			break;
		case REG_SOUND1CNT_X:
			GBAAudioWriteSOUND1CNT_X(&gba->audio, value);
			value &= 0x47FF;
			break;
		case REG_SOUND2CNT_LO:
			GBAAudioWriteSOUND2CNT_LO(&gba->audio, value);
			break;
		case REG_SOUND2CNT_HI:
			GBAAudioWriteSOUND2CNT_HI(&gba->audio, value);
			value &= 0x47FF;
			break;
		case REG_SOUND3CNT_LO:
			GBAAudioWriteSOUND3CNT_LO(&gba->audio, value);
			value &= 0x00E0;
			break;
		case REG_SOUND3CNT_HI:
			GBAAudioWriteSOUND3CNT_HI(&gba->audio, value);
			value &= 0xE03F;
			break;
		case REG_SOUND3CNT_X:
			GBAAudioWriteSOUND3CNT_X(&gba->audio, value);
			// TODO: The low bits need to not be readable, but still 8-bit writable
			value &= 0x47FF;
			break;
		case REG_SOUND4CNT_LO:
			GBAAudioWriteSOUND4CNT_LO(&gba->audio, value);
			value &= 0xFF3F;
			break;
		case REG_SOUND4CNT_HI:
			GBAAudioWriteSOUND4CNT_HI(&gba->audio, value);
			value &= 0x40FF;
			break;
		case REG_SOUNDCNT_LO:
			GBAAudioWriteSOUNDCNT_LO(&gba->audio, value);
			value &= 0xFF77;
			break;
		case REG_SOUNDCNT_HI:
			GBAAudioWriteSOUNDCNT_HI(&gba->audio, value);
			value &= 0x770F;
			break;
		case REG_SOUNDCNT_X:
			GBAAudioWriteSOUNDCNT_X(&gba->audio, value);
			value &= 0x0080;
			value |= gba->memory.io[REG_SOUNDCNT_X >> 1] & 0xF;
			break;
		case REG_SOUNDBIAS:
			GBAAudioWriteSOUNDBIAS(&gba->audio, value);
			break;

		case REG_WAVE_RAM0_LO:
		case REG_WAVE_RAM1_LO:
		case REG_WAVE_RAM2_LO:
		case REG_WAVE_RAM3_LO:
			GBAIOWrite32(gba, address, (gba->memory.io[(address >> 1) + 1] << 16) | value);
			break;

		case REG_WAVE_RAM0_HI:
		case REG_WAVE_RAM1_HI:
		case REG_WAVE_RAM2_HI:
		case REG_WAVE_RAM3_HI:
			GBAIOWrite32(gba, address - 2, gba->memory.io[(address >> 1) - 1] | (value << 16));
			break;

		case REG_FIFO_A_LO:
		case REG_FIFO_B_LO:
			GBAIOWrite32(gba, address, (gba->memory.io[(address >> 1) + 1] << 16) | value);
			break;

		case REG_FIFO_A_HI:
		case REG_FIFO_B_HI:
			GBAIOWrite32(gba, address - 2, gba->memory.io[(address >> 1) - 1] | (value << 16));
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
			GBADMAWriteCNT_LO(gba, 0, value);
			break;
		case REG_DMA0CNT_HI:
			value = GBADMAWriteCNT_HI(gba, 0, value);
			break;
		case REG_DMA1CNT_LO:
			GBADMAWriteCNT_LO(gba, 1, value);
			break;
		case REG_DMA1CNT_HI:
			value = GBADMAWriteCNT_HI(gba, 1, value);
			break;
		case REG_DMA2CNT_LO:
			GBADMAWriteCNT_LO(gba, 2, value);
			break;
		case REG_DMA2CNT_HI:
			value = GBADMAWriteCNT_HI(gba, 2, value);
			break;
		case REG_DMA3CNT_LO:
			GBADMAWriteCNT_LO(gba, 3, value);
			break;
		case REG_DMA3CNT_HI:
			value = GBADMAWriteCNT_HI(gba, 3, value);
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

		// SIO
		case REG_SIOCNT:
			GBASIOWriteSIOCNT(&gba->sio, value);
			break;
		case REG_RCNT:
			value &= 0xC1FF;
			GBASIOWriteRCNT(&gba->sio, value);
			break;
		case REG_JOY_TRANS_LO:
		case REG_JOY_TRANS_HI:
			gba->memory.io[REG_JOYSTAT >> 1] |= JOYSTAT_TRANS_BIT;
			// Fall through
		case REG_SIOMLT_SEND:
		case REG_JOYCNT:
		case REG_JOYSTAT:
		case REG_JOY_RECV_LO:
		case REG_JOY_RECV_HI:
			value = GBASIOWriteRegister(&gba->sio, address, value);
			break;

		// Interrupts and misc
		case REG_KEYCNT:
			value &= 0xC3FF;
			gba->memory.io[address >> 1] = value;
			GBATestKeypadIRQ(gba);
			return;
		case REG_WAITCNT:
			value &= 0x5FFF;
			GBAAdjustWaitstates(gba, value);
			break;
		case REG_IE:
			gba->memory.io[REG_IE >> 1] = value;
			GBATestIRQ(gba->cpu);
			return;
		case REG_IF:
			value = gba->memory.io[REG_IF >> 1] & ~value;
			gba->memory.io[REG_IF >> 1] = value;
			GBATestIRQ(gba->cpu);
			return;
		case REG_IME:
			gba->memory.io[REG_IME >> 1] = value;
			GBATestIRQ(gba->cpu);
			return;
		case REG_MAX:
			// Some bad interrupt libraries will write to this
			break;
		case REG_DEBUG_ENABLE:
			gba->debug = value == 0xC0DE;
			return;
		case REG_DEBUG_FLAGS:
			if (gba->debug) {
				GBADebug(gba, value);
				return;
			}
			// Fall through
		default:
			if (address >= REG_DEBUG_STRING && address - REG_DEBUG_STRING < sizeof(gba->debugString)) {
				STORE_16LE(value, address - REG_DEBUG_STRING, gba->debugString);
				return;
			}
			mLOG(GBA_IO, STUB, "Stub I/O register write: %03X", address);
			if (address >= REG_MAX) {
				mLOG(GBA_IO, GAME_ERROR, "Write to unused I/O register: %03X", address);
				return;
			}
			break;
		}
	}
	gba->memory.io[address >> 1] = value;
}

void GBAIOWrite8(struct GBA* gba, uint32_t address, uint8_t value) {
	if (address == REG_HALTCNT) {
		value &= 0x80;
		if (!value) {
			GBAHalt(gba);
		} else {
			GBAStop(gba);
		}
		return;
	}
	if (address == REG_POSTFLG) {
		gba->memory.io[(address & (SIZE_IO - 1)) >> 1] = value;
		return;
	}
	if (address >= REG_DEBUG_STRING && address - REG_DEBUG_STRING < sizeof(gba->debugString)) {
		gba->debugString[address - REG_DEBUG_STRING] = value;
		return;
	}
	if (address > SIZE_IO) {
		return;
	}
	uint16_t value16 = value << (8 * (address & 1));
	value16 |= (gba->memory.io[(address & (SIZE_IO - 1)) >> 1]) & ~(0xFF << (8 * (address & 1)));
	GBAIOWrite(gba, address & 0xFFFFFFFE, value16);
}

void GBAIOWrite32(struct GBA* gba, uint32_t address, uint32_t value) {
	switch (address) {
	case REG_WAVE_RAM0_LO:
		GBAAudioWriteWaveRAM(&gba->audio, 0, value);
		break;
	case REG_WAVE_RAM1_LO:
		GBAAudioWriteWaveRAM(&gba->audio, 1, value);
		break;
	case REG_WAVE_RAM2_LO:
		GBAAudioWriteWaveRAM(&gba->audio, 2, value);
		break;
	case REG_WAVE_RAM3_LO:
		GBAAudioWriteWaveRAM(&gba->audio, 3, value);
		break;
	case REG_FIFO_A_LO:
	case REG_FIFO_B_LO:
		GBAAudioWriteFIFO(&gba->audio, address, value);
		break;
	case REG_DMA0SAD_LO:
		value = GBADMAWriteSAD(gba, 0, value);
		break;
	case REG_DMA0DAD_LO:
		value = GBADMAWriteDAD(gba, 0, value);
		break;
	case REG_DMA1SAD_LO:
		value = GBADMAWriteSAD(gba, 1, value);
		break;
	case REG_DMA1DAD_LO:
		value = GBADMAWriteDAD(gba, 1, value);
		break;
	case REG_DMA2SAD_LO:
		value = GBADMAWriteSAD(gba, 2, value);
		break;
	case REG_DMA2DAD_LO:
		value = GBADMAWriteDAD(gba, 2, value);
		break;
	case REG_DMA3SAD_LO:
		value = GBADMAWriteSAD(gba, 3, value);
		break;
	case REG_DMA3DAD_LO:
		value = GBADMAWriteDAD(gba, 3, value);
		break;
	default:
		if (address >= REG_DEBUG_STRING && address - REG_DEBUG_STRING < sizeof(gba->debugString)) {
			STORE_32LE(value, address - REG_DEBUG_STRING, gba->debugString);
			return;
		}
		GBAIOWrite(gba, address, value & 0xFFFF);
		GBAIOWrite(gba, address | 2, value >> 16);
		return;
	}
	gba->memory.io[address >> 1] = value;
	gba->memory.io[(address >> 1) + 1] = value >> 16;
}

bool GBAIOIsReadConstant(uint32_t address) {
	switch (address) {
	default:
		return false;
	case REG_BG0CNT:
	case REG_BG1CNT:
	case REG_BG2CNT:
	case REG_BG3CNT:
	case REG_WININ:
	case REG_WINOUT:
	case REG_BLDCNT:
	case REG_BLDALPHA:
	case REG_SOUND1CNT_LO:
	case REG_SOUND1CNT_HI:
	case REG_SOUND1CNT_X:
	case REG_SOUND2CNT_LO:
	case REG_SOUND2CNT_HI:
	case REG_SOUND3CNT_LO:
	case REG_SOUND3CNT_HI:
	case REG_SOUND3CNT_X:
	case REG_SOUND4CNT_LO:
	case REG_SOUND4CNT_HI:
	case REG_SOUNDCNT_LO:
	case REG_SOUNDCNT_HI:
	case REG_TM0CNT_HI:
	case REG_TM1CNT_HI:
	case REG_TM2CNT_HI:
	case REG_TM3CNT_HI:
	case REG_KEYINPUT:
	case REG_KEYCNT:
	case REG_IE:
		return true;
	}
}

uint16_t GBAIORead(struct GBA* gba, uint32_t address) {
	if (!GBAIOIsReadConstant(address)) {
		// Most IO reads need to disable idle removal
		gba->haltPending = false;
	}

	switch (address) {
	// Reading this takes two cycles (1N+1I), so let's remove them preemptively
	case REG_TM0CNT_LO:
		GBATimerUpdateRegister(gba, 0, 4);
		break;
	case REG_TM1CNT_LO:
		GBATimerUpdateRegister(gba, 1, 4);
		break;
	case REG_TM2CNT_LO:
		GBATimerUpdateRegister(gba, 2, 4);
		break;
	case REG_TM3CNT_LO:
		GBATimerUpdateRegister(gba, 3, 4);
		break;

	case REG_KEYINPUT:
		if (gba->rr && gba->rr->isPlaying(gba->rr)) {
			return 0x3FF ^ gba->rr->queryInput(gba->rr);
		} else {
			uint16_t input = 0x3FF;
			if (gba->keyCallback) {
				input = gba->keyCallback->readKeys(gba->keyCallback);
				if (gba->keySource) {
					*gba->keySource = input;
				}
			} else if (gba->keySource) {
				input = *gba->keySource;
			}
			if (!gba->allowOpposingDirections) {
				unsigned rl = input & 0x030;
				unsigned ud = input & 0x0C0;
				input &= 0x30F;
				if (rl != 0x030) {
					input |= rl;
				}
				if (ud != 0x0C0) {
					input |= ud;
				}
			}
			if (gba->rr && gba->rr->isRecording(gba->rr)) {
				gba->rr->logInput(gba->rr, input);
			}
			return 0x3FF ^ input;
		}

	case REG_SIOCNT:
		return gba->sio.siocnt;
	case REG_RCNT:
		return gba->sio.rcnt;

	case REG_BG0HOFS:
	case REG_BG0VOFS:
	case REG_BG1HOFS:
	case REG_BG1VOFS:
	case REG_BG2HOFS:
	case REG_BG2VOFS:
	case REG_BG3HOFS:
	case REG_BG3VOFS:
	case REG_BG2PA:
	case REG_BG2PB:
	case REG_BG2PC:
	case REG_BG2PD:
	case REG_BG2X_LO:
	case REG_BG2X_HI:
	case REG_BG2Y_LO:
	case REG_BG2Y_HI:
	case REG_BG3PA:
	case REG_BG3PB:
	case REG_BG3PC:
	case REG_BG3PD:
	case REG_BG3X_LO:
	case REG_BG3X_HI:
	case REG_BG3Y_LO:
	case REG_BG3Y_HI:
	case REG_WIN0H:
	case REG_WIN1H:
	case REG_WIN0V:
	case REG_WIN1V:
	case REG_MOSAIC:
	case REG_BLDY:
	case REG_FIFO_A_LO:
	case REG_FIFO_A_HI:
	case REG_FIFO_B_LO:
	case REG_FIFO_B_HI:
	case REG_DMA0SAD_LO:
	case REG_DMA0SAD_HI:
	case REG_DMA0DAD_LO:
	case REG_DMA0DAD_HI:
	case REG_DMA1SAD_LO:
	case REG_DMA1SAD_HI:
	case REG_DMA1DAD_LO:
	case REG_DMA1DAD_HI:
	case REG_DMA2SAD_LO:
	case REG_DMA2SAD_HI:
	case REG_DMA2DAD_LO:
	case REG_DMA2DAD_HI:
	case REG_DMA3SAD_LO:
	case REG_DMA3SAD_HI:
	case REG_DMA3DAD_LO:
	case REG_DMA3DAD_HI:
		// Write-only register
		mLOG(GBA_IO, GAME_ERROR, "Read from write-only I/O register: %03X", address);
		return GBALoadBad(gba->cpu);

	case REG_DMA0CNT_LO:
	case REG_DMA1CNT_LO:
	case REG_DMA2CNT_LO:
	case REG_DMA3CNT_LO:
		// Write-only register
		mLOG(GBA_IO, GAME_ERROR, "Read from write-only I/O register: %03X", address);
		return 0;

	case REG_JOY_RECV_LO:
	case REG_JOY_RECV_HI:
		gba->memory.io[REG_JOYSTAT >> 1] &= ~JOYSTAT_RECV_BIT;
		break;

	case REG_SOUNDBIAS:
	case REG_POSTFLG:
		mLOG(GBA_IO, STUB, "Stub I/O register read: %03x", address);
		break;
	case REG_SOUND1CNT_LO:
	case REG_SOUND1CNT_HI:
	case REG_SOUND1CNT_X:
	case REG_SOUND2CNT_LO:
	case REG_SOUND2CNT_HI:
	case REG_SOUND3CNT_LO:
	case REG_SOUND3CNT_HI:
	case REG_SOUND3CNT_X:
	case REG_SOUND4CNT_LO:
	case REG_SOUND4CNT_HI:
	case REG_SOUNDCNT_LO:
		if (!GBAudioEnableIsEnable(gba->memory.io[REG_SOUNDCNT_X >> 1])) {
			// TODO: Is writing allowed when the circuit is disabled?
			return 0;
		}
		// Fall through
	case REG_DISPCNT:
	case REG_DISPSTAT:
	case REG_VCOUNT:
	case REG_BG0CNT:
	case REG_BG1CNT:
	case REG_BG2CNT:
	case REG_BG3CNT:
	case REG_WININ:
	case REG_WINOUT:
	case REG_BLDCNT:
	case REG_BLDALPHA:
	case REG_SOUNDCNT_HI:
	case REG_SOUNDCNT_X:
	case REG_WAVE_RAM0_LO:
	case REG_WAVE_RAM0_HI:
	case REG_WAVE_RAM1_LO:
	case REG_WAVE_RAM1_HI:
	case REG_WAVE_RAM2_LO:
	case REG_WAVE_RAM2_HI:
	case REG_WAVE_RAM3_LO:
	case REG_WAVE_RAM3_HI:
	case REG_DMA0CNT_HI:
	case REG_DMA1CNT_HI:
	case REG_DMA2CNT_HI:
	case REG_DMA3CNT_HI:
	case REG_TM0CNT_HI:
	case REG_TM1CNT_HI:
	case REG_TM2CNT_HI:
	case REG_TM3CNT_HI:
	case REG_KEYCNT:
	case REG_SIOMULTI0:
	case REG_SIOMULTI1:
	case REG_SIOMULTI2:
	case REG_SIOMULTI3:
	case REG_SIOMLT_SEND:
	case REG_JOYCNT:
	case REG_JOY_TRANS_LO:
	case REG_JOY_TRANS_HI:
	case REG_JOYSTAT:
	case REG_IE:
	case REG_IF:
	case REG_WAITCNT:
	case REG_IME:
		// Handled transparently by registers
		break;
	case REG_MAX:
		// Some bad interrupt libraries will read from this
	case 0x066:
	case 0x06E:
	case 0x076:
	case 0x07A:
	case 0x07E:
	case 0x086:
	case 0x08A:
	case 0x136:
	case 0x142:
	case 0x15A:
	case 0x206:
		mLOG(GBA_IO, GAME_ERROR, "Read from unused I/O register: %03X", address);
		return 0;
	case REG_DEBUG_ENABLE:
		if (gba->debug) {
			return 0x1DEA;
		}
		// Fall through
	default:
		mLOG(GBA_IO, GAME_ERROR, "Read from unused I/O register: %03X", address);
		return GBALoadBad(gba->cpu);
	}
	return gba->memory.io[address >> 1];
}

void GBAIOSerialize(struct GBA* gba, struct GBASerializedState* state) {
	int i;
	for (i = 0; i < REG_MAX; i += 2) {
		if (_isRSpecialRegister[i >> 1]) {
			STORE_16(gba->memory.io[i >> 1], i, state->io);
		} else if (_isValidRegister[i >> 1]) {
			uint16_t reg = GBAIORead(gba, i);
			STORE_16(reg, i, state->io);
		}
	}

	for (i = 0; i < 4; ++i) {
		STORE_16(gba->memory.io[(REG_DMA0CNT_LO + i * 12) >> 1], (REG_DMA0CNT_LO + i * 12), state->io);
		STORE_16(gba->timers[i].reload, 0, &state->timers[i].reload);
		STORE_32(gba->timers[i].lastEvent - mTimingCurrentTime(&gba->timing), 0, &state->timers[i].lastEvent);
		STORE_32(gba->timers[i].event.when - mTimingCurrentTime(&gba->timing), 0, &state->timers[i].nextEvent);
		STORE_32(gba->timers[i].flags, 0, &state->timers[i].flags);
		STORE_32(gba->memory.dma[i].nextSource, 0, &state->dma[i].nextSource);
		STORE_32(gba->memory.dma[i].nextDest, 0, &state->dma[i].nextDest);
		STORE_32(gba->memory.dma[i].nextCount, 0, &state->dma[i].nextCount);
		STORE_32(gba->memory.dma[i].when, 0, &state->dma[i].when);
	}

	state->dmaTransferRegister = gba->memory.dmaTransferRegister;

	GBAHardwareSerialize(&gba->memory.hw, state);
}

void GBAIODeserialize(struct GBA* gba, const struct GBASerializedState* state) {
	int i;
	for (i = 0; i < REG_MAX; i += 2) {
		if (_isWSpecialRegister[i >> 1]) {
			LOAD_16(gba->memory.io[i >> 1], i, state->io);
		} else if (_isValidRegister[i >> 1]) {
			uint16_t reg;
			LOAD_16(reg, i, state->io);
			GBAIOWrite(gba, i, reg);
		}
	}

	uint32_t when;
	for (i = 0; i < 4; ++i) {
		LOAD_16(gba->timers[i].reload, 0, &state->timers[i].reload);
		LOAD_32(gba->timers[i].flags, 0, &state->timers[i].flags);
		if (i > 0 && GBATimerFlagsIsCountUp(gba->timers[i].flags)) {
			// Overwrite invalid values in savestate
			gba->timers[i].lastEvent = 0;
		} else {
			LOAD_32(when, 0, &state->timers[i].lastEvent);
			gba->timers[i].lastEvent = when + mTimingCurrentTime(&gba->timing);
		}
		LOAD_32(when, 0, &state->timers[i].nextEvent);
		if (GBATimerFlagsIsEnable(gba->timers[i].flags)) {
			mTimingSchedule(&gba->timing, &gba->timers[i].event, when);
		}

		LOAD_16(gba->memory.dma[i].reg, (REG_DMA0CNT_HI + i * 12), state->io);
		LOAD_32(gba->memory.dma[i].nextSource, 0, &state->dma[i].nextSource);
		LOAD_32(gba->memory.dma[i].nextDest, 0, &state->dma[i].nextDest);
		LOAD_32(gba->memory.dma[i].nextCount, 0, &state->dma[i].nextCount);
		LOAD_32(gba->memory.dma[i].when, 0, &state->dma[i].when);
		if (GBADMARegisterGetTiming(gba->memory.dma[i].reg) != GBA_DMA_TIMING_NOW) {
			GBADMASchedule(gba, i, &gba->memory.dma[i]);
		}
	}
	GBAAudioWriteSOUNDCNT_X(&gba->audio, gba->memory.io[REG_SOUNDCNT_X >> 1]);
	gba->memory.dmaTransferRegister = state->dmaTransferRegister;
	GBADMAUpdate(gba);
	GBAHardwareDeserialize(&gba->memory.hw, state);
}
