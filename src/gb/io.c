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
	case REG_IE:
		gb->memory.ie = value;
		GBUpdateIRQs(gb);
		break;
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
