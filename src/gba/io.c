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
	[GBA_REG(DISPCNT)] = "DISPCNT",
	[GBA_REG(DISPSTAT)] = "DISPSTAT",
	[GBA_REG(VCOUNT)] = "VCOUNT",
	[GBA_REG(BG0CNT)] = "BG0CNT",
	[GBA_REG(BG1CNT)] = "BG1CNT",
	[GBA_REG(BG2CNT)] = "BG2CNT",
	[GBA_REG(BG3CNT)] = "BG3CNT",
	[GBA_REG(BG0HOFS)] = "BG0HOFS",
	[GBA_REG(BG0VOFS)] = "BG0VOFS",
	[GBA_REG(BG1HOFS)] = "BG1HOFS",
	[GBA_REG(BG1VOFS)] = "BG1VOFS",
	[GBA_REG(BG2HOFS)] = "BG2HOFS",
	[GBA_REG(BG2VOFS)] = "BG2VOFS",
	[GBA_REG(BG3HOFS)] = "BG3HOFS",
	[GBA_REG(BG3VOFS)] = "BG3VOFS",
	[GBA_REG(BG2PA)] = "BG2PA",
	[GBA_REG(BG2PB)] = "BG2PB",
	[GBA_REG(BG2PC)] = "BG2PC",
	[GBA_REG(BG2PD)] = "BG2PD",
	[GBA_REG(BG2X_LO)] = "BG2X_LO",
	[GBA_REG(BG2X_HI)] = "BG2X_HI",
	[GBA_REG(BG2Y_LO)] = "BG2Y_LO",
	[GBA_REG(BG2Y_HI)] = "BG2Y_HI",
	[GBA_REG(BG3PA)] = "BG3PA",
	[GBA_REG(BG3PB)] = "BG3PB",
	[GBA_REG(BG3PC)] = "BG3PC",
	[GBA_REG(BG3PD)] = "BG3PD",
	[GBA_REG(BG3X_LO)] = "BG3X_LO",
	[GBA_REG(BG3X_HI)] = "BG3X_HI",
	[GBA_REG(BG3Y_LO)] = "BG3Y_LO",
	[GBA_REG(BG3Y_HI)] = "BG3Y_HI",
	[GBA_REG(WIN0H)] = "WIN0H",
	[GBA_REG(WIN1H)] = "WIN1H",
	[GBA_REG(WIN0V)] = "WIN0V",
	[GBA_REG(WIN1V)] = "WIN1V",
	[GBA_REG(WININ)] = "WININ",
	[GBA_REG(WINOUT)] = "WINOUT",
	[GBA_REG(MOSAIC)] = "MOSAIC",
	[GBA_REG(BLDCNT)] = "BLDCNT",
	[GBA_REG(BLDALPHA)] = "BLDALPHA",
	[GBA_REG(BLDY)] = "BLDY",

	// Sound
	[GBA_REG(SOUND1CNT_LO)] = "SOUND1CNT_LO",
	[GBA_REG(SOUND1CNT_HI)] = "SOUND1CNT_HI",
	[GBA_REG(SOUND1CNT_X)] = "SOUND1CNT_X",
	[GBA_REG(SOUND2CNT_LO)] = "SOUND2CNT_LO",
	[GBA_REG(SOUND2CNT_HI)] = "SOUND2CNT_HI",
	[GBA_REG(SOUND3CNT_LO)] = "SOUND3CNT_LO",
	[GBA_REG(SOUND3CNT_HI)] = "SOUND3CNT_HI",
	[GBA_REG(SOUND3CNT_X)] = "SOUND3CNT_X",
	[GBA_REG(SOUND4CNT_LO)] = "SOUND4CNT_LO",
	[GBA_REG(SOUND4CNT_HI)] = "SOUND4CNT_HI",
	[GBA_REG(SOUNDCNT_LO)] = "SOUNDCNT_LO",
	[GBA_REG(SOUNDCNT_HI)] = "SOUNDCNT_HI",
	[GBA_REG(SOUNDCNT_X)] = "SOUNDCNT_X",
	[GBA_REG(SOUNDBIAS)] = "SOUNDBIAS",
	[GBA_REG(WAVE_RAM0_LO)] = "WAVE_RAM0_LO",
	[GBA_REG(WAVE_RAM0_HI)] = "WAVE_RAM0_HI",
	[GBA_REG(WAVE_RAM1_LO)] = "WAVE_RAM1_LO",
	[GBA_REG(WAVE_RAM1_HI)] = "WAVE_RAM1_HI",
	[GBA_REG(WAVE_RAM2_LO)] = "WAVE_RAM2_LO",
	[GBA_REG(WAVE_RAM2_HI)] = "WAVE_RAM2_HI",
	[GBA_REG(WAVE_RAM3_LO)] = "WAVE_RAM3_LO",
	[GBA_REG(WAVE_RAM3_HI)] = "WAVE_RAM3_HI",
	[GBA_REG(FIFO_A_LO)] = "FIFO_A_LO",
	[GBA_REG(FIFO_A_HI)] = "FIFO_A_HI",
	[GBA_REG(FIFO_B_LO)] = "FIFO_B_LO",
	[GBA_REG(FIFO_B_HI)] = "FIFO_B_HI",

	// DMA
	[GBA_REG(DMA0SAD_LO)] = "DMA0SAD_LO",
	[GBA_REG(DMA0SAD_HI)] = "DMA0SAD_HI",
	[GBA_REG(DMA0DAD_LO)] = "DMA0DAD_LO",
	[GBA_REG(DMA0DAD_HI)] = "DMA0DAD_HI",
	[GBA_REG(DMA0CNT_LO)] = "DMA0CNT_LO",
	[GBA_REG(DMA0CNT_HI)] = "DMA0CNT_HI",
	[GBA_REG(DMA1SAD_LO)] = "DMA1SAD_LO",
	[GBA_REG(DMA1SAD_HI)] = "DMA1SAD_HI",
	[GBA_REG(DMA1DAD_LO)] = "DMA1DAD_LO",
	[GBA_REG(DMA1DAD_HI)] = "DMA1DAD_HI",
	[GBA_REG(DMA1CNT_LO)] = "DMA1CNT_LO",
	[GBA_REG(DMA1CNT_HI)] = "DMA1CNT_HI",
	[GBA_REG(DMA2SAD_LO)] = "DMA2SAD_LO",
	[GBA_REG(DMA2SAD_HI)] = "DMA2SAD_HI",
	[GBA_REG(DMA2DAD_LO)] = "DMA2DAD_LO",
	[GBA_REG(DMA2DAD_HI)] = "DMA2DAD_HI",
	[GBA_REG(DMA2CNT_LO)] = "DMA2CNT_LO",
	[GBA_REG(DMA2CNT_HI)] = "DMA2CNT_HI",
	[GBA_REG(DMA3SAD_LO)] = "DMA3SAD_LO",
	[GBA_REG(DMA3SAD_HI)] = "DMA3SAD_HI",
	[GBA_REG(DMA3DAD_LO)] = "DMA3DAD_LO",
	[GBA_REG(DMA3DAD_HI)] = "DMA3DAD_HI",
	[GBA_REG(DMA3CNT_LO)] = "DMA3CNT_LO",
	[GBA_REG(DMA3CNT_HI)] = "DMA3CNT_HI",

	// Timers
	[GBA_REG(TM0CNT_LO)] = "TM0CNT_LO",
	[GBA_REG(TM0CNT_HI)] = "TM0CNT_HI",
	[GBA_REG(TM1CNT_LO)] = "TM1CNT_LO",
	[GBA_REG(TM1CNT_HI)] = "TM1CNT_HI",
	[GBA_REG(TM2CNT_LO)] = "TM2CNT_LO",
	[GBA_REG(TM2CNT_HI)] = "TM2CNT_HI",
	[GBA_REG(TM3CNT_LO)] = "TM3CNT_LO",
	[GBA_REG(TM3CNT_HI)] = "TM3CNT_HI",

	// SIO
	[GBA_REG(SIOMULTI0)] = "SIOMULTI0",
	[GBA_REG(SIOMULTI1)] = "SIOMULTI1",
	[GBA_REG(SIOMULTI2)] = "SIOMULTI2",
	[GBA_REG(SIOMULTI3)] = "SIOMULTI3",
	[GBA_REG(SIOCNT)] = "SIOCNT",
	[GBA_REG(SIOMLT_SEND)] = "SIOMLT_SEND",
	[GBA_REG(KEYINPUT)] = "KEYINPUT",
	[GBA_REG(KEYCNT)] = "KEYCNT",
	[GBA_REG(RCNT)] = "RCNT",
	[GBA_REG(JOYCNT)] = "JOYCNT",
	[GBA_REG(JOY_RECV_LO)] = "JOY_RECV_LO",
	[GBA_REG(JOY_RECV_HI)] = "JOY_RECV_HI",
	[GBA_REG(JOY_TRANS_LO)] = "JOY_TRANS_LO",
	[GBA_REG(JOY_TRANS_HI)] = "JOY_TRANS_HI",
	[GBA_REG(JOYSTAT)] = "JOYSTAT",

	// Interrupts, etc
	[GBA_REG(IE)] = "IE",
	[GBA_REG(IF)] = "IF",
	[GBA_REG(WAITCNT)] = "WAITCNT",
	[GBA_REG(IME)] = "IME",
};

