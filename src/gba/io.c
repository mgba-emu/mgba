/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/io.h>

#include <mgba/internal/arm/macros.h>
#include <mgba/internal/gba/dma.h>
#include <mgba/internal/gba/gba.h>
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

static const int _isValidRegister[REG_INTERNAL_MAX >> 1] = {
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
	1, 1, 1, 1, 1, 0, 0, 0,
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
	1, 1, 1, 0, 1, 0, 0, 0,
	// Internal registers
	1, 1
};

static const int _isRSpecialRegister[REG_INTERNAL_MAX >> 1] = {
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
	1, 1, 1, 1, 1, 0, 0, 0,
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
	0, 0, 0, 0, 0, 0, 0, 0,
	// Internal registers
	1, 1
};

static const int _isWSpecialRegister[REG_INTERNAL_MAX >> 1] = {
	// Video
	0, 0, 1, 1, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	// Audio
	0, 0, 1, 0, 0, 0, 1, 0,
	0, 0, 1, 0, 0, 0, 1, 0,
	0, 0, 1, 0, 0, 0, 0, 0,
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
	1, 1, 1, 1, 1, 0, 0, 0,
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
	1, 1, 0, 0, 1, 0, 0, 0,
	// Internal registers
	1, 1
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
	gba->memory.io[REG_INTERNAL_EXWAITCNT_LO >> 1] = 0x20;
	gba->memory.io[REG_INTERNAL_EXWAITCNT_HI >> 1] = 0xD00;

	if (!gba->biosVf) {
		gba->memory.io[REG_VCOUNT >> 1] = 0x7E;
		gba->memory.io[REG_POSTFLG >> 1] = 1;
	}
}

void GBAIOWrite(struct GBA* gba, uint32_t address, uint16_t value) {
	if (address < REG_SOUND1CNT_LO && (address > REG_VCOUNT || address < REG_DISPSTAT)) {
		gba->memory.io[address >> 1] = gba->video.renderer->writeVideoRegister(gba->video.renderer, address, value);
		return;
	}

	if (address >= REG_SOUND1CNT_LO && address <= REG_SOUNDCNT_LO && !gba->audio.enable) {
		// Ignore writes to most audio registers if the hardware is off.
		return;
	}

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
		value &= 0xC3FE;
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
		return;

	case REG_FIFO_A_HI:
	case REG_FIFO_B_HI:
		GBAIOWrite32(gba, address - 2, gba->memory.io[(address >> 1) - 1] | (value << 16));
		return;

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
		GBADMAWriteCNT_LO(gba, 0, value & 0x3FFF);
		break;
	case REG_DMA0CNT_HI:
		value = GBADMAWriteCNT_HI(gba, 0, value);
		break;
	case REG_DMA1CNT_LO:
		GBADMAWriteCNT_LO(gba, 1, value & 0x3FFF);
		break;
	case REG_DMA1CNT_HI:
		value = GBADMAWriteCNT_HI(gba, 1, value);
		break;
	case REG_DMA2CNT_LO:
		GBADMAWriteCNT_LO(gba, 2, value & 0x3FFF);
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
		gba->memory.io[REG_JOYSTAT >> 1] |= JOYSTAT_TRANS;
		// Fall through
	case REG_SIODATA32_LO:
	case REG_SIODATA32_HI:
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
		if (gba->keysLast < 0x400) {
			gba->keysLast &= gba->memory.io[address >> 1] | ~value;
		}
		gba->memory.io[address >> 1] = value;
		GBATestKeypadIRQ(gba);
		return;
	case REG_WAITCNT:
		value &= 0x5FFF;
		GBAAdjustWaitstates(gba, value);
		break;
	case REG_IE:
		gba->memory.io[REG_IE >> 1] = value;
		GBATestIRQ(gba, 1);
		return;
	case REG_IF:
		value = gba->memory.io[REG_IF >> 1] & ~value;
		gba->memory.io[REG_IF >> 1] = value;
		GBATestIRQ(gba, 1);
		return;
	case REG_IME:
		gba->memory.io[REG_IME >> 1] = value & 1;
		GBATestIRQ(gba, 1);
		return;
	case REG_MAX:
		// Some bad interrupt libraries will write to this
		break;
	case REG_EXWAITCNT_HI:
		// This register sits outside of the normal I/O block, so we need to stash it somewhere unused
		address = REG_INTERNAL_EXWAITCNT_HI;
		value &= 0xFF00;
		GBAAdjustEWRAMWaitstates(gba, value);
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
		gba->memory.io[(address & (GBA_SIZE_IO - 1)) >> 1] = value;
		return;
	}
	if (address >= REG_DEBUG_STRING && address - REG_DEBUG_STRING < sizeof(gba->debugString)) {
		gba->debugString[address - REG_DEBUG_STRING] = value;
		return;
	}
	if (address > GBA_SIZE_IO) {
		return;
	}
	uint16_t value16 = value << (8 * (address & 1));
	value16 |= (gba->memory.io[(address & (GBA_SIZE_IO - 1)) >> 1]) & ~(0xFF << (8 * (address & 1)));
	GBAIOWrite(gba, address & 0xFFFFFFFE, value16);
}

