/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_IO_H
#define GB_IO_H

#include "util/common.h"

enum GBIORegisters {
	REG_JOYP = 0x00,
	REG_SB = 0x01,
	REG_SC = 0x02,

	// Timing
	REG_DIV = 0x04,
	REG_TIMA = 0x05,
	REG_TMA = 0x06,
	REG_TAC = 0x07,

	// Interrupts
	REG_IF = 0x0F,
	REG_IE = 0xFF,

	// Audio
	REG_NR10 = 0x10,
	REG_NR11 = 0x11,
	REG_NR12 = 0x12,
	REG_NR13 = 0x13,
	REG_NR14 = 0x14,
	REG_NR21 = 0x16,
	REG_NR22 = 0x17,
	REG_NR23 = 0x18,
	REG_NR24 = 0x19,
	REG_NR30 = 0x1A,
	REG_NR31 = 0x1B,
	REG_NR32 = 0x1C,
	REG_NR33 = 0x1D,
	REG_NR34 = 0x1E,
	REG_NR41 = 0x20,
	REG_NR42 = 0x21,
	REG_NR43 = 0x22,
	REG_NR44 = 0x23,
	REG_NR50 = 0x24,
	REG_NR51 = 0x25,
	REG_NR52 = 0x26,

	REG_WAVE_0 = 0x30,
	REG_WAVE_1 = 0x31,
	REG_WAVE_2 = 0x32,
	REG_WAVE_3 = 0x33,
	REG_WAVE_4 = 0x34,
	REG_WAVE_5 = 0x35,
	REG_WAVE_6 = 0x36,
	REG_WAVE_7 = 0x37,
	REG_WAVE_8 = 0x38,
	REG_WAVE_9 = 0x39,
	REG_WAVE_A = 0x3A,
	REG_WAVE_B = 0x3B,
	REG_WAVE_C = 0x3C,
	REG_WAVE_D = 0x3D,
	REG_WAVE_E = 0x3E,
	REG_WAVE_F = 0x3F,

	// Video
	REG_LCDC = 0x40,
	REG_STAT = 0x41,
	REG_SCY = 0x42,
	REG_SCX = 0x43,
	REG_LY = 0x44,
	REG_LYC = 0x45,
	REG_DMA = 0x46,
	REG_BGP = 0x47,
	REG_OBP0 = 0x48,
	REG_OBP1 = 0x49,
	REG_WY = 0x4A,
	REG_WX = 0x4B,
};

struct GB;
void GBIOInit(struct GB* gb);
void GBIOReset(struct GB* gb);

void GBIOWrite(struct GB* gb, unsigned address, uint8_t value);
uint8_t GBIORead(struct GB* gb, unsigned address);

#endif
