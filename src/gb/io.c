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
	[GB_REG_JOYP] = "JOYP",
	[GB_REG_SB] = "SB",
	[GB_REG_SC] = "SC",
	[GB_REG_DIV] = "DIV",
	[GB_REG_TIMA] = "TIMA",
	[GB_REG_TMA] = "TMA",
	[GB_REG_TAC] = "TAC",
	[GB_REG_IF] = "IF",
	[GB_REG_NR10] = "NR10",
	[GB_REG_NR11] = "NR11",
	[GB_REG_NR12] = "NR12",
	[GB_REG_NR13] = "NR13",
	[GB_REG_NR14] = "NR14",
	[GB_REG_NR21] = "NR21",
	[GB_REG_NR22] = "NR22",
	[GB_REG_NR23] = "NR23",
	[GB_REG_NR24] = "NR24",
	[GB_REG_NR30] = "NR30",
	[GB_REG_NR31] = "NR31",
	[GB_REG_NR32] = "NR32",
	[GB_REG_NR33] = "NR33",
	[GB_REG_NR34] = "NR34",
	[GB_REG_NR41] = "NR41",
	[GB_REG_NR42] = "NR42",
	[GB_REG_NR43] = "NR43",
	[GB_REG_NR44] = "NR44",
	[GB_REG_NR50] = "NR50",
	[GB_REG_NR51] = "NR51",
	[GB_REG_NR52] = "NR52",
	[GB_REG_LCDC] = "LCDC",
	[GB_REG_STAT] = "STAT",
	[GB_REG_SCY] = "SCY",
	[GB_REG_SCX] = "SCX",
	[GB_REG_LY] = "LY",
	[GB_REG_LYC] = "LYC",
	[GB_REG_DMA] = "DMA",
	[GB_REG_BGP] = "BGP",
	[GB_REG_OBP0] = "OBP0",
	[GB_REG_OBP1] = "OBP1",
	[GB_REG_WY] = "WY",
	[GB_REG_WX] = "WX",
	[GB_REG_KEY0] = "KEY0",
	[GB_REG_KEY1] = "KEY1",
	[GB_REG_VBK] = "VBK",
	[GB_REG_BANK] = "BANK",
	[GB_REG_HDMA1] = "HDMA1",
	[GB_REG_HDMA2] = "HDMA2",
	[GB_REG_HDMA3] = "HDMA3",
	[GB_REG_HDMA4] = "HDMA4",
	[GB_REG_HDMA5] = "HDMA5",
	[GB_REG_RP] = "RP",
	[GB_REG_BCPS] = "BCPS",
	[GB_REG_BCPD] = "BCPD",
	[GB_REG_OCPS] = "OCPS",
	[GB_REG_OCPD] = "OCPD",
	[GB_REG_OPRI] = "OPRI",
	[GB_REG_SVBK] = "SVBK",
	[GB_REG_IE] = "IE",
};