static const int _isValidRegister[GBA_REG(INTERNAL_MAX)] = {
	/*      0  2  4  6  8  A  C  E */
	/*    Video */
	/* 00 */ 1, 0, 1, 1, 1, 1, 1, 1,
	/* 01 */ 1, 1, 1, 1, 1, 1, 1, 1,
	/* 02 */ 1, 1, 1, 1, 1, 1, 1, 1,
	/* 03 */ 1, 1, 1, 1, 1, 1, 1, 1,
	/* 04 */ 1, 1, 1, 1, 1, 1, 1, 0,
	/* 05 */ 1, 1, 1, 0, 0, 0, 0, 0,
	/*    Audio */
	/* 06 */ 1, 1, 1, 0, 1, 0, 1, 0,
	/* 07 */ 1, 1, 1, 0, 1, 0, 1, 0,
	/* 08 */ 1, 1, 1, 0, 1, 0, 0, 0,
	/* 09 */ 1, 1, 1, 1, 1, 1, 1, 1,
	/* 0A */ 1, 1, 1, 1, 0, 0, 0, 0,
	/*    DMA */
	/* 0B */ 1, 1, 1, 1, 1, 1, 1, 1,
	/* 0C */ 1, 1, 1, 1, 1, 1, 1, 1,
	/* 0D */ 1, 1, 1, 1, 1, 1, 1, 1,
	/* 0E */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 0F */ 0, 0, 0, 0, 0, 0, 0, 0,
	/*   Timers */
	/* 10 */ 1, 1, 1, 1, 1, 1, 1, 1,
	/* 11 */ 0, 0, 0, 0, 0, 0, 0, 0,
	/*    SIO */
	/* 12 */ 1, 1, 1, 1, 1, 0, 0, 0,
	/* 13 */ 1, 1, 1, 0, 0, 0, 0, 0,
	/* 14 */ 1, 0, 0, 0, 0, 0, 0, 0,
	/* 15 */ 1, 1, 1, 1, 1, 0, 0, 0,
	/* 16 */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 17 */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 18 */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 19 */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 1A */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 1B */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 1C */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 1D */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 1E */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 1F */ 0, 0, 0, 0, 0, 0, 0, 0,
	/*    Interrupts */
	/* 20 */ 1, 1, 1, 0, 1, 0, 0, 0,
	// Internal registers
	1, 1
};