void GBAIOWrite32(struct GBA* gba, uint32_t address, uint32_t value) {
	switch (address) {
	// Wave RAM can be written and read even if the audio hardware is disabled.
	// However, it is not possible to switch between the two banks because it
	// isn't possible to write to register SOUND3CNT_LO.
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
		value = GBAAudioWriteFIFO(&gba->audio, address, value);
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
		GBATimerUpdateRegister(gba, 0, 2);
		break;
	case REG_TM1CNT_LO:
		GBATimerUpdateRegister(gba, 1, 2);
		break;
	case REG_TM2CNT_LO:
		GBATimerUpdateRegister(gba, 2, 2);
		break;
	case REG_TM3CNT_LO:
		GBATimerUpdateRegister(gba, 3, 2);
		break;

	case REG_KEYINPUT: {
			size_t c;
			for (c = 0; c < mCoreCallbacksListSize(&gba->coreCallbacks); ++c) {
				struct mCoreCallbacks* callbacks = mCoreCallbacksListGetPointer(&gba->coreCallbacks, c);
				if (callbacks->keysRead) {
					callbacks->keysRead(callbacks->context);
				}
			}
			bool allowOpposingDirections = gba->allowOpposingDirections;
			if (gba->keyCallback) {
				gba->keysActive = gba->keyCallback->readKeys(gba->keyCallback);
				if (!allowOpposingDirections) {
					allowOpposingDirections = gba->keyCallback->requireOpposingDirections;
				}
			}
			uint16_t input = gba->keysActive;
			if (!allowOpposingDirections) {
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
			gba->memory.io[address >> 1] = 0x3FF ^ input;
		}
		break;
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
		// Many, many things read from the DMA register
	case REG_MAX:
		// Some bad interrupt libraries will read from this
		// (Silent) write-only register
		return 0;

	case REG_JOY_RECV_LO:
	case REG_JOY_RECV_HI:
		gba->memory.io[REG_JOYSTAT >> 1] &= ~JOYSTAT_RECV;
		break;

	case REG_POSTFLG:
		mLOG(GBA_IO, STUB, "Stub I/O register read: %03x", address);
		break;

	// Wave RAM can be written and read even if the audio hardware is disabled.
	// However, it is not possible to switch between the two banks because it
	// isn't possible to write to register SOUND3CNT_LO.
	case REG_WAVE_RAM0_LO:
		return GBAAudioReadWaveRAM(&gba->audio, 0) & 0xFFFF;
	case REG_WAVE_RAM0_HI:
		return GBAAudioReadWaveRAM(&gba->audio, 0) >> 16;
	case REG_WAVE_RAM1_LO:
		return GBAAudioReadWaveRAM(&gba->audio, 1) & 0xFFFF;
	case REG_WAVE_RAM1_HI:
		return GBAAudioReadWaveRAM(&gba->audio, 1) >> 16;
	case REG_WAVE_RAM2_LO:
		return GBAAudioReadWaveRAM(&gba->audio, 2) & 0xFFFF;
	case REG_WAVE_RAM2_HI:
		return GBAAudioReadWaveRAM(&gba->audio, 2) >> 16;
	case REG_WAVE_RAM3_LO:
		return GBAAudioReadWaveRAM(&gba->audio, 3) & 0xFFFF;
	case REG_WAVE_RAM3_HI:
		return GBAAudioReadWaveRAM(&gba->audio, 3) >> 16;

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
	case REG_GREENSWP:
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
	case REG_SOUNDBIAS:
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
	case 0x066:
	case 0x06A:
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
	// These registers sit outside of the normal I/O block, so we need to stash them somewhere unused
	case REG_EXWAITCNT_LO:
	case REG_EXWAITCNT_HI:
		address += REG_INTERNAL_EXWAITCNT_LO - REG_EXWAITCNT_LO;
		break;
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
	for (i = 0; i < REG_INTERNAL_MAX; i += 2) {
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

	STORE_32(gba->memory.dmaTransferRegister, 0, &state->dmaTransferRegister);
	STORE_32(gba->dmaPC, 0, &state->dmaBlockPC);

	GBAHardwareSerialize(&gba->memory.hw, state);
}

void GBAIODeserialize(struct GBA* gba, const struct GBASerializedState* state) {
	LOAD_16(gba->memory.io[REG_SOUNDCNT_X >> 1], REG_SOUNDCNT_X, state->io);
	GBAAudioWriteSOUNDCNT_X(&gba->audio, gba->memory.io[REG_SOUNDCNT_X >> 1]);

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
	if (state->versionMagic >= 0x01000006) {
		GBAIOWrite(gba, REG_EXWAITCNT_HI, gba->memory.io[REG_INTERNAL_EXWAITCNT_HI >> 1]);
	}

	uint32_t when;
	for (i = 0; i < 4; ++i) {
		LOAD_16(gba->timers[i].reload, 0, &state->timers[i].reload);
		LOAD_32(gba->timers[i].flags, 0, &state->timers[i].flags);
		LOAD_32(when, 0, &state->timers[i].lastEvent);
		gba->timers[i].lastEvent = when + mTimingCurrentTime(&gba->timing);
		LOAD_32(when, 0, &state->timers[i].nextEvent);
		if ((i < 1 || !GBATimerFlagsIsCountUp(gba->timers[i].flags)) && GBATimerFlagsIsEnable(gba->timers[i].flags)) {
			mTimingSchedule(&gba->timing, &gba->timers[i].event, when);
		} else {
			gba->timers[i].event.when = when + mTimingCurrentTime(&gba->timing);
		}

		LOAD_16(gba->memory.dma[i].reg, (REG_DMA0CNT_HI + i * 12), state->io);
		LOAD_32(gba->memory.dma[i].nextSource, 0, &state->dma[i].nextSource);
		LOAD_32(gba->memory.dma[i].nextDest, 0, &state->dma[i].nextDest);
		LOAD_32(gba->memory.dma[i].nextCount, 0, &state->dma[i].nextCount);
		LOAD_32(gba->memory.dma[i].when, 0, &state->dma[i].when);
	}
	gba->sio.siocnt = gba->memory.io[REG_SIOCNT >> 1];
	GBASIOWriteRCNT(&gba->sio, gba->memory.io[REG_RCNT >> 1]);

	LOAD_32(gba->memory.dmaTransferRegister, 0, &state->dmaTransferRegister);
	LOAD_32(gba->dmaPC, 0, &state->dmaBlockPC);

	GBADMAUpdate(gba);
	GBAHardwareDeserialize(&gba->memory.hw, state);
}
