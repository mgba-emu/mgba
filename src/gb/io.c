/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/io.h>

#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/sio.h>
#include <mgba/internal/gb/serialize.h>

mLOG_DEFINE_CATEGORY(GB_IO, "GB I/O", "gb.io");

MGBA_EXPORT const char* const GBIORegisterNames[] = {
	[REG_JOYP] = "JOYP",
	[REG_SB] = "SB",
	[REG_SC] = "SC",
	[REG_DIV] = "DIV",
	[REG_TIMA] = "TIMA",
	[REG_TMA] = "TMA",
	[REG_TAC] = "TAC",
	[REG_IF] = "IF",
	[REG_NR10] = "NR10",
	[REG_NR11] = "NR11",
	[REG_NR12] = "NR12",
	[REG_NR13] = "NR13",
	[REG_NR14] = "NR14",
	[REG_NR21] = "NR21",
	[REG_NR22] = "NR22",
	[REG_NR23] = "NR23",
	[REG_NR24] = "NR24",
	[REG_NR30] = "NR30",
	[REG_NR31] = "NR31",
	[REG_NR32] = "NR32",
	[REG_NR33] = "NR33",
	[REG_NR34] = "NR34",
	[REG_NR41] = "NR41",
	[REG_NR42] = "NR42",
	[REG_NR43] = "NR43",
	[REG_NR44] = "NR44",
	[REG_NR50] = "NR50",
	[REG_NR51] = "NR51",
	[REG_NR52] = "NR52",
	[REG_LCDC] = "LCDC",
	[REG_STAT] = "STAT",
	[REG_SCY] = "SCY",
	[REG_SCX] = "SCX",
	[REG_LY] = "LY",
	[REG_LYC] = "LYC",
	[REG_DMA] = "DMA",
	[REG_BGP] = "BGP",
	[REG_OBP0] = "OBP0",
	[REG_OBP1] = "OBP1",
	[REG_WY] = "WY",
	[REG_WX] = "WX",
	[REG_KEY1] = "KEY1",
	[REG_VBK] = "VBK",
	[REG_HDMA1] = "HDMA1",
	[REG_HDMA2] = "HDMA2",
	[REG_HDMA3] = "HDMA3",
	[REG_HDMA4] = "HDMA4",
	[REG_HDMA5] = "HDMA5",
	[REG_RP] = "RP",
	[REG_BCPS] = "BCPS",
	[REG_BCPD] = "BCPD",
	[REG_OCPS] = "OCPS",
	[REG_OCPD] = "OCPD",
	[REG_SVBK] = "SVBK",
	[REG_IE] = "IE",
};

static const uint8_t _registerMask[] = {
	[REG_SC]   = 0x7E, // TODO: GBC differences
	[REG_IF]   = 0xE0,
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
	[REG_KEY1] = 0x7E,
	[REG_VBK] = 0xFE,
	[REG_OCPS] = 0x40,
	[REG_BCPS] = 0x40,
	[REG_UNK6C] = 0xFE,
	[REG_SVBK] = 0xF8,
	[REG_IE]   = 0xE0,
};

static uint8_t _readKeys(struct GB* gb);
static uint8_t _readKeysFiltered(struct GB* gb);

static void _writeSGBBits(struct GB* gb, int bits) {
	if (!bits) {
		gb->sgbBit = -1;
		memset(gb->sgbPacket, 0, sizeof(gb->sgbPacket));
	}
	if (bits == gb->currentSgbBits) {
		return;
	}
	gb->currentSgbBits = bits;
	if (gb->sgbBit > 128) {
		switch (bits) {
		case 1:
			gb->sgbBit |= 2;
			break;
		case 2:
			gb->sgbBit |= 4;
			break;
		case 3:
			if (gb->sgbBit == 135) {
				gb->sgbBit &= ~6;
				gb->sgbCurrentController = (gb->sgbCurrentController + 1) & gb->sgbControllers;
			}
			break;
		}
	}
	if (gb->sgbBit == 128 && bits == 2) {
		GBVideoWriteSGBPacket(&gb->video, gb->sgbPacket);
		++gb->sgbBit;
	}
	if (gb->sgbBit >= 128) {
		return;
	}
	switch (bits) {
	case 1:
		if (gb->sgbBit < 0) {
			return;
		}
		gb->sgbPacket[gb->sgbBit >> 3] |= 1 << (gb->sgbBit & 7);
		break;
	case 3:
		++gb->sgbBit;
	default:
		break;
	}
}