static const int _isRSpecialRegister[GBA_REG(INTERNAL_MAX)] = {
	/*      0  2  4  6  8  A  C  E */
	/*    Video */
	/* 00 */ 0, 0, 1, 1, 0, 0, 0, 0,
	/* 01 */ 1, 1, 1, 1, 1, 1, 1, 1,
	/* 02 */ 1, 1, 1, 1, 1, 1, 1, 1,
	/* 03 */ 1, 1, 1, 1, 1, 1, 1, 1,
	/* 04 */ 1, 1, 1, 1, 1, 1, 1, 1,
	/* 05 */ 1, 1, 1, 1, 1, 1, 1, 1,
	/*    Audio */
	/* 06 */ 0, 0, 1, 0, 0, 0, 1, 0,
	/* 07 */ 0, 0, 1, 0, 0, 0, 1, 0,
	/* 08 */ 0, 0, 0, 0, 1, 0, 0, 0,
	/* 09 */ 1, 1, 1, 1, 1, 1, 1, 1,
	/* 0A */ 1, 1, 1, 1, 0, 0, 0, 0,
	/*    DMA */
	/* 0B */ 1, 1, 1, 1, 1, 1, 1, 1,
	/* 0C */ 1, 1, 1, 1, 1, 1, 1, 1,
	/* 0D */ 1, 1, 1, 1, 1, 1, 1, 1,
	/* 0E */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 0F */ 0, 0, 0, 0, 0, 0, 0, 0,
	/*    Timers */
	/* 10 */ 1, 1, 1, 1, 1, 1, 1, 1,
	/* 11 */ 0, 0, 0, 0, 0, 0, 0, 0,
	/*    SIO */
	/* 12 */ 1, 1, 1, 1, 0, 0, 0, 0,
	/* 13 */ 1, 1, 0, 0, 0, 0, 0, 0,
	/* 14 */ 1, 0, 0, 0, 0, 0, 0, 0,
	/* 15 */ 1, 1, 1, 1, 1, 0, 0, 0,
	/* 16 */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 17 */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 18 */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 19 */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 1A */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 1B */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 1C */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 1D */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 1E */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 1F */ 0, 0, 0, 0, 0, 0, 0, 0,
	/*    Interrupts */
	/* 20 */ 0, 0, 0, 0, 0, 0, 0, 0,
	// Internal registers
	1, 1
};

static const int _isWSpecialRegister[GBA_REG(INTERNAL_MAX)] = {
	/*      0  2  4  6  8  A  C  E */
	/*    Video */
	/* 00 */ 0, 0, 1, 1, 0, 0, 0, 0,
	/* 01 */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 02 */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 03 */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 04 */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 05 */ 0, 0, 0, 0, 0, 0, 0, 0,
	/*    Audio */
	/* 06 */ 0, 0, 1, 0, 0, 0, 1, 0,
	/* 07 */ 0, 0, 1, 0, 0, 0, 1, 0,
	/* 08 */ 0, 0, 1, 0, 0, 0, 0, 0,
	/* 09 */ 1, 1, 1, 1, 1, 1, 1, 1,
	/* 0A */ 1, 1, 1, 1, 0, 0, 0, 0,
	/*    DMA */
	/* 0B */ 0, 0, 0, 0, 0, 1, 0, 0,
	/* 0C */ 0, 0, 0, 1, 0, 0, 0, 0,
	/* 0D */ 0, 1, 0, 0, 0, 0, 0, 1,
	/* 0E */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 0F */ 0, 0, 0, 0, 0, 0, 0, 0,
	/*    Timers */
	/* 10 */ 1, 1, 1, 1, 1, 1, 1, 1,
	/* 11 */ 0, 0, 0, 0, 0, 0, 0, 0,
	/*    SIO */
	/* 12 */ 1, 1, 1, 1, 1, 0, 0, 0,
	/* 13 */ 1, 1, 1, 0, 0, 0, 0, 0,
	/* 14 */ 1, 0, 0, 0, 0, 0, 0, 0,
	/* 15 */ 1, 1, 1, 1, 1, 0, 0, 0,
	/* 16 */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 17 */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 18 */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 19 */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 1A */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 1B */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 1C */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 1D */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 1E */ 0, 0, 0, 0, 0, 0, 0, 0,
	/* 1F */ 0, 0, 0, 0, 0, 0, 0, 0,
	/*    Interrupts */
	/* 20 */ 1, 1, 0, 0, 1, 0, 0, 0,
	// Internal registers
	1, 1
};

void GBAIOInit(struct GBA* gba) {
	gba->memory.io[GBA_REG(DISPCNT)] = 0x0080;
	gba->memory.io[GBA_REG(RCNT)] = RCNT_INITIAL;
	gba->memory.io[GBA_REG(KEYINPUT)] = 0x3FF;
	gba->memory.io[GBA_REG(SOUNDBIAS)] = 0x200;
	gba->memory.io[GBA_REG(BG2PA)] = 0x100;
	gba->memory.io[GBA_REG(BG2PD)] = 0x100;
	gba->memory.io[GBA_REG(BG3PA)] = 0x100;
	gba->memory.io[GBA_REG(BG3PD)] = 0x100;
	gba->memory.io[GBA_REG(INTERNAL_EXWAITCNT_LO)] = 0x20;
	gba->memory.io[GBA_REG(INTERNAL_EXWAITCNT_HI)] = 0xD00;

	if (!gba->biosVf) {
		gba->memory.io[GBA_REG(VCOUNT)] = 0x7E;
		gba->memory.io[GBA_REG(POSTFLG)] = 1;
	}
}

