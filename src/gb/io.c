/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "io.h"

#include "gb/gb.h"

mLOG_DEFINE_CATEGORY(GB_IO, "GB I/O");

const static uint8_t _registerMask[0x50] = {
	[REG_TAC]  = 0xF8,
	[REG_NR10] = 0x80,
	[REG_NR11] = 0x3F,
	[REG_NR12] = 0x00,
	[REG_NR13] = 0xFF,
	[REG_NR14] = 0xBF,
	[REG_NR21] = 0x3F,
	[REG_NR22] = 0x00,
	[REG_NR23] = 0xFF,
	[REG_NR24] = 0xBF,
	[REG_NR30] = 0x7F,
	[REG_NR31] = 0xFF,
	[REG_NR32] = 0x9F,
	[REG_NR33] = 0xFF,
	[REG_NR34] = 0xBF,
	[REG_NR41] = 0xFF,
	[REG_NR42] = 0x00,
	[REG_NR43] = 0x00,
	[REG_NR44] = 0xBF,
	[REG_NR50] = 0x00,
	[REG_NR51] = 0x00,
	[REG_NR52] = 0x70,
	[REG_STAT] = 0x80,
};

void GBIOInit(struct GB* gb) {
	memset(gb->memory.io, 0, sizeof(gb->memory.io));
}

void GBIOReset(struct GB* gb) {
	memset(gb->memory.io, 0, sizeof(gb->memory.io));

	GBIOWrite(gb, 0x05, 0);
	GBIOWrite(gb, 0x06, 0);
	GBIOWrite(gb, 0x07, 0);
	GBIOWrite(gb, 0x10, 0x80);
	GBIOWrite(gb, 0x11, 0xBF);
	GBIOWrite(gb, 0x12, 0xF3);
	GBIOWrite(gb, 0x12, 0xF3);
	GBIOWrite(gb, 0x14, 0xBF);
	GBIOWrite(gb, 0x16, 0x3F);
	GBIOWrite(gb, 0x17, 0x00);
	GBIOWrite(gb, 0x19, 0xBF);
	GBIOWrite(gb, 0x1A, 0x7F);
	GBIOWrite(gb, 0x1B, 0xFF);
	GBIOWrite(gb, 0x1C, 0x9F);
	GBIOWrite(gb, 0x1E, 0xBF);
	GBIOWrite(gb, 0x20, 0xFF);
	GBIOWrite(gb, 0x21, 0x00);
	GBIOWrite(gb, 0x22, 0x00);
	GBIOWrite(gb, 0x23, 0xBF);
	GBIOWrite(gb, 0x24, 0x77);
	GBIOWrite(gb, 0x25, 0xF3);
	GBIOWrite(gb, 0x26, 0xF1);
	GBIOWrite(gb, 0x40, 0x91);
	GBIOWrite(gb, 0x42, 0x00);
	GBIOWrite(gb, 0x43, 0x00);
	GBIOWrite(gb, 0x45, 0x00);
	GBIOWrite(gb, 0x47, 0xFC);
	GBIOWrite(gb, 0x48, 0xFF);
	GBIOWrite(gb, 0x49, 0xFF);
	GBIOWrite(gb, 0x4A, 0x00);
	GBIOWrite(gb, 0x4B, 0x00);
	GBIOWrite(gb, 0xFF, 0x00);
}