void GBIOInit(struct GB* gb) {
	memset(gb->memory.io, 0, sizeof(gb->memory.io));
}

void GBIOReset(struct GB* gb) {
	memset(gb->memory.io, 0, sizeof(gb->memory.io));

	GBIOWrite(gb, REG_TIMA, 0);
	GBIOWrite(gb, REG_TMA, 0);
	GBIOWrite(gb, REG_TAC, 0);
	GBIOWrite(gb, REG_IF, 1);
	GBIOWrite(gb, REG_NR52, 0xF1);
	GBIOWrite(gb, REG_NR14, 0x3F);
	GBIOWrite(gb, REG_NR10, 0x80);
	GBIOWrite(gb, REG_NR11, 0xBF);
	GBIOWrite(gb, REG_NR12, 0xF3);
	GBIOWrite(gb, REG_NR13, 0xF3);
	GBIOWrite(gb, REG_NR24, 0x3F);
	GBIOWrite(gb, REG_NR21, 0x3F);
	GBIOWrite(gb, REG_NR22, 0x00);
	GBIOWrite(gb, REG_NR34, 0x3F);
	GBIOWrite(gb, REG_NR30, 0x7F);
	GBIOWrite(gb, REG_NR31, 0xFF);
	GBIOWrite(gb, REG_NR32, 0x9F);
	GBIOWrite(gb, REG_NR44, 0x3F);
	GBIOWrite(gb, REG_NR41, 0xFF);
	GBIOWrite(gb, REG_NR42, 0x00);
	GBIOWrite(gb, REG_NR43, 0x00);
	GBIOWrite(gb, REG_NR50, 0x77);
	GBIOWrite(gb, REG_NR51, 0xF3);
	if (!gb->biosVf) {
		GBIOWrite(gb, REG_LCDC, 0x91);
		gb->memory.io[0x50] = 1;
	} else {
		GBIOWrite(gb, REG_LCDC, 0x00);
		gb->memory.io[0x50] = 0xFF;
	}
	GBIOWrite(gb, REG_SCY, 0x00);
	GBIOWrite(gb, REG_SCX, 0x00);
	GBIOWrite(gb, REG_LYC, 0x00);
	GBIOWrite(gb, REG_DMA, 0xFF);
	GBIOWrite(gb, REG_BGP, 0xFC);
	if (gb->model < GB_MODEL_CGB) {
		GBIOWrite(gb, REG_OBP0, 0xFF);
		GBIOWrite(gb, REG_OBP1, 0xFF);
	}
	GBIOWrite(gb, REG_WY, 0x00);
	GBIOWrite(gb, REG_WX, 0x00);
	if (gb->model & GB_MODEL_CGB) {
		GBIOWrite(gb, REG_UNK4C, 0);
		GBIOWrite(gb, REG_JOYP, 0xFF);
		GBIOWrite(gb, REG_VBK, 0);
		GBIOWrite(gb, REG_BCPS, 0);
		GBIOWrite(gb, REG_OCPS, 0);
		GBIOWrite(gb, REG_SVBK, 1);
		GBIOWrite(gb, REG_HDMA1, 0xFF);
		GBIOWrite(gb, REG_HDMA2, 0xFF);
		GBIOWrite(gb, REG_HDMA3, 0xFF);
		GBIOWrite(gb, REG_HDMA4, 0xFF);
		gb->memory.io[REG_HDMA5] = 0xFF;
	} else if (gb->model & GB_MODEL_SGB) {
		GBIOWrite(gb, REG_JOYP, 0xFF);
	}
	GBIOWrite(gb, REG_IE, 0x00);
}