void GBAIOWrite(struct GBA* gba, uint32_t address, uint16_t value) {
	if (address < GBA_REG_SOUND1CNT_LO && (address > GBA_REG_VCOUNT || address < GBA_REG_DISPSTAT)) {
		gba->memory.io[address >> 1] = gba->video.renderer->writeVideoRegister(gba->video.renderer, address, value);
		return;
	}

	if (address >= GBA_REG_SOUND1CNT_LO && address <= GBA_REG_SOUNDCNT_LO && !gba->audio.enable) {
		// Ignore writes to most audio registers if the hardware is off.
		return;
	}

	switch (address) {
	// Video
	case GBA_REG_DISPSTAT:
		value = GBAVideoWriteDISPSTAT(&gba->video, value);
		break;

	case GBA_REG_VCOUNT:
		mLOG(GBA_IO, GAME_ERROR, "Write to read-only I/O register: %03X", address);
		return;

	// Audio
	case GBA_REG_SOUND1CNT_LO:
		GBAAudioWriteSOUND1CNT_LO(&gba->audio, value);
		value &= 0x007F;
		break;
	case GBA_REG_SOUND1CNT_HI:
		GBAAudioWriteSOUND1CNT_HI(&gba->audio, value);
		value &= 0xFFC0;
		break;
	case GBA_REG_SOUND1CNT_X:
		GBAAudioWriteSOUND1CNT_X(&gba->audio, value);
		value &= 0x4000;
		break;
	case GBA_REG_SOUND2CNT_LO:
		GBAAudioWriteSOUND2CNT_LO(&gba->audio, value);
		value &= 0xFFC0;
		break;
	case GBA_REG_SOUND2CNT_HI:
		GBAAudioWriteSOUND2CNT_HI(&gba->audio, value);
		value &= 0x4000;
		break;
	case GBA_REG_SOUND3CNT_LO:
		GBAAudioWriteSOUND3CNT_LO(&gba->audio, value);
		value &= 0x00E0;
		break;
	case GBA_REG_SOUND3CNT_HI:
		GBAAudioWriteSOUND3CNT_HI(&gba->audio, value);
		value &= 0xE000;
		break;
	case GBA_REG_SOUND3CNT_X:
		GBAAudioWriteSOUND3CNT_X(&gba->audio, value);
		value &= 0x4000;
		break;
	case GBA_REG_SOUND4CNT_LO:
		GBAAudioWriteSOUND4CNT_LO(&gba->audio, value);
		value &= 0xFF00;
		break;
	case GBA_REG_SOUND4CNT_HI:
		GBAAudioWriteSOUND4CNT_HI(&gba->audio, value);
		value &= 0x40FF;
		break;
	case GBA_REG_SOUNDCNT_LO:
		GBAAudioWriteSOUNDCNT_LO(&gba->audio, value);
		value &= 0xFF77;
		break;
	case GBA_REG_SOUNDCNT_HI:
		GBAAudioWriteSOUNDCNT_HI(&gba->audio, value);
		value &= 0x770F;
		break;
	case GBA_REG_SOUNDCNT_X:
		GBAAudioWriteSOUNDCNT_X(&gba->audio, value);
		value &= 0x0080;
		value |= gba->memory.io[GBA_REG(SOUNDCNT_X)] & 0xF;
		break;
	case GBA_REG_SOUNDBIAS:
		value &= 0xC3FE;
		GBAAudioWriteSOUNDBIAS(&gba->audio, value);
		break;

	case GBA_REG_WAVE_RAM0_LO:
	case GBA_REG_WAVE_RAM1_LO:
	case GBA_REG_WAVE_RAM2_LO:
	case GBA_REG_WAVE_RAM3_LO:
		GBAIOWrite32(gba, address, (gba->memory.io[(address >> 1) + 1] << 16) | value);
		break;

	case GBA_REG_WAVE_RAM0_HI:
	case GBA_REG_WAVE_RAM1_HI:
	case GBA_REG_WAVE_RAM2_HI:
	case GBA_REG_WAVE_RAM3_HI:
		GBAIOWrite32(gba, address - 2, gba->memory.io[(address >> 1) - 1] | (value << 16));
		break;

	case GBA_REG_FIFO_A_LO:
	case GBA_REG_FIFO_B_LO:
		GBAIOWrite32(gba, address, (gba->memory.io[(address >> 1) + 1] << 16) | value);
		return;

	case GBA_REG_FIFO_A_HI:
	case GBA_REG_FIFO_B_HI:
		GBAIOWrite32(gba, address - 2, gba->memory.io[(address >> 1) - 1] | (value << 16));
		return;

	// DMA
	case GBA_REG_DMA0SAD_LO:
	case GBA_REG_DMA0DAD_LO:
	case GBA_REG_DMA1SAD_LO:
	case GBA_REG_DMA1DAD_LO:
	case GBA_REG_DMA2SAD_LO:
	case GBA_REG_DMA2DAD_LO:
	case GBA_REG_DMA3SAD_LO:
	case GBA_REG_DMA3DAD_LO:
		GBAIOWrite32(gba, address, (gba->memory.io[(address >> 1) + 1] << 16) | value);
		break;

	case GBA_REG_DMA0SAD_HI:
	case GBA_REG_DMA0DAD_HI:
	case GBA_REG_DMA1SAD_HI:
	case GBA_REG_DMA1DAD_HI:
	case GBA_REG_DMA2SAD_HI:
	case GBA_REG_DMA2DAD_HI:
	case GBA_REG_DMA3SAD_HI:
	case GBA_REG_DMA3DAD_HI:
		GBAIOWrite32(gba, address - 2, gba->memory.io[(address >> 1) - 1] | (value << 16));
		break;

	case GBA_REG_DMA0CNT_LO:
		GBADMAWriteCNT_LO(gba, 0, value & 0x3FFF);
		break;
	case GBA_REG_DMA0CNT_HI:
		value = GBADMAWriteCNT_HI(gba, 0, value);
		break;
	case GBA_REG_DMA1CNT_LO:
		GBADMAWriteCNT_LO(gba, 1, value & 0x3FFF);
		break;
	case GBA_REG_DMA1CNT_HI:
		value = GBADMAWriteCNT_HI(gba, 1, value);
		break;
	case GBA_REG_DMA2CNT_LO:
		GBADMAWriteCNT_LO(gba, 2, value & 0x3FFF);
		break;
	case GBA_REG_DMA2CNT_HI:
		value = GBADMAWriteCNT_HI(gba, 2, value);
		break;
	case GBA_REG_DMA3CNT_LO:
		GBADMAWriteCNT_LO(gba, 3, value);
		break;
	case GBA_REG_DMA3CNT_HI:
		value = GBADMAWriteCNT_HI(gba, 3, value);
		break;

	// Timers
	case GBA_REG_TM0CNT_LO:
		GBATimerWriteTMCNT_LO(gba, 0, value);
		return;
	case GBA_REG_TM1CNT_LO:
		GBATimerWriteTMCNT_LO(gba, 1, value);
		return;
	case GBA_REG_TM2CNT_LO:
		GBATimerWriteTMCNT_LO(gba, 2, value);
		return;
	case GBA_REG_TM3CNT_LO:
		GBATimerWriteTMCNT_LO(gba, 3, value);
		return;

	case GBA_REG_TM0CNT_HI:
		value &= 0x00C7;
		GBATimerWriteTMCNT_HI(gba, 0, value);
		break;
	case GBA_REG_TM1CNT_HI:
		value &= 0x00C7;
		GBATimerWriteTMCNT_HI(gba, 1, value);
		break;
	case GBA_REG_TM2CNT_HI:
		value &= 0x00C7;
		GBATimerWriteTMCNT_HI(gba, 2, value);
		break;
	case GBA_REG_TM3CNT_HI:
		value &= 0x00C7;
		GBATimerWriteTMCNT_HI(gba, 3, value);
		break;

	// SIO
	case GBA_REG_SIOCNT:
		value &= 0x7FFF;
		GBASIOWriteSIOCNT(&gba->sio, value);
		break;
	case GBA_REG_RCNT:
		value &= 0xC1FF;
		GBASIOWriteRCNT(&gba->sio, value);
		break;
	case GBA_REG_JOY_TRANS_LO:
	case GBA_REG_JOY_TRANS_HI:
		gba->memory.io[GBA_REG(JOYSTAT)] |= JOYSTAT_TRANS;
		// Fall through
	case GBA_REG_SIODATA32_LO:
	case GBA_REG_SIODATA32_HI:
	case GBA_REG_SIOMLT_SEND:
	case GBA_REG_JOYCNT:
	case GBA_REG_JOYSTAT:
	case GBA_REG_JOY_RECV_LO:
	case GBA_REG_JOY_RECV_HI:
		value = GBASIOWriteRegister(&gba->sio, address, value);
		break;

	// Interrupts and misc
	case GBA_REG_KEYCNT:
		value &= 0xC3FF;
		if (gba->keysLast < 0x400) {
			gba->keysLast &= gba->memory.io[address >> 1] | ~value;
		}
		gba->memory.io[address >> 1] = value;
		GBATestKeypadIRQ(gba);
		return;
	case GBA_REG_WAITCNT:
		value &= 0x5FFF;
		GBAAdjustWaitstates(gba, value);
		break;
	case GBA_REG_IE:
		gba->memory.io[GBA_REG(IE)] = value;
		GBATestIRQ(gba, 1);
		return;
	case GBA_REG_IF:
		value = gba->memory.io[GBA_REG(IF)] & ~value;
		gba->memory.io[GBA_REG(IF)] = value;
		GBATestIRQ(gba, 1);
		return;
	case GBA_REG_IME:
		gba->memory.io[GBA_REG(IME)] = value & 1;
		GBATestIRQ(gba, 1);
		return;
	case GBA_REG_MAX:
		// Some bad interrupt libraries will write to this
		break;
	case GBA_REG_POSTFLG:
		if (gba->memory.activeRegion == GBA_REGION_BIOS) {
			if (gba->memory.io[address >> 1]) {
				if (value & 0x8000) {
					GBAStop(gba);
				} else {
					GBAHalt(gba);
				}
			}
			value &= ~0x8000;
		} else {
			mLOG(GBA_IO, GAME_ERROR, "Write to BIOS-only I/O register: %03X", address);
			return;
		}
		break;
	case GBA_REG_EXWAITCNT_HI:
		// This register sits outside of the normal I/O block, so we need to stash it somewhere unused
		address = GBA_REG_INTERNAL_EXWAITCNT_HI;
		value &= 0xFF00;
		GBAAdjustEWRAMWaitstates(gba, value);
		break;
	case GBA_REG_DEBUG_ENABLE:
		gba->debug = value == 0xC0DE;
		return;
	case GBA_REG_DEBUG_FLAGS:
		if (gba->debug) {
			GBADebug(gba, value);

			return;
		}
		// Fall through
	default:
		if (address >= GBA_REG_DEBUG_STRING && address - GBA_REG_DEBUG_STRING < sizeof(gba->debugString)) {
			STORE_16LE(value, address - GBA_REG_DEBUG_STRING, gba->debugString);
			return;
		}
		mLOG(GBA_IO, STUB, "Stub I/O register write: %03X", address);
		if (address >= GBA_REG_MAX) {
			mLOG(GBA_IO, GAME_ERROR, "Write to unused I/O register: %03X", address);
			return;
		}
		break;
	}
	gba->memory.io[address >> 1] = value;
}