static const uint8_t _registerMask[] = {
	[GB_REG_SC]   = 0x7E, // TODO: GBC differences
	[GB_REG_IF]   = 0xE0,
	[GB_REG_TAC]  = 0xF8,
	[GB_REG_NR10] = 0x80,
	[GB_REG_NR11] = 0x3F,
	[GB_REG_NR12] = 0x00,
	[GB_REG_NR13] = 0xFF,
	[GB_REG_NR14] = 0xBF,
	[GB_REG_NR21] = 0x3F,
	[GB_REG_NR22] = 0x00,
	[GB_REG_NR23] = 0xFF,
	[GB_REG_NR24] = 0xBF,
	[GB_REG_NR30] = 0x7F,
	[GB_REG_NR31] = 0xFF,
	[GB_REG_NR32] = 0x9F,
	[GB_REG_NR33] = 0xFF,
	[GB_REG_NR34] = 0xBF,
	[GB_REG_NR41] = 0xFF,
	[GB_REG_NR42] = 0x00,
	[GB_REG_NR43] = 0x00,
	[GB_REG_NR44] = 0xBF,
	[GB_REG_NR50] = 0x00,
	[GB_REG_NR51] = 0x00,
	[GB_REG_NR52] = 0x70,
	[GB_REG_STAT] = 0x80,
	[GB_REG_KEY1] = 0x7E,
	[GB_REG_VBK]  = 0xFE,
	[GB_REG_OCPS] = 0x40,
	[GB_REG_BCPS] = 0x40,
	[GB_REG_OPRI] = 0xFE,
	[GB_REG_SVBK] = 0xF8,
	[GB_REG_UNK75] = 0x8F,
	[GB_REG_IE]   = 0xE0,
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
	if (bits & 2) {
		if (gb->sgbIncrement) {
			gb->sgbIncrement = false;
			gb->sgbCurrentController = (gb->sgbCurrentController + 1) & gb->sgbControllers;
		}
	} else {
		if (gb->currentSgbBits & 2) {
			gb->sgbIncrement = !gb->sgbIncrement;
		}
	}
	gb->currentSgbBits = bits;
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

	GBIOWrite(gb, GB_REG_TIMA, 0);
	GBIOWrite(gb, GB_REG_TMA, 0);
	GBIOWrite(gb, GB_REG_TAC, 0);
	GBIOWrite(gb, GB_REG_IF, 1);
	GBIOWrite(gb, GB_REG_LCDC, 0x00);
	GBIOWrite(gb, GB_REG_SCY, 0x00);
	GBIOWrite(gb, GB_REG_SCX, 0x00);
	GBIOWrite(gb, GB_REG_LYC, 0x00);
	gb->memory.io[GB_REG_DMA] = 0xFF;
	GBIOWrite(gb, GB_REG_BGP, 0xFC);
	if (gb->model < GB_MODEL_CGB) {
		GBIOWrite(gb, GB_REG_OBP0, 0xFF);
		GBIOWrite(gb, GB_REG_OBP1, 0xFF);
	}
	GBIOWrite(gb, GB_REG_WY, 0x00);
	GBIOWrite(gb, GB_REG_WX, 0x00);
	gb->memory.io[GB_REG_BANK] = 0xFF;
	if (gb->model & GB_MODEL_CGB) {
		GBIOWrite(gb, GB_REG_KEY0, 0);
		GBIOWrite(gb, GB_REG_JOYP, 0xFF);
		GBIOWrite(gb, GB_REG_VBK, 0);
		GBIOWrite(gb, GB_REG_BCPS, 0x80);
		GBIOWrite(gb, GB_REG_OCPS, 0);
		GBIOWrite(gb, GB_REG_SVBK, 1);
		GBIOWrite(gb, GB_REG_HDMA1, 0xFF);
		GBIOWrite(gb, GB_REG_HDMA2, 0xFF);
		GBIOWrite(gb, GB_REG_HDMA3, 0xFF);
		GBIOWrite(gb, GB_REG_HDMA4, 0xFF);
		gb->memory.io[GB_REG_HDMA5] = 0xFF;
	} else {
		memset(&gb->memory.io[GB_REG_KEY0], 0xFF, GB_REG_PCM34 - GB_REG_KEY0 + 1);
	}

	if (gb->model & GB_MODEL_SGB) {
		GBIOWrite(gb, GB_REG_JOYP, 0xFF);
	}
	GBIOWrite(gb, GB_REG_IE, 0x00);
}

