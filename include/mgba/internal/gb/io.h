/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_IO_H
#define GB_IO_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>

mLOG_DECLARE_CATEGORY(GB_IO);

enum GBIORegisters {
	GB_REG_JOYP = 0x00,
	GB_REG_SB = 0x01,
	GB_REG_SC = 0x02,

	// Timing
	GB_REG_DIV = 0x04,
	GB_REG_TIMA = 0x05,
	GB_REG_TMA = 0x06,
	GB_REG_TAC = 0x07,

	// Interrupts
	GB_REG_IF = 0x0F,
	GB_REG_IE = 0xFF,

	// Audio
	GB_REG_NR10 = 0x10,
	GB_REG_NR11 = 0x11,
	GB_REG_NR12 = 0x12,
	GB_REG_NR13 = 0x13,
	GB_REG_NR14 = 0x14,
	GB_REG_NR21 = 0x16,
	GB_REG_NR22 = 0x17,
	GB_REG_NR23 = 0x18,
	GB_REG_NR24 = 0x19,
	GB_REG_NR30 = 0x1A,
	GB_REG_NR31 = 0x1B,
	GB_REG_NR32 = 0x1C,
	GB_REG_NR33 = 0x1D,
	GB_REG_NR34 = 0x1E,
	GB_REG_NR41 = 0x20,
	GB_REG_NR42 = 0x21,
	GB_REG_NR43 = 0x22,
	GB_REG_NR44 = 0x23,
	GB_REG_NR50 = 0x24,
	GB_REG_NR51 = 0x25,
	GB_REG_NR52 = 0x26,

	GB_REG_WAVE_0 = 0x30,
	GB_REG_WAVE_1 = 0x31,
	GB_REG_WAVE_2 = 0x32,
	GB_REG_WAVE_3 = 0x33,
	GB_REG_WAVE_4 = 0x34,
	GB_REG_WAVE_5 = 0x35,
	GB_REG_WAVE_6 = 0x36,
	GB_REG_WAVE_7 = 0x37,
	GB_REG_WAVE_8 = 0x38,
	GB_REG_WAVE_9 = 0x39,
	GB_REG_WAVE_A = 0x3A,
	GB_REG_WAVE_B = 0x3B,
	GB_REG_WAVE_C = 0x3C,
	GB_REG_WAVE_D = 0x3D,
	GB_REG_WAVE_E = 0x3E,
	GB_REG_WAVE_F = 0x3F,

	// Video
	GB_REG_LCDC = 0x40,
	GB_REG_STAT = 0x41,
	GB_REG_SCY = 0x42,
	GB_REG_SCX = 0x43,
	GB_REG_LY = 0x44,
	GB_REG_LYC = 0x45,
	GB_REG_DMA = 0x46,
	GB_REG_BGP = 0x47,
	GB_REG_OBP0 = 0x48,
	GB_REG_OBP1 = 0x49,
	GB_REG_WY = 0x4A,
	GB_REG_WX = 0x4B,

	// CGB
	GB_REG_KEY0 = 0x4C,
	GB_REG_KEY1 = 0x4D,
	GB_REG_VBK = 0x4F,
	GB_REG_BANK = 0x50,
	GB_REG_HDMA1 = 0x51,
	GB_REG_HDMA2 = 0x52,
	GB_REG_HDMA3 = 0x53,
	GB_REG_HDMA4 = 0x54,
	GB_REG_HDMA5 = 0x55,
	GB_REG_RP = 0x56,
	GB_REG_BCPS = 0x68,
	GB_REG_BCPD = 0x69,
	GB_REG_OCPS = 0x6A,
	GB_REG_OCPD = 0x6B,
	GB_REG_OPRI = 0x6C,
	GB_REG_SVBK = 0x70,
	GB_REG_PSWX = 0x72,
	GB_REG_PSWY = 0x73,
	GB_REG_PSW = 0x74,
	GB_REG_UNK75 = 0x75,
	GB_REG_PCM12 = 0x76,
	GB_REG_PCM34 = 0x77,
	GB_REG_MAX = 0x100
};

extern MGBA_EXPORT const char* const GBIORegisterNames[];

struct GB;
void GBIOInit(struct GB* gb);
void GBIOReset(struct GB* gb);

void GBIOWrite(struct GB* gb, unsigned address, uint8_t value);
uint8_t GBIORead(struct GB* gb, unsigned address);

struct GBSerializedState;
void GBIOSerialize(const struct GB* gb, struct GBSerializedState* state);
void GBIODeserialize(struct GB* gb, const struct GBSerializedState* state);

CXX_GUARD_END

#endif