void GBAIOWrite8(struct GBA* gba, uint32_t address, uint8_t value) {
	if (address >= GBA_REG_DEBUG_STRING && address - GBA_REG_DEBUG_STRING < sizeof(gba->debugString)) {
		gba->debugString[address - GBA_REG_DEBUG_STRING] = value;
		return;
	}
	if (address > GBA_SIZE_IO) {
		return;
	}
	uint16_t value16;

	switch (address) {
	case GBA_REG_SOUND1CNT_HI:
		GBAAudioSample(&gba->audio, mTimingCurrentTime(&gba->timing));
		GBAudioWriteNR11(&gba->audio.psg, value);
		gba->memory.io[GBA_REG(SOUND1CNT_HI)] &= 0xFF00;
		gba->memory.io[GBA_REG(SOUND1CNT_HI)] |= value & 0xC0;
		break;
	case GBA_REG_SOUND1CNT_HI + 1:
		GBAAudioSample(&gba->audio, mTimingCurrentTime(&gba->timing));
		GBAudioWriteNR12(&gba->audio.psg, value);
		gba->memory.io[GBA_REG(SOUND1CNT_HI)] &= 0x00C0;
		gba->memory.io[GBA_REG(SOUND1CNT_HI)] |= value << 8;
		break;
	case GBA_REG_SOUND1CNT_X:
		GBAAudioSample(&gba->audio, mTimingCurrentTime(&gba->timing));
		GBAudioWriteNR13(&gba->audio.psg, value);
		break;
	case GBA_REG_SOUND1CNT_X + 1:
		GBAAudioSample(&gba->audio, mTimingCurrentTime(&gba->timing));
		GBAudioWriteNR14(&gba->audio.psg, value);
		gba->memory.io[GBA_REG(SOUND1CNT_X)] = (value & 0x40) << 8;
		break;
	case GBA_REG_SOUND2CNT_LO:
		GBAAudioSample(&gba->audio, mTimingCurrentTime(&gba->timing));
		GBAudioWriteNR21(&gba->audio.psg, value);
		gba->memory.io[GBA_REG(SOUND2CNT_LO)] &= 0xFF00;
		gba->memory.io[GBA_REG(SOUND2CNT_LO)] |= value & 0xC0;
		break;
	case GBA_REG_SOUND2CNT_LO + 1:
		GBAAudioSample(&gba->audio, mTimingCurrentTime(&gba->timing));
		GBAudioWriteNR22(&gba->audio.psg, value);
		gba->memory.io[GBA_REG(SOUND2CNT_LO)] &= 0x00C0;
		gba->memory.io[GBA_REG(SOUND2CNT_LO)] |= value << 8;
		break;
	case GBA_REG_SOUND2CNT_HI:
		GBAAudioSample(&gba->audio, mTimingCurrentTime(&gba->timing));
		GBAudioWriteNR23(&gba->audio.psg, value);
		break;
	case GBA_REG_SOUND2CNT_HI + 1:
		GBAAudioSample(&gba->audio, mTimingCurrentTime(&gba->timing));
		GBAudioWriteNR24(&gba->audio.psg, value);
		gba->memory.io[GBA_REG(SOUND2CNT_HI)] = (value & 0x40) << 8;
		break;
	case GBA_REG_SOUND3CNT_HI:
		GBAAudioSample(&gba->audio, mTimingCurrentTime(&gba->timing));
		GBAudioWriteNR31(&gba->audio.psg, value);
		break;
	case GBA_REG_SOUND3CNT_HI + 1:
		GBAAudioSample(&gba->audio, mTimingCurrentTime(&gba->timing));
		gba->audio.psg.ch3.volume = GBAudioRegisterBankVolumeGetVolumeGBA(value);
		gba->memory.io[GBA_REG(SOUND3CNT_HI)] = (value & 0xE0) << 8;
		break;
	case GBA_REG_SOUND3CNT_X:
		GBAAudioSample(&gba->audio, mTimingCurrentTime(&gba->timing));
		GBAudioWriteNR33(&gba->audio.psg, value);
		break;
	case GBA_REG_SOUND3CNT_X + 1:
		GBAAudioSample(&gba->audio, mTimingCurrentTime(&gba->timing));
		GBAudioWriteNR34(&gba->audio.psg, value);
		gba->memory.io[GBA_REG(SOUND3CNT_X)] = (value & 0x40) << 8;
		break;
	case GBA_REG_SOUND4CNT_LO:
		GBAAudioSample(&gba->audio, mTimingCurrentTime(&gba->timing));
		GBAudioWriteNR41(&gba->audio.psg, value);
		break;
	case GBA_REG_SOUND4CNT_LO + 1:
		GBAAudioSample(&gba->audio, mTimingCurrentTime(&gba->timing));
		GBAudioWriteNR42(&gba->audio.psg, value);
		gba->memory.io[GBA_REG(SOUND4CNT_LO)] = value << 8;
		break;
	case GBA_REG_SOUND4CNT_HI:
		GBAAudioSample(&gba->audio, mTimingCurrentTime(&gba->timing));
		GBAudioWriteNR43(&gba->audio.psg, value);
		gba->memory.io[GBA_REG(SOUND4CNT_HI)] &= 0x4000;
		gba->memory.io[GBA_REG(SOUND4CNT_HI)] |= value;
		break;
	case GBA_REG_SOUND4CNT_HI + 1:
		GBAAudioSample(&gba->audio, mTimingCurrentTime(&gba->timing));
		GBAudioWriteNR44(&gba->audio.psg, value);
		gba->memory.io[GBA_REG(SOUND4CNT_HI)] &= 0x00FF;
		gba->memory.io[GBA_REG(SOUND4CNT_HI)] |= (value & 0x40) << 8;
		break;
	default:
		value16 = value << (8 * (address & 1));
		value16 |= (gba->memory.io[(address & (GBA_SIZE_IO - 1)) >> 1]) & ~(0xFF << (8 * (address & 1)));
		GBAIOWrite(gba, address & 0xFFFFFFFE, value16);
		break;
	}
}

