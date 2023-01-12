/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gb/mbc/mbc-private.h"

#include <mgba/internal/defines.h>
#include <mgba/internal/gb/gb.h>

static void _GBPocketCamCapture(struct GBMemory*);

void _GBPocketCam(struct GB* gb, uint16_t address, uint8_t value) {
	struct GBMemory* memory = &gb->memory;
	int bank = value & 0x3F;
	switch (address >> 13) {
	case 0x0:
		switch (value) {
		case 0:
			memory->sramAccess = false;
			break;
		case 0xA:
			memory->sramAccess = true;
			GBMBCSwitchSramBank(gb, memory->sramCurrentBank);
			break;
		default:
			// TODO
			mLOG(GB_MBC, STUB, "Pocket Cam unknown value %02X", value);
			break;
		}
		break;
	case 0x1:
		GBMBCSwitchBank(gb, bank);
		break;
	case 0x2:
		if (value < 0x10) {
			GBMBCSwitchSramBank(gb, value);
			memory->mbcState.pocketCam.registersActive = false;
			memory->directSramAccess = true;
		} else {
			memory->mbcState.pocketCam.registersActive = true;
			memory->directSramAccess = false;
		}
		break;
	case 0x5:
		if (!memory->mbcState.pocketCam.registersActive) {
			break;
		}
		address &= 0x7F;
		if (address == 0 && value & 1) {
			value &= 6; // TODO: Timing
			gb->sramDirty |= mSAVEDATA_DIRT_NEW;
			_GBPocketCamCapture(memory);
		}
		if (address < sizeof(memory->mbcState.pocketCam.registers)) {
			memory->mbcState.pocketCam.registers[address] = value;
		}
		break;
	default:
		mLOG(GB_MBC, STUB, "Pocket Cam unknown address: %04X:%02X", address, value);
		break;
	}
}

uint8_t _GBPocketCamRead(struct GBMemory* memory, uint16_t address) {
	if (memory->mbcState.pocketCam.registersActive) {
		if ((address & 0x7F) == 0) {
			return memory->mbcState.pocketCam.registers[0];
		}
		return 0;
	}
	return memory->sramBank[address & (GB_SIZE_EXTERNAL_RAM - 1)];
}

void _GBPocketCamCapture(struct GBMemory* memory) {
	if (!memory->cam) {
		return;
	}
	const void* image = NULL;
	size_t stride;
	enum mColorFormat format;
	memory->cam->requestImage(memory->cam, &image, &stride, &format);
	if (!image) {
		return;
	}
	memset(&memory->sram[0x100], 0, GBCAM_HEIGHT * GBCAM_WIDTH / 4);
	struct GBPocketCamState* pocketCam = &memory->mbcState.pocketCam;
	size_t x, y;
	for (y = 0; y < GBCAM_HEIGHT; ++y) {
		for (x = 0; x < GBCAM_WIDTH; ++x) {
			uint32_t gray;
			uint32_t color;
			switch (format) {
			case mCOLOR_XBGR8:
			case mCOLOR_XRGB8:
			case mCOLOR_ARGB8:
			case mCOLOR_ABGR8:
				color = ((const uint32_t*) image)[y * stride + x];
				gray = (color & 0xFF) + ((color >> 8) & 0xFF) + ((color >> 16) & 0xFF);
				break;
			case mCOLOR_BGRX8:
			case mCOLOR_RGBX8:
			case mCOLOR_RGBA8:
			case mCOLOR_BGRA8:
				color = ((const uint32_t*) image)[y * stride + x];
				gray = ((color >> 8) & 0xFF) + ((color >> 16) & 0xFF) + ((color >> 24) & 0xFF);
				break;
			case mCOLOR_BGR5:
			case mCOLOR_RGB5:
			case mCOLOR_ARGB5:
			case mCOLOR_ABGR5:
				color = ((const uint16_t*) image)[y * stride + x];
				gray = ((color << 3) & 0xF8) + ((color >> 2) & 0xF8) + ((color >> 7) & 0xF8);
				break;
			case mCOLOR_BGR565:
			case mCOLOR_RGB565:
				color = ((const uint16_t*) image)[y * stride + x];
				gray = ((color << 3) & 0xF8) + ((color >> 3) & 0xFC) + ((color >> 8) & 0xF8);
				break;
			case mCOLOR_BGRA5:
			case mCOLOR_RGBA5:
				color = ((const uint16_t*) image)[y * stride + x];
				gray = ((color << 2) & 0xF8) + ((color >> 3) & 0xF8) + ((color >> 8) & 0xF8);
				break;
			default:
				mLOG(GB_MBC, WARN, "Unsupported pixel format: %X", format);
				return;
			}
			uint16_t exposure = (pocketCam->registers[2] << 8) | (pocketCam->registers[3]);
			gray = (gray + 1) * exposure / 0x300;
			// TODO: Additional processing
			int matrixEntry = 3 * ((x & 3) + 4 * (y & 3));
			if (gray < pocketCam->registers[matrixEntry + 6]) {
				gray = 0x101;
			} else if (gray < pocketCam->registers[matrixEntry + 7]) {
				gray = 0x100;
			} else if (gray < pocketCam->registers[matrixEntry + 8]) {
				gray = 0x001;
			} else {
				gray = 0;
			}
			int coord = (((x >> 3) & 0xF) * 8 + (y & 0x7)) * 2 + (y & ~0x7) * 0x20;
			uint16_t existing;
			LOAD_16LE(existing, coord + 0x100, memory->sram);
			existing |= gray << (7 - (x & 7));
			STORE_16LE(existing, coord + 0x100, memory->sram);
		}
	}
}