void GBIOWrite(struct GB* gb, unsigned address, uint8_t value) {
	switch (address) {
	case REG_SB:
		GBSIOWriteSB(&gb->sio, value);
		break;
	case REG_SC:
		GBSIOWriteSC(&gb->sio, value);
		break;
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
			if (gb->audio.style == GB_AUDIO_DMG) {
				GBAudioWriteNR11(&gb->audio, value & _registerMask[REG_NR11]);
			}
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
			if (gb->audio.style == GB_AUDIO_DMG) {
				GBAudioWriteNR21(&gb->audio, value & _registerMask[REG_NR21]);
			}
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
		if (gb->audio.enable || gb->audio.style == GB_AUDIO_DMG) {
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
		if (gb->audio.enable || gb->audio.style == GB_AUDIO_DMG) {
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
		if (!gb->audio.playingCh3 || gb->audio.style != GB_AUDIO_DMG) {
			gb->audio.ch3.wavedata8[address - REG_WAVE_0] = value;
		} else if(gb->audio.ch3.readable) {
			gb->audio.ch3.wavedata8[gb->audio.ch3.window >> 1] = value;
		}
		break;
	case REG_JOYP:
		gb->memory.io[REG_JOYP] = value | 0x0F;
		_readKeys(gb);
		if (gb->model & GB_MODEL_SGB) {
			_writeSGBBits(gb, (value >> 4) & 3);
		}
		return;
	case REG_TIMA:
		if (value && mTimingUntil(&gb->timing, &gb->timer.irq) > 1) {
			mTimingDeschedule(&gb->timing, &gb->timer.irq);
		}
		if (mTimingUntil(&gb->timing, &gb->timer.irq) == -1) {
			return;
		}
		break;
	case REG_TMA:
		if (mTimingUntil(&gb->timing, &gb->timer.irq) == -1) {
			gb->memory.io[REG_TIMA] = value;
		}
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
		GBVideoProcessDots(&gb->video, 0);
		value = gb->video.renderer->writeVideoRegister(gb->video.renderer, address, value);
		GBVideoWriteLCDC(&gb->video, value);
		break;
	case REG_LYC:
		GBVideoWriteLYC(&gb->video, value);
		break;
	case REG_DMA:
		GBMemoryDMA(gb, value << 8);
		break;
	case REG_SCY:
	case REG_SCX:
	case REG_WY:
	case REG_WX:
		GBVideoProcessDots(&gb->video, 0);
		value = gb->video.renderer->writeVideoRegister(gb->video.renderer, address, value);
		break;
	case REG_BGP:
	case REG_OBP0:
	case REG_OBP1:
		GBVideoProcessDots(&gb->video, 0);
		GBVideoWritePalette(&gb->video, address, value);
		break;
	case REG_STAT:
		GBVideoWriteSTAT(&gb->video, value);
		value = gb->video.stat;
		break;
	case 0x50:
		GBUnmapBIOS(gb);
		if (gb->model >= GB_MODEL_CGB && gb->memory.io[REG_UNK4C] < 0x80) {
			gb->model = GB_MODEL_DMG;
			GBVideoDisableCGB(&gb->video);
		}
		break;
	case REG_IE:
		gb->memory.ie = value;
		GBUpdateIRQs(gb);
		return;
	default:
		if (gb->model >= GB_MODEL_CGB) {
			switch (address) {
			case REG_UNK4C:
				break;
			case REG_KEY1:
				value &= 0x1;
				value |= gb->memory.io[address] & 0x80;
				break;
			case REG_VBK:
				GBVideoSwitchBank(&gb->video, value);
				break;
			case REG_HDMA1:
			case REG_HDMA2:
			case REG_HDMA3:
			case REG_HDMA4:
				// Handled transparently by the registers
				break;
			case REG_HDMA5:
				value = GBMemoryWriteHDMA5(gb, value);
				break;
			case REG_BCPS:
				gb->video.bcpIndex = value & 0x3F;
				gb->video.bcpIncrement = value & 0x80;
				gb->memory.io[REG_BCPD] = gb->video.palette[gb->video.bcpIndex >> 1] >> (8 * (gb->video.bcpIndex & 1));
				break;
			case REG_BCPD:
				if (gb->video.mode != 3) {
					GBVideoProcessDots(&gb->video, 0);
					GBVideoWritePalette(&gb->video, address, value);
				}
				return;
			case REG_OCPS:
				gb->video.ocpIndex = value & 0x3F;
				gb->video.ocpIncrement = value & 0x80;
				gb->memory.io[REG_OCPD] = gb->video.palette[8 * 4 + (gb->video.ocpIndex >> 1)] >> (8 * (gb->video.ocpIndex & 1));
				break;
			case REG_OCPD:
				if (gb->video.mode != 3) {
					GBVideoProcessDots(&gb->video, 0);
					GBVideoWritePalette(&gb->video, address, value);
				}
				return;
			case REG_SVBK:
				GBMemorySwitchWramBank(&gb->memory, value);
				value = gb->memory.wramCurrentBank;
				break;
			default:
				goto failed;
			}
			goto success;
		}
		failed:
		mLOG(GB_IO, STUB, "Writing to unknown register FF%02X:%02X", address, value);
		if (address >= GB_SIZE_IO) {
			return;
		}
		break;
	}
	success:
	gb->memory.io[address] = value;
}

static uint8_t _readKeys(struct GB* gb) {
	uint8_t keys = *gb->keySource;
	if (gb->sgbCurrentController != 0) {
		keys = 0;
	}
	uint8_t joyp = gb->memory.io[REG_JOYP];
	switch (joyp & 0x30) {
	case 0x30:
		keys = gb->sgbCurrentController;
		break;
	case 0x20:
		keys >>= 4;
		break;
	case 0x10:
		break;
	case 0x00:
		keys |= keys >> 4;
		break;
	}
	gb->memory.io[REG_JOYP] = (0xCF | joyp) ^ (keys & 0xF);
	if (joyp & ~gb->memory.io[REG_JOYP] & 0xF) {
		gb->memory.io[REG_IF] |= (1 << GB_IRQ_KEYPAD);
		GBUpdateIRQs(gb);
	}
	return gb->memory.io[REG_JOYP];
}

static uint8_t _readKeysFiltered(struct GB* gb) {
	uint8_t keys = _readKeys(gb);
	if (!gb->allowOpposingDirections && (keys & 0x30) == 0x20) {
		unsigned rl = keys & 0x03;
		unsigned ud = keys & 0x0C;
		if (!rl) {
			keys |= 0x03;
		}
		if (!ud) {
			keys |= 0x0C;
		}
	}
	return keys;
}

uint8_t GBIORead(struct GB* gb, unsigned address) {
	switch (address) {
	case REG_JOYP:
		{
			size_t c;
			for (c = 0; c < mCoreCallbacksListSize(&gb->coreCallbacks); ++c) {
				struct mCoreCallbacks* callbacks = mCoreCallbacksListGetPointer(&gb->coreCallbacks, c);
				if (callbacks->keysRead) {
					callbacks->keysRead(callbacks->context);
				}
			}
		}
		return _readKeysFiltered(gb);
	case REG_IE:
		return gb->memory.ie;
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
		if (gb->audio.playingCh3) {
			if (gb->audio.ch3.readable || gb->audio.style != GB_AUDIO_DMG) {
				return gb->audio.ch3.wavedata8[gb->audio.ch3.window >> 1];
			} else {
				return 0xFF;
			}
		} else {
			return gb->audio.ch3.wavedata8[address - REG_WAVE_0];
		}
		break;
	case REG_SB:
	case REG_SC:
	case REG_IF:
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
	case REG_DMA:
	case REG_BGP:
	case REG_OBP0:
	case REG_OBP1:
	case REG_WY:
	case REG_WX:
		// Handled transparently by the registers
		break;
	default:
		if (gb->model >= GB_MODEL_CGB) {
			switch (address) {
			case REG_KEY1:
			case REG_VBK:
			case REG_HDMA1:
			case REG_HDMA2:
			case REG_HDMA3:
			case REG_HDMA4:
			case REG_HDMA5:
			case REG_BCPS:
			case REG_BCPD:
			case REG_OCPS:
			case REG_OCPD:
			case REG_SVBK:
				// Handled transparently by the registers
				goto success;
			default:
				break;
			}
		}
		mLOG(GB_IO, STUB, "Reading from unknown register FF%02X", address);
		return 0xFF;
	}
	success:
	return gb->memory.io[address] | _registerMask[address];
}

void GBTestKeypadIRQ(struct GB* gb) {
	_readKeys(gb);
}

struct GBSerializedState;
void GBIOSerialize(const struct GB* gb, struct GBSerializedState* state) {
	memcpy(state->io, gb->memory.io, GB_SIZE_IO);
	state->ie = gb->memory.ie;
}

void GBIODeserialize(struct GB* gb, const struct GBSerializedState* state) {
	memcpy(gb->memory.io, state->io, GB_SIZE_IO);
	gb->memory.ie = state->ie;

	if (GBAudioEnableGetEnable(*gb->audio.nr52)) {
		GBIOWrite(gb, REG_NR10, gb->memory.io[REG_NR10]);
		GBIOWrite(gb, REG_NR11, gb->memory.io[REG_NR11]);
		GBIOWrite(gb, REG_NR12, gb->memory.io[REG_NR12]);
		GBIOWrite(gb, REG_NR13, gb->memory.io[REG_NR13]);
		gb->audio.ch1.control.frequency &= 0xFF;
		gb->audio.ch1.control.frequency |= GBAudioRegisterControlGetFrequency(gb->memory.io[REG_NR14] << 8);
		gb->audio.ch1.control.stop = GBAudioRegisterControlGetStop(gb->memory.io[REG_NR14] << 8);
		GBIOWrite(gb, REG_NR21, gb->memory.io[REG_NR21]);
		GBIOWrite(gb, REG_NR22, gb->memory.io[REG_NR22]);
		GBIOWrite(gb, REG_NR22, gb->memory.io[REG_NR23]);
		gb->audio.ch2.control.frequency &= 0xFF;
		gb->audio.ch2.control.frequency |= GBAudioRegisterControlGetFrequency(gb->memory.io[REG_NR24] << 8);
		gb->audio.ch2.control.stop = GBAudioRegisterControlGetStop(gb->memory.io[REG_NR24] << 8);
		GBIOWrite(gb, REG_NR30, gb->memory.io[REG_NR30]);
		GBIOWrite(gb, REG_NR31, gb->memory.io[REG_NR31]);
		GBIOWrite(gb, REG_NR32, gb->memory.io[REG_NR32]);
		GBIOWrite(gb, REG_NR32, gb->memory.io[REG_NR33]);
		gb->audio.ch3.rate &= 0xFF;
		gb->audio.ch3.rate |= GBAudioRegisterControlGetRate(gb->memory.io[REG_NR34] << 8);
		gb->audio.ch3.stop = GBAudioRegisterControlGetStop(gb->memory.io[REG_NR34] << 8);
		GBIOWrite(gb, REG_NR41, gb->memory.io[REG_NR41]);
		GBIOWrite(gb, REG_NR42, gb->memory.io[REG_NR42]);
		GBIOWrite(gb, REG_NR43, gb->memory.io[REG_NR43]);
		gb->audio.ch4.stop = GBAudioRegisterNoiseControlGetStop(gb->memory.io[REG_NR44]);
		GBIOWrite(gb, REG_NR50, gb->memory.io[REG_NR50]);
		GBIOWrite(gb, REG_NR51, gb->memory.io[REG_NR51]);
	}

	gb->video.renderer->writeVideoRegister(gb->video.renderer, REG_LCDC, state->io[REG_LCDC]);
	gb->video.renderer->writeVideoRegister(gb->video.renderer, REG_SCY, state->io[REG_SCY]);
	gb->video.renderer->writeVideoRegister(gb->video.renderer, REG_SCX, state->io[REG_SCX]);
	gb->video.renderer->writeVideoRegister(gb->video.renderer, REG_WY, state->io[REG_WY]);
	gb->video.renderer->writeVideoRegister(gb->video.renderer, REG_WX, state->io[REG_WX]);
	if (gb->model & GB_MODEL_SGB) {
		gb->video.renderer->writeVideoRegister(gb->video.renderer, REG_BGP, state->io[REG_BGP]);
		gb->video.renderer->writeVideoRegister(gb->video.renderer, REG_OBP0, state->io[REG_OBP0]);
		gb->video.renderer->writeVideoRegister(gb->video.renderer, REG_OBP1, state->io[REG_OBP1]);
	}
	gb->video.stat = state->io[REG_STAT];
}
