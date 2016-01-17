/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "io.h"

#include "gb/gb.h"

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
	case REG_IF:
		gb->memory.io[REG_IF] = value;
		GBUpdateIRQs(gb);
		return;
	case REG_LCDC:
		// TODO: handle GBC differences
		GBVideoWriteLCDC(&gb->video, value);
		break;
	case REG_STAT:
		GBVideoWriteSTAT(&gb->video, value);
		break;
	case REG_IE:
		gb->memory.ie = value;
		GBUpdateIRQs(gb);
		return;
	default:
		// TODO: Log
		if (address >= GB_SIZE_IO) {
			return;
		}
		break;
	}
	gb->memory.io[address] = value;
}

uint8_t GBIORead(struct GB* gb, unsigned address) {
	switch (address) {
	case REG_IF:
		break;
	case REG_IE:
		return gb->memory.ie;
	default:
		// TODO: Log
		if (address >= GB_SIZE_IO) {
			return 0;
		}
		break;
	}
	return gb->memory.io[address];
}