void GBIOWrite(struct GB* gb, unsigned address, uint8_t value) {
	switch (address) {
	case REG_DIV:
		GBTimerDivReset(&gb->timer);
		return;
	case REG_NR10:
		if (gb->audio.enable) {
			GBAudioWriteNR10(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case REG_NR11:
		if (gb->audio.enable) {
			GBAudioWriteNR11(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case REG_NR12:
		if (gb->audio.enable) {
			GBAudioWriteNR12(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case REG_NR13:
		if (gb->audio.enable) {
			GBAudioWriteNR13(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case REG_NR14:
		if (gb->audio.enable) {
			GBAudioWriteNR14(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case REG_NR21:
		if (gb->audio.enable) {
			GBAudioWriteNR21(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case REG_NR22:
		if (gb->audio.enable) {
			GBAudioWriteNR22(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case REG_NR23:
		if (gb->audio.enable) {
			GBAudioWriteNR23(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case REG_NR24:
		if (gb->audio.enable) {
			GBAudioWriteNR24(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case REG_NR30:
		if (gb->audio.enable) {
			GBAudioWriteNR30(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case REG_NR31:
		if (gb->audio.enable) {
			GBAudioWriteNR31(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case REG_NR32:
		if (gb->audio.enable) {
			GBAudioWriteNR32(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case REG_NR33:
		if (gb->audio.enable) {
			GBAudioWriteNR33(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case REG_NR34:
		if (gb->audio.enable) {
			GBAudioWriteNR34(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case REG_NR41:
		if (gb->audio.enable) {
			GBAudioWriteNR41(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case REG_NR42:
		if (gb->audio.enable) {
			GBAudioWriteNR42(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case REG_NR43:
		if (gb->audio.enable) {
			GBAudioWriteNR43(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case REG_NR44:
		if (gb->audio.enable) {
			GBAudioWriteNR44(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case REG_NR50:
		if (gb->audio.enable) {
			GBAudioWriteNR50(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case REG_NR51:
		if (gb->audio.enable) {
			GBAudioWriteNR51(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case REG_NR52:
		GBAudioWriteNR52(&gb->audio, value);
		value &= 0x80;
		value |= gb->memory.io[REG_NR52] & 0x0F;
		break;
	case REG_WAVE_0:
	case REG_WAVE_1:
	case REG_WAVE_2:
	case REG_WAVE_3:
	case REG_WAVE_4:
	case REG_WAVE_5:
	case REG_WAVE_6:
	case REG_WAVE_7:
	case REG_WAVE_8:
	case REG_WAVE_9:
	case REG_WAVE_A:
	case REG_WAVE_B:
	case REG_WAVE_C:
	case REG_WAVE_D:
	case REG_WAVE_E:
	case REG_WAVE_F:
		((uint8_t*) gb->audio.ch3.wavedata)[address - REG_WAVE_0] = value; // TODO: Big endian
		break;
	case REG_JOYP:
	case REG_TIMA:
	case REG_TMA:
	case REG_LYC:
		// Handled transparently by the registers
		break;
	case REG_TAC:
		value = GBTimerUpdateTAC(&gb->timer, value);
		break;
	case REG_IF:
		gb->memory.io[REG_IF] = value | 0xE0;
		GBUpdateIRQs(gb);
		return;
	case REG_LCDC:
		// TODO: handle GBC differences
		value = gb->video.renderer->writeVideoRegister(gb->video.renderer, address, value);
		GBVideoWriteLCDC(&gb->video, value);
		break;
	case REG_DMA:
		GBMemoryDMA(gb, value << 8);
		break;
	case REG_SCY:
	case REG_SCX:
	case REG_WY:
	case REG_WX:
	case REG_BGP:
	case REG_OBP0:
	case REG_OBP1:
		value = gb->video.renderer->writeVideoRegister(gb->video.renderer, address, value);
		break;
	case REG_STAT:
		GBVideoWriteSTAT(&gb->video, value);
		break;
	case REG_IE:
		gb->memory.ie = value;
		GBUpdateIRQs(gb);
		return;
	default:
		mLOG(GB_IO, STUB, "Writing to unknown register FF%02X:%02X", address, value);
		if (address >= GB_SIZE_IO) {
			return;
		}
		break;
	}
	gb->memory.io[address] = value;
}

static uint8_t _readKeys(struct GB* gb) {
	uint8_t keys = *gb->keySource;
	switch (gb->memory.io[REG_JOYP] & 0x30) {
	case 0x20:
		keys >>= 4;
		break;
	case 0x10:
		break;
	default:
		// ???
		keys = 0;
		break;
	}
	return 0xC0 | (gb->memory.io[REG_JOYP] | 0xF) ^ (keys & 0xF);
}

uint8_t GBIORead(struct GB* gb, unsigned address) {
	switch (address) {
	case REG_JOYP:
		return _readKeys(gb);
	case REG_IF:
		break;
	case REG_IE:
		return gb->memory.ie;
	case REG_NR10:
	case REG_NR11:
	case REG_NR12:
	case REG_NR14:
	case REG_NR21:
	case REG_NR22:
	case REG_NR24:
	case REG_NR30:
	case REG_NR32:
	case REG_NR34:
	case REG_NR41:
	case REG_NR42:
	case REG_NR43:
	case REG_NR44:
	case REG_NR50:
	case REG_NR51:
	case REG_NR52:
	case REG_WAVE_0:
	case REG_WAVE_1:
	case REG_WAVE_2:
	case REG_WAVE_3:
	case REG_WAVE_4:
	case REG_WAVE_5:
	case REG_WAVE_6:
	case REG_WAVE_7:
	case REG_WAVE_8:
	case REG_WAVE_9:
	case REG_WAVE_A:
	case REG_WAVE_B:
	case REG_WAVE_C:
	case REG_WAVE_D:
	case REG_WAVE_E:
	case REG_WAVE_F:
	case REG_DIV:
	case REG_TIMA:
	case REG_TMA:
	case REG_TAC:
	case REG_STAT:
	case REG_LCDC:
	case REG_SCY:
	case REG_SCX:
	case REG_LY:
	case REG_LYC:
		// Handled transparently by the registers
		break;
	default:
		mLOG(GB_IO, STUB, "Reading from unknown register FF%02X", address);
		return 0xFF;
	}
	return gb->memory.io[address] | _registerMask[address];
}