void GBAIOWrite32(struct GBA* gba, uint32_t address, uint32_t value) {
	switch (address) {
	// Wave RAM can be written and read even if the audio hardware is disabled.
	// However, it is not possible to switch between the two banks because it
	// isn't possible to write to register SOUND3CNT_LO.
	case GBA_REG_WAVE_RAM0_LO:
		GBAAudioWriteWaveRAM(&gba->audio, 0, value);
		break;
	case GBA_REG_WAVE_RAM1_LO:
		GBAAudioWriteWaveRAM(&gba->audio, 1, value);
		break;
	case GBA_REG_WAVE_RAM2_LO:
		GBAAudioWriteWaveRAM(&gba->audio, 2, value);
		break;
	case GBA_REG_WAVE_RAM3_LO:
		GBAAudioWriteWaveRAM(&gba->audio, 3, value);
		break;
	case GBA_REG_FIFO_A_LO:
	case GBA_REG_FIFO_B_LO:
		value = GBAAudioWriteFIFO(&gba->audio, address, value);
		break;
	case GBA_REG_DMA0SAD_LO:
		value = GBADMAWriteSAD(gba, 0, value);
		break;
	case GBA_REG_DMA0DAD_LO:
		value = GBADMAWriteDAD(gba, 0, value);
		break;
	case GBA_REG_DMA1SAD_LO:
		value = GBADMAWriteSAD(gba, 1, value);
		break;
	case GBA_REG_DMA1DAD_LO:
		value = GBADMAWriteDAD(gba, 1, value);
		break;
	case GBA_REG_DMA2SAD_LO:
		value = GBADMAWriteSAD(gba, 2, value);
		break;
	case GBA_REG_DMA2DAD_LO:
		value = GBADMAWriteDAD(gba, 2, value);
		break;
	case GBA_REG_DMA3SAD_LO:
		value = GBADMAWriteSAD(gba, 3, value);
		break;
	case GBA_REG_DMA3DAD_LO:
		value = GBADMAWriteDAD(gba, 3, value);
		break;
	default:
		if (address >= GBA_REG_DEBUG_STRING && address - GBA_REG_DEBUG_STRING < sizeof(gba->debugString)) {
			STORE_32LE(value, address - GBA_REG_DEBUG_STRING, gba->debugString);
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
	case GBA_REG_BG0CNT:
	case GBA_REG_BG1CNT:
	case GBA_REG_BG2CNT:
	case GBA_REG_BG3CNT:
	case GBA_REG_WININ:
	case GBA_REG_WINOUT:
	case GBA_REG_BLDCNT:
	case GBA_REG_BLDALPHA:
	case GBA_REG_SOUND1CNT_LO:
	case GBA_REG_SOUND1CNT_HI:
	case GBA_REG_SOUND1CNT_X:
	case GBA_REG_SOUND2CNT_LO:
	case GBA_REG_SOUND2CNT_HI:
	case GBA_REG_SOUND3CNT_LO:
	case GBA_REG_SOUND3CNT_HI:
	case GBA_REG_SOUND3CNT_X:
	case GBA_REG_SOUND4CNT_LO:
	case GBA_REG_SOUND4CNT_HI:
	case GBA_REG_SOUNDCNT_LO:
	case GBA_REG_SOUNDCNT_HI:
	case GBA_REG_TM0CNT_HI:
	case GBA_REG_TM1CNT_HI:
	case GBA_REG_TM2CNT_HI:
	case GBA_REG_TM3CNT_HI:
	case GBA_REG_KEYINPUT:
	case GBA_REG_KEYCNT:
	case GBA_REG_IE:
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
	case GBA_REG_TM0CNT_LO:
		GBATimerUpdateRegister(gba, 0, 2);
		break;
	case GBA_REG_TM1CNT_LO:
		GBATimerUpdateRegister(gba, 1, 2);
		break;
	case GBA_REG_TM2CNT_LO:
		GBATimerUpdateRegister(gba, 2, 2);
		break;
	case GBA_REG_TM3CNT_LO:
		GBATimerUpdateRegister(gba, 3, 2);
		break;

	case GBA_REG_KEYINPUT: {
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
	case GBA_REG_SIOCNT:
		return gba->sio.siocnt;
	case GBA_REG_RCNT:
		return gba->sio.rcnt;

	case GBA_REG_BG0HOFS:
	case GBA_REG_BG0VOFS:
	case GBA_REG_BG1HOFS:
	case GBA_REG_BG1VOFS:
	case GBA_REG_BG2HOFS:
	case GBA_REG_BG2VOFS:
	case GBA_REG_BG3HOFS:
	case GBA_REG_BG3VOFS:
	case GBA_REG_BG2PA:
	case GBA_REG_BG2PB:
	case GBA_REG_BG2PC:
	case GBA_REG_BG2PD:
	case GBA_REG_BG2X_LO:
	case GBA_REG_BG2X_HI:
	case GBA_REG_BG2Y_LO:
	case GBA_REG_BG2Y_HI:
	case GBA_REG_BG3PA:
	case GBA_REG_BG3PB:
	case GBA_REG_BG3PC:
	case GBA_REG_BG3PD:
	case GBA_REG_BG3X_LO:
	case GBA_REG_BG3X_HI:
	case GBA_REG_BG3Y_LO:
	case GBA_REG_BG3Y_HI:
	case GBA_REG_WIN0H:
	case GBA_REG_WIN1H:
	case GBA_REG_WIN0V:
	case GBA_REG_WIN1V:
	case GBA_REG_MOSAIC:
	case GBA_REG_BLDY:
	case GBA_REG_FIFO_A_LO:
	case GBA_REG_FIFO_A_HI:
	case GBA_REG_FIFO_B_LO:
	case GBA_REG_FIFO_B_HI:
	case GBA_REG_DMA0SAD_LO:
	case GBA_REG_DMA0SAD_HI:
	case GBA_REG_DMA0DAD_LO:
	case GBA_REG_DMA0DAD_HI:
	case GBA_REG_DMA1SAD_LO:
	case GBA_REG_DMA1SAD_HI:
	case GBA_REG_DMA1DAD_LO:
	case GBA_REG_DMA1DAD_HI:
	case GBA_REG_DMA2SAD_LO:
	case GBA_REG_DMA2SAD_HI:
	case GBA_REG_DMA2DAD_LO:
	case GBA_REG_DMA2DAD_HI:
	case GBA_REG_DMA3SAD_LO:
	case GBA_REG_DMA3SAD_HI:
	case GBA_REG_DMA3DAD_LO:
	case GBA_REG_DMA3DAD_HI:
		// Write-only register
		mLOG(GBA_IO, GAME_ERROR, "Read from write-only I/O register: %03X", address);
		return GBALoadBad(gba->cpu);

	case GBA_REG_DMA0CNT_LO:
	case GBA_REG_DMA1CNT_LO:
	case GBA_REG_DMA2CNT_LO:
	case GBA_REG_DMA3CNT_LO:
		// Many, many things read from the DMA register
	case GBA_REG_MAX:
		// Some bad interrupt libraries will read from this
		// (Silent) write-only register
		return 0;

	case GBA_REG_JOY_RECV_LO:
	case GBA_REG_JOY_RECV_HI:
		gba->memory.io[GBA_REG(JOYSTAT)] &= ~JOYSTAT_RECV;
		break;

	// Wave RAM can be written and read even if the audio hardware is disabled.
	// However, it is not possible to switch between the two banks because it
	// isn't possible to write to register SOUND3CNT_LO.
	case GBA_REG_WAVE_RAM0_LO:
		return GBAAudioReadWaveRAM(&gba->audio, 0) & 0xFFFF;
	case GBA_REG_WAVE_RAM0_HI:
		return GBAAudioReadWaveRAM(&gba->audio, 0) >> 16;
	case GBA_REG_WAVE_RAM1_LO:
		return GBAAudioReadWaveRAM(&gba->audio, 1) & 0xFFFF;
	case GBA_REG_WAVE_RAM1_HI:
		return GBAAudioReadWaveRAM(&gba->audio, 1) >> 16;
	case GBA_REG_WAVE_RAM2_LO:
		return GBAAudioReadWaveRAM(&gba->audio, 2) & 0xFFFF;
	case GBA_REG_WAVE_RAM2_HI:
		return GBAAudioReadWaveRAM(&gba->audio, 2) >> 16;
	case GBA_REG_WAVE_RAM3_LO:
		return GBAAudioReadWaveRAM(&gba->audio, 3) & 0xFFFF;
	case GBA_REG_WAVE_RAM3_HI:
		return GBAAudioReadWaveRAM(&gba->audio, 3) >> 16;

	case GBA_REG_SOUND1CNT_LO:
	case GBA_REG_SOUND1CNT_HI:
	case GBA_REG_SOUND1CNT_X:
	case GBA_REG_SOUND2CNT_LO:
	case GBA_REG_SOUND2CNT_HI:
	case GBA_REG_SOUND3CNT_LO:
	case GBA_REG_SOUND3CNT_HI:
	case GBA_REG_SOUND3CNT_X:
	case GBA_REG_SOUND4CNT_LO:
	case GBA_REG_SOUND4CNT_HI:
	case GBA_REG_SOUNDCNT_LO:
		if (!GBAudioEnableIsEnable(gba->memory.io[GBA_REG(SOUNDCNT_X)])) {
			// TODO: Is writing allowed when the circuit is disabled?
			return 0;
		}
		// Fall through
	case GBA_REG_DISPCNT:
	case GBA_REG_STEREOCNT:
	case GBA_REG_DISPSTAT:
	case GBA_REG_VCOUNT:
	case GBA_REG_BG0CNT:
	case GBA_REG_BG1CNT:
	case GBA_REG_BG2CNT:
	case GBA_REG_BG3CNT:
	case GBA_REG_WININ:
	case GBA_REG_WINOUT:
	case GBA_REG_BLDCNT:
	case GBA_REG_BLDALPHA:
	case GBA_REG_SOUNDCNT_HI:
	case GBA_REG_SOUNDCNT_X:
	case GBA_REG_SOUNDBIAS:
	case GBA_REG_DMA0CNT_HI:
	case GBA_REG_DMA1CNT_HI:
	case GBA_REG_DMA2CNT_HI:
	case GBA_REG_DMA3CNT_HI:
	case GBA_REG_TM0CNT_HI:
	case GBA_REG_TM1CNT_HI:
	case GBA_REG_TM2CNT_HI:
	case GBA_REG_TM3CNT_HI:
	case GBA_REG_KEYCNT:
	case GBA_REG_SIOMULTI0:
	case GBA_REG_SIOMULTI1:
	case GBA_REG_SIOMULTI2:
	case GBA_REG_SIOMULTI3:
	case GBA_REG_SIOMLT_SEND:
	case GBA_REG_JOYCNT:
	case GBA_REG_JOY_TRANS_LO:
	case GBA_REG_JOY_TRANS_HI:
	case GBA_REG_JOYSTAT:
	case GBA_REG_IE:
	case GBA_REG_IF:
	case GBA_REG_WAITCNT:
	case GBA_REG_IME:
	case GBA_REG_POSTFLG:
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
	case 0x302:
		mLOG(GBA_IO, GAME_ERROR, "Read from unused I/O register: %03X", address);
		return 0;
	// These registers sit outside of the normal I/O block, so we need to stash them somewhere unused
	case GBA_REG_EXWAITCNT_LO:
	case GBA_REG_EXWAITCNT_HI:
		address += GBA_REG_INTERNAL_EXWAITCNT_LO - GBA_REG_EXWAITCNT_LO;
		break;
	case GBA_REG_DEBUG_ENABLE:
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
	for (i = 0; i < GBA_REG_INTERNAL_MAX; i += 2) {
		if (_isRSpecialRegister[i >> 1]) {
			STORE_16(gba->memory.io[i >> 1], i, state->io);
		} else if (_isValidRegister[i >> 1]) {
			uint16_t reg = GBAIORead(gba, i);
			STORE_16(reg, i, state->io);
		}
	}

	for (i = 0; i < 4; ++i) {
		STORE_16(gba->memory.io[(GBA_REG_DMA0CNT_LO + i * 12) >> 1], (GBA_REG_DMA0CNT_LO + i * 12), state->io);
		STORE_16(gba->timers[i].reload, 0, &state->timers[i].reload);
		STORE_32(gba->timers[i].lastEvent - mTimingCurrentTime(&gba->timing), 0, &state->timers[i].lastEvent);
		STORE_32(gba->timers[i].event.when - mTimingCurrentTime(&gba->timing), 0, &state->timers[i].nextEvent);
		STORE_32(gba->timers[i].flags, 0, &state->timers[i].flags);
	}
	STORE_32(gba->bus, 0, &state->bus);

	GBADMASerialize(gba, state);
	GBAHardwareSerialize(&gba->memory.hw, state);
}

void GBAIODeserialize(struct GBA* gba, const struct GBASerializedState* state) {
	LOAD_16(gba->memory.io[GBA_REG(SOUNDCNT_X)], GBA_REG_SOUNDCNT_X, state->io);
	GBAAudioWriteSOUNDCNT_X(&gba->audio, gba->memory.io[GBA_REG(SOUNDCNT_X)]);

	int i;
	for (i = 0; i < GBA_REG_MAX; i += 2) {
		if (_isWSpecialRegister[i >> 1]) {
			LOAD_16(gba->memory.io[i >> 1], i, state->io);
		} else if (_isValidRegister[i >> 1]) {
			uint16_t reg;
			LOAD_16(reg, i, state->io);
			GBAIOWrite(gba, i, reg);
		}
	}
	if (state->versionMagic >= 0x01000006) {
		GBAIOWrite(gba, GBA_REG_EXWAITCNT_HI, gba->memory.io[GBA_REG(INTERNAL_EXWAITCNT_HI)]);
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
	}
	gba->sio.siocnt = gba->memory.io[GBA_REG(SIOCNT)];
	GBASIOWriteRCNT(&gba->sio, gba->memory.io[GBA_REG(RCNT)]);

	LOAD_32(gba->bus, 0, &state->bus);
	GBADMADeserialize(gba, state);
	GBAHardwareDeserialize(&gba->memory.hw, state);
}
