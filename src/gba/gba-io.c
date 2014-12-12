/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba-io.h"

#include "gba-rr.h"
#include "gba-serialize.h"
#include "gba-sio.h"
#include "gba-video.h"

const char* GBAIORegisterNames[] = {
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
	"JOY_RECV_HI",
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
	1, 1, 1, 1, 1, 0, 0, 0,
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

static const int _isSpecialRegister[REG_MAX >> 1] = {
	// Video
	0, 0, 0, 1, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	// Audio
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
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
	1, 1, 1, 0, 1
};

void GBAIOInit(struct GBA* gba) {
	gba->memory.io[REG_DISPCNT >> 1] = 0x0080;
	gba->memory.io[REG_RCNT >> 1] = RCNT_INITIAL;
	gba->memory.io[REG_KEYINPUT >> 1] = 0x3FF;
	gba->memory.io[REG_SOUNDBIAS >> 1] = 0x200;
}

void GBAIOWrite(struct GBA* gba, uint32_t address, uint16_t value) {
	if (address < REG_SOUND1CNT_LO && address != REG_DISPSTAT) {
		value = gba->video.renderer->writeVideoRegister(gba->video.renderer, address, value);
	} else {
		switch (address) {
		// Video
		case REG_DISPSTAT:
			value &= 0xFFF8;
			GBAVideoWriteDISPSTAT(&gba->video, value);
			break;

		// Audio
		case REG_SOUND1CNT_LO:
			GBAAudioWriteSOUND1CNT_LO(&gba->audio, value);
			value &= 0x00FF;
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
			value &= 0xE000;
			break;
		case REG_SOUND3CNT_X:
			GBAAudioWriteSOUND3CNT_X(&gba->audio, value);
			// TODO: The low bits need to not be readable, but still 8-bit writable
			value &= 0x43FF;
			break;
		case REG_SOUND4CNT_LO:
			GBAAudioWriteSOUND4CNT_LO(&gba->audio, value);
			value &= 0xFF00;
			break;
		case REG_SOUND4CNT_HI:
			GBAAudioWriteSOUND4CNT_HI(&gba->audio, value);
			value &= 0x40FF;
			break;
		case REG_SOUNDCNT_LO:
			GBAAudioWriteSOUNDCNT_LO(&gba->audio, value);
			break;
		case REG_SOUNDCNT_HI:
			GBAAudioWriteSOUNDCNT_HI(&gba->audio, value);
			break;
		case REG_SOUNDCNT_X:
			GBAAudioWriteSOUNDCNT_X(&gba->audio, value);
			break;
		case REG_SOUNDBIAS:
			GBAAudioWriteSOUNDBIAS(&gba->audio, value);
			break;

		case REG_WAVE_RAM0_LO:
		case REG_WAVE_RAM1_LO:
		case REG_WAVE_RAM2_LO:
		case REG_WAVE_RAM3_LO:
		case REG_FIFO_A_LO:
		case REG_FIFO_B_LO:
			GBAIOWrite32(gba, address, (gba->memory.io[(address >> 1) + 1] << 16) | value);
			break;

		case REG_WAVE_RAM0_HI:
		case REG_WAVE_RAM1_HI:
		case REG_WAVE_RAM2_HI:
		case REG_WAVE_RAM3_HI:
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
			GBAMemoryWriteDMACNT_LO(gba, 0, value);
			break;
		case REG_DMA0CNT_HI:
			value = GBAMemoryWriteDMACNT_HI(gba, 0, value);
			break;
		case REG_DMA1CNT_LO:
			GBAMemoryWriteDMACNT_LO(gba, 1, value);
			break;
		case REG_DMA1CNT_HI:
			value = GBAMemoryWriteDMACNT_HI(gba, 1, value);
			break;
		case REG_DMA2CNT_LO:
			GBAMemoryWriteDMACNT_LO(gba, 2, value);
			break;
		case REG_DMA2CNT_HI:
			value = GBAMemoryWriteDMACNT_HI(gba, 2, value);
			break;
		case REG_DMA3CNT_LO:
			GBAMemoryWriteDMACNT_LO(gba, 3, value);
			break;
		case REG_DMA3CNT_HI:
			value = GBAMemoryWriteDMACNT_HI(gba, 3, value);
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
		case REG_SIOMLT_SEND:
			GBASIOWriteSIOMLT_SEND(&gba->sio, value);
			break;

		// Interrupts and misc
		case REG_WAITCNT:
			GBAAdjustWaitstates(gba, value);
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
		case REG_MAX:
			// Some bad interrupt libraries will write to this
			break;
		default:
			GBALog(gba, GBA_LOG_STUB, "Stub I/O register write: %03x", address);
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
			GBALog(gba, GBA_LOG_STUB, "Stop unimplemented");
		}
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
		GBAMemoryWriteDMASAD(gba, 0, value);
		break;
	case REG_DMA0DAD_LO:
		GBAMemoryWriteDMADAD(gba, 0, value);
		break;
	case REG_DMA1SAD_LO:
		GBAMemoryWriteDMASAD(gba, 1, value);
		break;
	case REG_DMA1DAD_LO:
		GBAMemoryWriteDMADAD(gba, 1, value);
		break;
	case REG_DMA2SAD_LO:
		GBAMemoryWriteDMASAD(gba, 2, value);
		break;
	case REG_DMA2DAD_LO:
		GBAMemoryWriteDMADAD(gba, 2, value);
		break;
	case REG_DMA3SAD_LO:
		GBAMemoryWriteDMASAD(gba, 3, value);
		break;
	case REG_DMA3DAD_LO:
		GBAMemoryWriteDMADAD(gba, 3, value);
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
		if (GBARRIsPlaying(gba->rr)) {
			return 0x3FF ^ GBARRQueryInput(gba->rr);
		} else if (gba->keySource) {
			uint16_t input = *gba->keySource;
			if (GBARRIsRecording(gba->rr)) {
				GBARRLogInput(gba->rr, input);
			}
			return 0x3FF ^ input;
		}
		break;

	case REG_SIOCNT:
		return gba->sio.siocnt;
	case REG_RCNT:
		return gba->sio.rcnt;

	case REG_DMA0CNT_LO:
	case REG_DMA1CNT_LO:
	case REG_DMA2CNT_LO:
	case REG_DMA3CNT_LO:
		// Write-only register
		return 0;
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
	case REG_DMA0CNT_HI:
	case REG_DMA1CNT_HI:
	case REG_DMA2CNT_HI:
	case REG_DMA3CNT_HI:
	case REG_SIOMULTI0:
	case REG_SIOMULTI1:
	case REG_SIOMULTI2:
	case REG_SIOMULTI3:
	case REG_SIOMLT_SEND:
	case REG_IE:
	case REG_IF:
	case REG_WAITCNT:
	case REG_IME:
		// Handled transparently by registers
		break;
	case REG_MAX:
		// Some bad interrupt libraries will read from this
		break;
	default:
		GBALog(gba, GBA_LOG_STUB, "Stub I/O register read: %03x", address);
		break;
	}
	return gba->memory.io[address >> 1];
}

void GBAIOSerialize(struct GBA* gba, struct GBASerializedState* state) {
	int i;
	for (i = 0; i < REG_MAX; i += 2) {
		if (_isSpecialRegister[i >> 1]) {
			state->io[i >> 1] = gba->memory.io[i >> 1];
		} else if (_isValidRegister[i >> 1]) {
			state->io[i >> 1] = GBAIORead(gba, i);
		}
	}

	for (i = 0; i < 4; ++i) {
		state->io[(REG_DMA0CNT_LO + i * 12) >> 1] = gba->memory.io[(REG_DMA0CNT_LO + i * 12) >> 1];
		state->dma[i].nextSource = gba->memory.dma[i].nextSource;
		state->dma[i].nextDest = gba->memory.dma[i].nextDest;
		state->dma[i].nextCount = gba->memory.dma[i].nextCount;
		state->dma[i].nextEvent = gba->memory.dma[i].nextEvent;
	}

	memcpy(state->timers, gba->timers, sizeof(state->timers));
	GBAGPIOSerialize(&gba->memory.gpio, state);
}

void GBAIODeserialize(struct GBA* gba, struct GBASerializedState* state) {
	int i;
	for (i = 0; i < REG_MAX; i += 2) {
		if (_isSpecialRegister[i >> 1]) {
			gba->memory.io[i >> 1] = state->io[i >> 1];
		} else if (_isValidRegister[i >> 1]) {
			GBAIOWrite(gba, i, state->io[i >> 1]);
		}
	}

	gba->timersEnabled = 0;
	memcpy(gba->timers, state->timers, sizeof(gba->timers));
	for (i = 0; i < 4; ++i) {
		gba->memory.dma[i].reg = state->io[(REG_DMA0CNT_HI + i * 12) >> 1];
		gba->memory.dma[i].nextSource = state->dma[i].nextSource;
		gba->memory.dma[i].nextDest = state->dma[i].nextDest;
		gba->memory.dma[i].nextCount = state->dma[i].nextCount;
		gba->memory.dma[i].nextEvent = state->dma[i].nextEvent;
		if (GBADMARegisterGetTiming(gba->memory.dma[i].reg) != DMA_TIMING_NOW) {
			GBAMemoryScheduleDMA(gba, i, &gba->memory.dma[i]);
		}

		if (gba->timers[i].enable) {
			gba->timersEnabled |= 1 << i;
		}
	}
	GBAGPIODeserialize(&gba->memory.gpio, state);
}