void GBIOWrite(struct GB* gb, unsigned address, uint8_t value) {
	switch (address) {
	case GB_REG_SB:
		GBSIOWriteSB(&gb->sio, value);
		break;
	case GB_REG_SC:
		GBSIOWriteSC(&gb->sio, value);
		break;
	case GB_REG_DIV:
		GBTimerDivReset(&gb->timer);
		return;
	case GB_REG_NR10:
		if (gb->audio.enable) {
			GBAudioWriteNR10(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case GB_REG_NR11:
		if (gb->audio.enable) {
			GBAudioWriteNR11(&gb->audio, value);
		} else {
			if (gb->audio.style == GB_AUDIO_DMG) {
				GBAudioWriteNR11(&gb->audio, value & _registerMask[GB_REG_NR11]);
			}
			value = 0;
		}
		break;
	case GB_REG_NR12:
		if (gb->audio.enable) {
			GBAudioWriteNR12(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case GB_REG_NR13:
		if (gb->audio.enable) {
			GBAudioWriteNR13(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case GB_REG_NR14:
		if (gb->audio.enable) {
			GBAudioWriteNR14(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case GB_REG_NR21:
		if (gb->audio.enable) {
			GBAudioWriteNR21(&gb->audio, value);
		} else {
			if (gb->audio.style == GB_AUDIO_DMG) {
				GBAudioWriteNR21(&gb->audio, value & _registerMask[GB_REG_NR21]);
			}
			value = 0;
		}
		break;
	case GB_REG_NR22:
		if (gb->audio.enable) {
			GBAudioWriteNR22(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case GB_REG_NR23:
		if (gb->audio.enable) {
			GBAudioWriteNR23(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case GB_REG_NR24:
		if (gb->audio.enable) {
			GBAudioWriteNR24(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case GB_REG_NR30:
		if (gb->audio.enable) {
			GBAudioWriteNR30(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case GB_REG_NR31:
		if (gb->audio.enable || gb->audio.style == GB_AUDIO_DMG) {
			GBAudioWriteNR31(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case GB_REG_NR32:
		if (gb->audio.enable) {
			GBAudioWriteNR32(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case GB_REG_NR33:
		if (gb->audio.enable) {
			GBAudioWriteNR33(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case GB_REG_NR34:
		if (gb->audio.enable) {
			GBAudioWriteNR34(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case GB_REG_NR41:
		if (gb->audio.enable || gb->audio.style == GB_AUDIO_DMG) {
			GBAudioWriteNR41(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case GB_REG_NR42:
		if (gb->audio.enable) {
			GBAudioWriteNR42(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case GB_REG_NR43:
		if (gb->audio.enable) {
			GBAudioWriteNR43(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case GB_REG_NR44:
		if (gb->audio.enable) {
			GBAudioWriteNR44(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case GB_REG_NR50:
		if (gb->audio.enable) {
			GBAudioWriteNR50(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case GB_REG_NR51:
		if (gb->audio.enable) {
			GBAudioWriteNR51(&gb->audio, value);
		} else {
			value = 0;
		}
		break;
	case GB_REG_NR52:
		GBAudioWriteNR52(&gb->audio, value);
		value &= 0x80;
		value |= gb->memory.io[GB_REG_NR52] & 0x0F;
		break;
	case GB_REG_WAVE_0:
	case GB_REG_WAVE_1:
	case GB_REG_WAVE_2:
	case GB_REG_WAVE_3:
	case GB_REG_WAVE_4:
	case GB_REG_WAVE_5:
	case GB_REG_WAVE_6:
	case GB_REG_WAVE_7:
	case GB_REG_WAVE_8:
	case GB_REG_WAVE_9:
	case GB_REG_WAVE_A:
	case GB_REG_WAVE_B:
	case GB_REG_WAVE_C:
	case GB_REG_WAVE_D:
	case GB_REG_WAVE_E:
	case GB_REG_WAVE_F:
		GBAudioRun(&gb->audio, mTimingCurrentTime(gb->audio.timing), 0x4);
		if (!gb->audio.playingCh3) {
			gb->audio.ch3.wavedata8[address - GB_REG_WAVE_0] = value;
		} else if (gb->audio.ch3.readable || gb->audio.style == GB_AUDIO_CGB) {
			gb->audio.ch3.wavedata8[gb->audio.ch3.window >> 1] = value;
		}
		break;
	case GB_REG_JOYP:
		gb->memory.io[GB_REG_JOYP] = value | 0x0F;
		_readKeys(gb);
		if (gb->model & GB_MODEL_SGB) {
			_writeSGBBits(gb, (value >> 4) & 3);
		}
		return;
	case GB_REG_TIMA:
		if (value && mTimingUntil(&gb->timing, &gb->timer.irq) > 2 - (int) gb->doubleSpeed) {
			mTimingDeschedule(&gb->timing, &gb->timer.irq);
		}
		if (mTimingUntil(&gb->timing, &gb->timer.irq) == (int) gb->doubleSpeed - 2) {
			return;
		}
		break;
	case GB_REG_TMA:
		if (mTimingUntil(&gb->timing, &gb->timer.irq) == (int) gb->doubleSpeed - 2) {
			gb->memory.io[GB_REG_TIMA] = value;
		}
		break;
	case GB_REG_TAC:
		value = GBTimerUpdateTAC(&gb->timer, value);
		break;
	case GB_REG_IF:
		gb->memory.io[GB_REG_IF] = value | 0xE0;
		GBUpdateIRQs(gb);
		return;
	case GB_REG_LCDC:
		// TODO: handle GBC differences
		GBVideoProcessDots(&gb->video, 0);
		value = gb->video.renderer->writeVideoRegister(gb->video.renderer, address, value);
		GBVideoWriteLCDC(&gb->video, value);
		break;
	case GB_REG_LYC:
		GBVideoWriteLYC(&gb->video, value);
		break;
	case GB_REG_DMA:
		GBMemoryDMA(gb, value << 8);
		break;
	case GB_REG_SCY:
	case GB_REG_SCX:
	case GB_REG_WY:
	case GB_REG_WX:
		GBVideoProcessDots(&gb->video, 0);
		value = gb->video.renderer->writeVideoRegister(gb->video.renderer, address, value);
		break;
	case GB_REG_BGP:
	case GB_REG_OBP0:
	case GB_REG_OBP1:
		GBVideoProcessDots(&gb->video, 0);
		GBVideoWritePalette(&gb->video, address, value);
		break;
	case GB_REG_STAT:
		GBVideoWriteSTAT(&gb->video, value);
		value = gb->video.stat;
		break;
	case GB_REG_BANK:
		if (gb->memory.io[GB_REG_BANK] != 0xFF) {
			break;
		}
		GBUnmapBIOS(gb);
		if (gb->model >= GB_MODEL_CGB && gb->memory.io[GB_REG_KEY0] < 0x80) {
			gb->model = GB_MODEL_DMG;
			GBVideoDisableCGB(&gb->video);
		}
		break;
	case GB_REG_IE:
		gb->memory.ie = value;
		GBUpdateIRQs(gb);
		return;
	default:
		if (gb->model >= GB_MODEL_CGB) {
			switch (address) {
			case GB_REG_KEY0:
				break;
			case GB_REG_KEY1:
				value &= 0x1;
				value |= gb->memory.io[address] & 0x80;
				break;
			case GB_REG_VBK:
				GBVideoSwitchBank(&gb->video, value);
				break;
			case GB_REG_HDMA1:
			case GB_REG_HDMA2:
			case GB_REG_HDMA3:
			case GB_REG_HDMA4:
				// Handled transparently by the registers
				break;
			case GB_REG_HDMA5:
				value = GBMemoryWriteHDMA5(gb, value);
				break;
			case GB_REG_BCPS:
				gb->video.bcpIndex = value & 0x3F;
				gb->video.bcpIncrement = value & 0x80;
				gb->memory.io[GB_REG_BCPD] = gb->video.palette[gb->video.bcpIndex >> 1] >> (8 * (gb->video.bcpIndex & 1));
				break;
			case GB_REG_BCPD:
				GBVideoWritePalette(&gb->video, address, value);
				return;
			case GB_REG_OCPS:
				gb->video.ocpIndex = value & 0x3F;
				gb->video.ocpIncrement = value & 0x80;
				gb->memory.io[GB_REG_OCPD] = gb->video.palette[8 * 4 + (gb->video.ocpIndex >> 1)] >> (8 * (gb->video.ocpIndex & 1));
				break;
			case GB_REG_OCPD:
				GBVideoWritePalette(&gb->video, address, value);
				return;
			case GB_REG_SVBK:
				GBMemorySwitchWramBank(&gb->memory, value);
				value = gb->memory.wramCurrentBank;
				break;
			default:
				goto failed;
			}
			goto success;
		}
		failed:
		mLOG(GB_IO, GAME_ERROR, "Writing to unknown register FF%02X:%02X", address, value);
		return;
	}
	success:
	gb->memory.io[address] = value;
}

static uint8_t _readKeys(struct GB* gb) {
	uint8_t keys = *gb->keySource;
	if (gb->sgbCurrentController != 0) {
		keys = 0;
	}
	uint8_t joyp = gb->memory.io[GB_REG_JOYP];
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
	gb->memory.io[GB_REG_JOYP] = (0xCF | joyp) ^ (keys & 0xF);
	if (joyp & ~gb->memory.io[GB_REG_JOYP] & 0xF) {
		gb->memory.io[GB_REG_IF] |= (1 << GB_IRQ_KEYPAD);
		GBUpdateIRQs(gb);
	}
	return gb->memory.io[GB_REG_JOYP];
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
	case GB_REG_JOYP:
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
	case GB_REG_IE:
		return gb->memory.ie;
	case GB_REG_WAVE_0:
	case GB_REG_WAVE_1:
	case GB_REG_WAVE_2:
	case GB_REG_WAVE_3:
	case GB_REG_WAVE_4:
	case GB_REG_WAVE_5:
	case GB_REG_WAVE_6:
	case GB_REG_WAVE_7:
	case GB_REG_WAVE_8:
	case GB_REG_WAVE_9:
	case GB_REG_WAVE_A:
	case GB_REG_WAVE_B:
	case GB_REG_WAVE_C:
	case GB_REG_WAVE_D:
	case GB_REG_WAVE_E:
	case GB_REG_WAVE_F:
		if (gb->audio.playingCh3) {
			GBAudioRun(&gb->audio, mTimingCurrentTime(gb->audio.timing), 0x4);
			if (gb->audio.ch3.readable || gb->audio.style == GB_AUDIO_CGB) {
				return gb->audio.ch3.wavedata8[gb->audio.ch3.window >> 1];
			} else {
				return 0xFF;
			}
		} else {
			return gb->audio.ch3.wavedata8[address - GB_REG_WAVE_0];
		}
		break;
	case GB_REG_PCM12:
		if (gb->model < GB_MODEL_CGB) {
			mLOG(GB_IO, GAME_ERROR, "Reading from CGB register FF%02X in DMG mode", address);
		} else if (gb->audio.enable) {
			GBAudioRun(&gb->audio, mTimingCurrentTime(gb->audio.timing), 0x3);
			return (gb->audio.ch1.sample) | (gb->audio.ch2.sample << 4);
		}
		break;
	case GB_REG_PCM34:
		if (gb->model < GB_MODEL_CGB) {
			mLOG(GB_IO, GAME_ERROR, "Reading from CGB register FF%02X in DMG mode", address);
		} else if (gb->audio.enable) {
			GBAudioRun(&gb->audio, mTimingCurrentTime(gb->audio.timing), 0xC);
			return (gb->audio.ch3.sample) | (gb->audio.ch4.sample << 4);
		}
		break;
	case GB_REG_SB:
	case GB_REG_SC:
	case GB_REG_IF:
	case GB_REG_NR10:
	case GB_REG_NR11:
	case GB_REG_NR12:
	case GB_REG_NR14:
	case GB_REG_NR21:
	case GB_REG_NR22:
	case GB_REG_NR24:
	case GB_REG_NR30:
	case GB_REG_NR32:
	case GB_REG_NR34:
	case GB_REG_NR41:
	case GB_REG_NR42:
	case GB_REG_NR43:
	case GB_REG_NR44:
	case GB_REG_NR50:
	case GB_REG_NR51:
	case GB_REG_NR52:
	case GB_REG_DIV:
	case GB_REG_TIMA:
	case GB_REG_TMA:
	case GB_REG_TAC:
	case GB_REG_STAT:
	case GB_REG_LCDC:
	case GB_REG_SCY:
	case GB_REG_SCX:
	case GB_REG_LY:
	case GB_REG_LYC:
	case GB_REG_DMA:
	case GB_REG_BGP:
	case GB_REG_OBP0:
	case GB_REG_OBP1:
	case GB_REG_WY:
	case GB_REG_WX:
		// Handled transparently by the registers
		break;
	case GB_REG_KEY1:
	case GB_REG_VBK:
	case GB_REG_HDMA1:
	case GB_REG_HDMA2:
	case GB_REG_HDMA3:
	case GB_REG_HDMA4:
	case GB_REG_HDMA5:
	case GB_REG_BCPS:
	case GB_REG_BCPD:
	case GB_REG_OCPS:
	case GB_REG_OCPD:
	case GB_REG_SVBK:
	case GB_REG_PSWX:
	case GB_REG_PSWY:
	case GB_REG_UNK75:
		// Handled transparently by the registers
		if (gb->model < GB_MODEL_CGB) {
			// In DMG mode, these all get initialized to 0xFF during reset
			// But in DMG-on-CGB mode, they get initialized by the CGB reset so they can be non-zero
			mLOG(GB_IO, GAME_ERROR, "Reading from CGB register FF%02X in DMG mode", address);
		}
		break;
	default:
		mLOG(GB_IO, STUB, "Reading from unknown register FF%02X", address);
		return 0xFF;
	}
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

	gb->audio.enable = GBAudioEnableGetEnable(*gb->audio.nr52);
	if (gb->audio.enable) {
		gb->audio.playingCh1 = false;
		GBIOWrite(gb, GB_REG_NR10, gb->memory.io[GB_REG_NR10]);
		GBIOWrite(gb, GB_REG_NR11, gb->memory.io[GB_REG_NR11]);
		GBIOWrite(gb, GB_REG_NR12, gb->memory.io[GB_REG_NR12]);
		GBIOWrite(gb, GB_REG_NR13, gb->memory.io[GB_REG_NR13]);
		gb->audio.ch1.control.frequency &= 0xFF;
		gb->audio.ch1.control.frequency |= GBAudioRegisterControlGetFrequency(gb->memory.io[GB_REG_NR14] << 8);
		gb->audio.ch1.control.stop = GBAudioRegisterControlGetStop(gb->memory.io[GB_REG_NR14] << 8);
		gb->audio.playingCh2 = false;
		GBIOWrite(gb, GB_REG_NR21, gb->memory.io[GB_REG_NR21]);
		GBIOWrite(gb, GB_REG_NR22, gb->memory.io[GB_REG_NR22]);
		GBIOWrite(gb, GB_REG_NR23, gb->memory.io[GB_REG_NR23]);
		gb->audio.ch2.control.frequency &= 0xFF;
		gb->audio.ch2.control.frequency |= GBAudioRegisterControlGetFrequency(gb->memory.io[GB_REG_NR24] << 8);
		gb->audio.ch2.control.stop = GBAudioRegisterControlGetStop(gb->memory.io[GB_REG_NR24] << 8);
		gb->audio.playingCh3 = false;
		GBIOWrite(gb, GB_REG_NR30, gb->memory.io[GB_REG_NR30]);
		GBIOWrite(gb, GB_REG_NR31, gb->memory.io[GB_REG_NR31]);
		GBIOWrite(gb, GB_REG_NR32, gb->memory.io[GB_REG_NR32]);
		GBIOWrite(gb, GB_REG_NR33, gb->memory.io[GB_REG_NR33]);
		gb->audio.ch3.rate &= 0xFF;
		gb->audio.ch3.rate |= GBAudioRegisterControlGetRate(gb->memory.io[GB_REG_NR34] << 8);
		gb->audio.ch3.stop = GBAudioRegisterControlGetStop(gb->memory.io[GB_REG_NR34] << 8);
		gb->audio.playingCh4 = false;
		GBIOWrite(gb, GB_REG_NR41, gb->memory.io[GB_REG_NR41]);
		GBIOWrite(gb, GB_REG_NR42, gb->memory.io[GB_REG_NR42]);
		GBIOWrite(gb, GB_REG_NR43, gb->memory.io[GB_REG_NR43]);
		gb->audio.ch4.stop = GBAudioRegisterNoiseControlGetStop(gb->memory.io[GB_REG_NR44]);
		GBIOWrite(gb, GB_REG_NR50, gb->memory.io[GB_REG_NR50]);
		GBIOWrite(gb, GB_REG_NR51, gb->memory.io[GB_REG_NR51]);
	}

	gb->video.renderer->writeVideoRegister(gb->video.renderer, GB_REG_LCDC, state->io[GB_REG_LCDC]);
	gb->video.renderer->writeVideoRegister(gb->video.renderer, GB_REG_SCY, state->io[GB_REG_SCY]);
	gb->video.renderer->writeVideoRegister(gb->video.renderer, GB_REG_SCX, state->io[GB_REG_SCX]);
	gb->video.renderer->writeVideoRegister(gb->video.renderer, GB_REG_WY, state->io[GB_REG_WY]);
	gb->video.renderer->writeVideoRegister(gb->video.renderer, GB_REG_WX, state->io[GB_REG_WX]);
	if (gb->model == GB_MODEL_SGB) {
		gb->video.renderer->writeVideoRegister(gb->video.renderer, GB_REG_BGP, state->io[GB_REG_BGP]);
		gb->video.renderer->writeVideoRegister(gb->video.renderer, GB_REG_OBP0, state->io[GB_REG_OBP0]);
		gb->video.renderer->writeVideoRegister(gb->video.renderer, GB_REG_OBP1, state->io[GB_REG_OBP1]);
	}
	gb->video.stat = state->io[GB_REG_STAT];
}
