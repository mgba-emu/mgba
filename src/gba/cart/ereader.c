
/* Copyright (c) 2013-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/cart/ereader.h>

#include <mgba/internal/arm/macros.h>
#include <mgba/internal/gba/gba.h>
#include <mgba-util/memory.h>

#ifdef USE_FFMPEG
#include <mgba-util/convolve.h>
#ifdef USE_PNG
#include <mgba-util/image/png-io.h>
#include <mgba-util/vfs.h>
#endif

#include "feature/ffmpeg/ffmpeg-scale.h"
#endif

#define EREADER_BLOCK_SIZE 40

static void _eReaderReset(struct GBACartEReader* ereader);
static void _eReaderWriteControl0(struct GBACartEReader* ereader, uint8_t value);
static void _eReaderWriteControl1(struct GBACartEReader* ereader, uint8_t value);
static void _eReaderReadData(struct GBACartEReader* ereader);
static void _eReaderReedSolomon(const uint8_t* input, uint8_t* output);
static void _eReaderScanCard(struct GBACartEReader* ereader);

const int EREADER_NYBBLE_5BIT[16][5] = {
	{ 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 1 },
	{ 0, 0, 0, 1, 0 },
	{ 1, 0, 0, 1, 0 },
	{ 0, 0, 1, 0, 0 },
	{ 0, 0, 1, 0, 1 },
	{ 0, 0, 1, 1, 0 },
	{ 1, 0, 1, 1, 0 },
	{ 0, 1, 0, 0, 0 },
	{ 0, 1, 0, 0, 1 },
	{ 0, 1, 0, 1, 0 },
	{ 1, 0, 1, 0, 0 },
	{ 0, 1, 1, 0, 0 },
	{ 0, 1, 1, 0, 1 },
	{ 1, 0, 0, 0, 1 },
	{ 1, 0, 0, 0, 0 }
};

const int EREADER_NYBBLE_LOOKUP[32] = {
	 0,  1,  2, -1,  4,  5,  6, -1,
	 8,  9, 10, -1, 12, 13, -1, -1,
	15, 14,  3, -1, 11, -1,  7, -1,
	-1, -1, -1, -1, -1, -1, -1, -1
};

const uint8_t EREADER_CALIBRATION_TEMPLATE[] = {
	0x43, 0x61, 0x72, 0x64, 0x2d, 0x45, 0x20, 0x52, 0x65, 0x61, 0x64, 0x65, 0x72, 0x20, 0x32, 0x30,
	0x30, 0x31, 0x00, 0x00, 0xcf, 0x72, 0x2f, 0x37, 0x3a, 0x3a, 0x3a, 0x38, 0x33, 0x30, 0x30, 0x37,
	0x3a, 0x39, 0x37, 0x35, 0x33, 0x2f, 0x2f, 0x34, 0x36, 0x36, 0x37, 0x36, 0x34, 0x31, 0x2d, 0x30,
	0x32, 0x34, 0x35, 0x35, 0x34, 0x30, 0x2a, 0x2d, 0x2d, 0x2f, 0x31, 0x32, 0x31, 0x2f, 0x29, 0x2a,
	0x2c, 0x2b, 0x2c, 0x2e, 0x2e, 0x2d, 0x18, 0x2d, 0x8f, 0x03, 0x00, 0x00, 0xc0, 0xfd, 0x77, 0x00,
	0x00, 0x00, 0x01
};

const uint16_t EREADER_ADDRESS_CODES[] = {
	1023,
	1174,
	2628,
	3373,
	4233,
	6112,
	6450,
	7771,
	8826,
	9491,
	11201,
	11432,
	12556,
	13925,
	14519,
	16350,
	16629,
	18332,
	18766,
	20007,
	21379,
	21738,
	23096,
	23889,
	24944,
	26137,
	26827,
	28578,
	29190,
	30063,
	31677,
	31956,
	33410,
	34283,
	35641,
	35920,
	37364,
	38557,
	38991,
	40742,
	41735,
	42094,
	43708,
	44501,
	45169,
	46872,
	47562,
	48803,
	49544,
	50913,
	51251,
	53082,
	54014,
	54679
};

static const uint8_t DUMMY_HEADER_STRIP[2][0x10] = {
	{ 0x00, 0x30, 0x01, 0x01, 0x00, 0x01, 0x05, 0x10, 0x00, 0x00, 0x10, 0x13, 0x00, 0x00, 0x02, 0x00 },
	{ 0x00, 0x30, 0x01, 0x02, 0x00, 0x01, 0x08, 0x10, 0x00, 0x00, 0x10, 0x12, 0x00, 0x00, 0x01, 0x00 }
};

static const uint8_t DUMMY_HEADER_FIXED[0x16] = {
	0x00, 0x00, 0x10, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x08, 0x4e, 0x49, 0x4e, 0x54, 0x45, 0x4e,
	0x44, 0x4f, 0x00, 0x22, 0x00, 0x09
};

static const uint8_t BLOCK_HEADER[2][0x18] = {
	{ 0x00, 0x02, 0x00, 0x01, 0x40, 0x10, 0x00, 0x1c, 0x10, 0x6f, 0x40, 0xda, 0x39, 0x25, 0x8e, 0xe0, 0x7b, 0xb5, 0x98, 0xb6, 0x5b, 0xcf, 0x7f, 0x72 },
	{ 0x00, 0x03, 0x00, 0x19, 0x40, 0x10, 0x00, 0x2c, 0x0e, 0x88, 0xed, 0x82, 0x50, 0x67, 0xfb, 0xd1, 0x43, 0xee, 0x03, 0xc6, 0xc6, 0x2b, 0x2c, 0x93 }
};

static const uint8_t RS_POW[] = {
	0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x87, 0x89, 0x95, 0xad, 0xdd, 0x3d, 0x7a, 0xf4,
	0x6f, 0xde, 0x3b, 0x76, 0xec, 0x5f, 0xbe, 0xfb, 0x71, 0xe2, 0x43, 0x86, 0x8b, 0x91, 0xa5, 0xcd,
	0x1d, 0x3a, 0x74, 0xe8, 0x57, 0xae, 0xdb, 0x31, 0x62, 0xc4, 0x0f, 0x1e, 0x3c, 0x78, 0xf0, 0x67,
	0xce, 0x1b, 0x36, 0x6c, 0xd8, 0x37, 0x6e, 0xdc, 0x3f, 0x7e, 0xfc, 0x7f, 0xfe, 0x7b, 0xf6, 0x6b,
	0xd6, 0x2b, 0x56, 0xac, 0xdf, 0x39, 0x72, 0xe4, 0x4f, 0x9e, 0xbb, 0xf1, 0x65, 0xca, 0x13, 0x26,
	0x4c, 0x98, 0xb7, 0xe9, 0x55, 0xaa, 0xd3, 0x21, 0x42, 0x84, 0x8f, 0x99, 0xb5, 0xed, 0x5d, 0xba,
	0xf3, 0x61, 0xc2, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x07, 0x0e, 0x1c, 0x38, 0x70, 0xe0,
	0x47, 0x8e, 0x9b, 0xb1, 0xe5, 0x4d, 0x9a, 0xb3, 0xe1, 0x45, 0x8a, 0x93, 0xa1, 0xc5, 0x0d, 0x1a,
	0x34, 0x68, 0xd0, 0x27, 0x4e, 0x9c, 0xbf, 0xf9, 0x75, 0xea, 0x53, 0xa6, 0xcb, 0x11, 0x22, 0x44,
	0x88, 0x97, 0xa9, 0xd5, 0x2d, 0x5a, 0xb4, 0xef, 0x59, 0xb2, 0xe3, 0x41, 0x82, 0x83, 0x81, 0x85,
	0x8d, 0x9d, 0xbd, 0xfd, 0x7d, 0xfa, 0x73, 0xe6, 0x4b, 0x96, 0xab, 0xd1, 0x25, 0x4a, 0x94, 0xaf,
	0xd9, 0x35, 0x6a, 0xd4, 0x2f, 0x5e, 0xbc, 0xff, 0x79, 0xf2, 0x63, 0xc6, 0x0b, 0x16, 0x2c, 0x58,
	0xb0, 0xe7, 0x49, 0x92, 0xa3, 0xc1, 0x05, 0x0a, 0x14, 0x28, 0x50, 0xa0, 0xc7, 0x09, 0x12, 0x24,
	0x48, 0x90, 0xa7, 0xc9, 0x15, 0x2a, 0x54, 0xa8, 0xd7, 0x29, 0x52, 0xa4, 0xcf, 0x19, 0x32, 0x64,
	0xc8, 0x17, 0x2e, 0x5c, 0xb8, 0xf7, 0x69, 0xd2, 0x23, 0x46, 0x8c, 0x9f, 0xb9, 0xf5, 0x6d, 0xda,
	0x33, 0x66, 0xcc, 0x1f, 0x3e, 0x7c, 0xf8, 0x77, 0xee, 0x5b, 0xb6, 0xeb, 0x51, 0xa2, 0xc3, 0x00,
};

static const uint8_t RS_REV[] = {
	0xff, 0x00, 0x01, 0x63, 0x02, 0xc6, 0x64, 0x6a, 0x03, 0xcd, 0xc7, 0xbc, 0x65, 0x7e, 0x6b, 0x2a,
	0x04, 0x8d, 0xce, 0x4e, 0xc8, 0xd4, 0xbd, 0xe1, 0x66, 0xdd, 0x7f, 0x31, 0x6c, 0x20, 0x2b, 0xf3,
	0x05, 0x57, 0x8e, 0xe8, 0xcf, 0xac, 0x4f, 0x83, 0xc9, 0xd9, 0xd5, 0x41, 0xbe, 0x94, 0xe2, 0xb4,
	0x67, 0x27, 0xde, 0xf0, 0x80, 0xb1, 0x32, 0x35, 0x6d, 0x45, 0x21, 0x12, 0x2c, 0x0d, 0xf4, 0x38,
	0x06, 0x9b, 0x58, 0x1a, 0x8f, 0x79, 0xe9, 0x70, 0xd0, 0xc2, 0xad, 0xa8, 0x50, 0x75, 0x84, 0x48,
	0xca, 0xfc, 0xda, 0x8a, 0xd6, 0x54, 0x42, 0x24, 0xbf, 0x98, 0x95, 0xf9, 0xe3, 0x5e, 0xb5, 0x15,
	0x68, 0x61, 0x28, 0xba, 0xdf, 0x4c, 0xf1, 0x2f, 0x81, 0xe6, 0xb2, 0x3f, 0x33, 0xee, 0x36, 0x10,
	0x6e, 0x18, 0x46, 0xa6, 0x22, 0x88, 0x13, 0xf7, 0x2d, 0xb8, 0x0e, 0x3d, 0xf5, 0xa4, 0x39, 0x3b,
	0x07, 0x9e, 0x9c, 0x9d, 0x59, 0x9f, 0x1b, 0x08, 0x90, 0x09, 0x7a, 0x1c, 0xea, 0xa0, 0x71, 0x5a,
	0xd1, 0x1d, 0xc3, 0x7b, 0xae, 0x0a, 0xa9, 0x91, 0x51, 0x5b, 0x76, 0x72, 0x85, 0xa1, 0x49, 0xeb,
	0xcb, 0x7c, 0xfd, 0xc4, 0xdb, 0x1e, 0x8b, 0xd2, 0xd7, 0x92, 0x55, 0xaa, 0x43, 0x0b, 0x25, 0xaf,
	0xc0, 0x73, 0x99, 0x77, 0x96, 0x5c, 0xfa, 0x52, 0xe4, 0xec, 0x5f, 0x4a, 0xb6, 0xa2, 0x16, 0x86,
	0x69, 0xc5, 0x62, 0xfe, 0x29, 0x7d, 0xbb, 0xcc, 0xe0, 0xd3, 0x4d, 0x8c, 0xf2, 0x1f, 0x30, 0xdc,
	0x82, 0xab, 0xe7, 0x56, 0xb3, 0x93, 0x40, 0xd8, 0x34, 0xb0, 0xef, 0x26, 0x37, 0x0c, 0x11, 0x44,
	0x6f, 0x78, 0x19, 0x9a, 0x47, 0x74, 0xa7, 0xc1, 0x23, 0x53, 0x89, 0xfb, 0x14, 0x5d, 0xf8, 0x97,
	0x2e, 0x4b, 0xb9, 0x60, 0x0f, 0xed, 0x3e, 0xe5, 0xf6, 0x87, 0xa5, 0x17, 0x3a, 0xa3, 0x3c, 0xb7,
};

static const uint8_t RS_GG[] = {
	0x00, 0x4b, 0xeb, 0xd5, 0xef, 0x4c, 0x71, 0x00, 0xf4, 0x00, 0x71, 0x4c, 0xef, 0xd5, 0xeb, 0x4b
};


void GBACartEReaderInit(struct GBACartEReader* ereader) {
	ereader->p->memory.hw.devices |= HW_EREADER;
	_eReaderReset(ereader);

	if (ereader->p->memory.savedata.data[0xD000] == 0xFF) {
		memset(&ereader->p->memory.savedata.data[0xD000], 0, 0x1000);
		memcpy(&ereader->p->memory.savedata.data[0xD000], EREADER_CALIBRATION_TEMPLATE, sizeof(EREADER_CALIBRATION_TEMPLATE));
	}
	if (ereader->p->memory.savedata.data[0xE000] == 0xFF) {
		memset(&ereader->p->memory.savedata.data[0xE000], 0, 0x1000);
		memcpy(&ereader->p->memory.savedata.data[0xE000], EREADER_CALIBRATION_TEMPLATE, sizeof(EREADER_CALIBRATION_TEMPLATE));
	}
}

void GBACartEReaderDeinit(struct GBACartEReader* ereader) {
	if (ereader->dots) {
		mappedMemoryFree(ereader->dots, EREADER_DOTCODE_SIZE);
		ereader->dots = NULL;
	}
	int i;
	for (i = 0; i < EREADER_CARDS_MAX; ++i) {
		if (!ereader->cards[i].data) {
			continue;
		}
		free(ereader->cards[i].data);
		ereader->cards[i].data = NULL;
		ereader->cards[i].size = 0;
	}
}

void GBACartEReaderWrite(struct GBACartEReader* ereader, uint32_t address, uint16_t value) {
	address &= 0x700FF;
	switch (address >> 17) {
	case 0:
		ereader->registerUnk = value & 0xF;
		break;
	case 1:
		ereader->registerReset = (value & 0x8A) | 4;
		if (value & 2) {
			_eReaderReset(ereader);
		}
		break;
	case 2:
		mLOG(GBA_HW, GAME_ERROR, "e-Reader write to read-only registers: %05X:%04X", address, value);
		break;
	default:
		mLOG(GBA_HW, STUB, "Unimplemented e-Reader write: %05X:%04X", address, value);
	}
}

void GBACartEReaderWriteFlash(struct GBACartEReader* ereader, uint32_t address, uint8_t value) {
	address &= 0xFFFF;
	switch (address) {
	case 0xFFB0:
		_eReaderWriteControl0(ereader, value);
		break;
	case 0xFFB1:
		_eReaderWriteControl1(ereader, value);
		break;
	case 0xFFB2:
		ereader->registerLed &= 0xFF00;
		ereader->registerLed |= value;
		break;
	case 0xFFB3:
		ereader->registerLed &= 0x00FF;
		ereader->registerLed |= value << 8;
		break;
	default:
		mLOG(GBA_HW, STUB, "Unimplemented e-Reader write to flash: %04X:%02X", address, value);
	}
}

uint16_t GBACartEReaderRead(struct GBACartEReader* ereader, uint32_t address) {
	address &= 0x700FF;
	uint16_t value;
	switch (address >> 17) {
	case 0:
		return ereader->registerUnk;
	case 1:
		return ereader->registerReset;
	case 2:
		if (address > 0x40088) {
			return 0;
		}
		LOAD_16(value, address & 0xFE, ereader->data);
		return value;
	}
	mLOG(GBA_HW, STUB, "Unimplemented e-Reader read: %05X", address);
	return 0;
}

uint8_t GBACartEReaderReadFlash(struct GBACartEReader* ereader, uint32_t address) {
	address &= 0xFFFF;
	switch (address) {
	case 0xFFB0:
		return ereader->registerControl0;
	case 0xFFB1:
		return ereader->registerControl1;
	default:
		mLOG(GBA_HW, STUB, "Unimplemented e-Reader read from flash: %04X", address);
		return 0;
	}
}

static void _eReaderAnchor(uint8_t* origin) {
	origin[EREADER_DOTCODE_STRIDE * 0 + 1] = 1;
	origin[EREADER_DOTCODE_STRIDE * 0 + 2] = 1;
	origin[EREADER_DOTCODE_STRIDE * 0 + 3] = 1;
	origin[EREADER_DOTCODE_STRIDE * 1 + 0] = 1;
	origin[EREADER_DOTCODE_STRIDE * 1 + 1] = 1;
	origin[EREADER_DOTCODE_STRIDE * 1 + 2] = 1;
	origin[EREADER_DOTCODE_STRIDE * 1 + 3] = 1;
	origin[EREADER_DOTCODE_STRIDE * 1 + 4] = 1;
	origin[EREADER_DOTCODE_STRIDE * 2 + 0] = 1;
	origin[EREADER_DOTCODE_STRIDE * 2 + 1] = 1;
	origin[EREADER_DOTCODE_STRIDE * 2 + 2] = 1;
	origin[EREADER_DOTCODE_STRIDE * 2 + 3] = 1;
	origin[EREADER_DOTCODE_STRIDE * 2 + 4] = 1;
	origin[EREADER_DOTCODE_STRIDE * 3 + 0] = 1;
	origin[EREADER_DOTCODE_STRIDE * 3 + 1] = 1;
	origin[EREADER_DOTCODE_STRIDE * 3 + 2] = 1;
	origin[EREADER_DOTCODE_STRIDE * 3 + 3] = 1;
	origin[EREADER_DOTCODE_STRIDE * 3 + 4] = 1;
	origin[EREADER_DOTCODE_STRIDE * 4 + 1] = 1;
	origin[EREADER_DOTCODE_STRIDE * 4 + 2] = 1;
	origin[EREADER_DOTCODE_STRIDE * 4 + 3] = 1;
}

static void _eReaderAlignment(uint8_t* origin) {
	origin[8] = 1;
	origin[10] = 1;
	origin[12] = 1;
	origin[14] = 1;
	origin[16] = 1;
	origin[18] = 1;
	origin[21] = 1;
	origin[23] = 1;
	origin[25] = 1;
	origin[27] = 1;
	origin[29] = 1;
	origin[31] = 1;
}

static void _eReaderAddress(uint8_t* origin, int a) {
	origin[EREADER_DOTCODE_STRIDE * 7 + 2] = 1;
	uint16_t addr = EREADER_ADDRESS_CODES[a];
	int i;
	for (i = 0; i < 16; ++i) {
		origin[EREADER_DOTCODE_STRIDE * (16 + i) + 2] = (addr >> (15 - i)) & 1;
	}
}

static void _eReaderReedSolomon(const uint8_t* input, uint8_t* output) {
	uint8_t rsBuffer[64] = { 0 };
	int i;
	for (i = 0; i < 48; ++i) {
		rsBuffer[63 - i] = input[i];
	}
	for (i = 0; i < 48; ++i) {
		unsigned z = RS_REV[rsBuffer[63 - i] ^ rsBuffer[15]];
		int j;
		for (j = 15; j >= 0; --j) {
			unsigned x = 0;
			if (j != 0) {
				x = rsBuffer[j - 1];
			}
			if (z != 0xFF) {
				unsigned y = RS_GG[j];
				if (y != 0xFF) {
					y += z;
					if (y >= 0xFF) {
						y -= 0xFF;
					}
					x ^= RS_POW[y];
				}
			}
			rsBuffer[j] = x;
		}
	}
	for (i = 0; i < 16; ++i) {
		output[15 - i] = ~rsBuffer[i];
	}
}

void GBACartEReaderScan(struct GBACartEReader* ereader, const void* data, size_t size) {
	if (!ereader->dots) {
		ereader->dots = anonymousMemoryMap(EREADER_DOTCODE_SIZE);
	}
	ereader->scanX = -24;
	memset(ereader->dots, 0, EREADER_DOTCODE_SIZE);

	uint8_t blockRS[44][0x10];
	uint8_t block0[0x30];
	bool parsed = false;
	bool bitmap = false;
	bool reducedHeader = false;
	size_t blocks;
	int base;
	switch (size) {
	// Raw sizes
	case 2076:
		memcpy(block0, DUMMY_HEADER_STRIP[1], sizeof(DUMMY_HEADER_STRIP[1]));
		reducedHeader = true;
		// Fallthrough
	case 2112:
		parsed = true;
		// Fallthrough
	case 2912:
		base = 25;
		blocks = 28;
		break;
	case 1308:
		memcpy(block0, DUMMY_HEADER_STRIP[0], sizeof(DUMMY_HEADER_STRIP[0]));
		reducedHeader = true;
		// Fallthrough
	case 1344:
		parsed = true;
		// Fallthrough
	case 1872:
		base = 1;
		blocks = 18;
		break;
	// Bitmap sizes
	case 5456:
		bitmap = true;
		blocks = 124;
		break;
	case 3520:
		bitmap = true;
		blocks = 80;
		break;
	default:
		return;
	}

	const uint8_t* cdata = data;
	size_t i;
	if (bitmap) {
		size_t x;
		for (i = 0; i < 40; ++i) {
			const uint8_t* line = &cdata[(i + 2) * blocks];
			uint8_t* origin = &ereader->dots[EREADER_DOTCODE_STRIDE * i + 200];
			for (x = 0; x < blocks; ++x) {
				uint8_t byte = line[x];
				if (x == 123) {
					byte &= 0xE0;
				}
				origin[x * 8 + 0] = (byte >> 7) & 1;
				origin[x * 8 + 1] = (byte >> 6) & 1;
				origin[x * 8 + 2] = (byte >> 5) & 1;
				origin[x * 8 + 3] = (byte >> 4) & 1;
				origin[x * 8 + 4] = (byte >> 3) & 1;
				origin[x * 8 + 5] = (byte >> 2) & 1;
				origin[x * 8 + 6] = (byte >> 1) & 1;
				origin[x * 8 + 7] = byte & 1;
			}
		}
		return;
	}

	for (i = 0; i < blocks + 1; ++i) {
		uint8_t* origin = &ereader->dots[35 * i + 200];
		_eReaderAnchor(&origin[EREADER_DOTCODE_STRIDE * 0]);
		_eReaderAnchor(&origin[EREADER_DOTCODE_STRIDE * 35]);
		_eReaderAddress(origin, base + i);
	}
	if (parsed) {
		if (reducedHeader) {
			memcpy(&block0[0x10], DUMMY_HEADER_FIXED, sizeof(DUMMY_HEADER_FIXED));
			block0[0x0D] = cdata[0x0];
			block0[0x0C] = cdata[0x1];
			block0[0x10] = cdata[0x2];
			block0[0x11] = cdata[0x3];
			block0[0x26] = cdata[0x4];
			block0[0x27] = cdata[0x5];
			block0[0x28] = cdata[0x6];
			block0[0x29] = cdata[0x7];
			block0[0x2A] = cdata[0x8];
			block0[0x2B] = cdata[0x9];
			block0[0x2C] = cdata[0xA];
			block0[0x2D] = cdata[0xB];
			for (i = 0; i < 12; ++i) {
				block0[0x2E] ^= cdata[i];
			}
			unsigned dataChecksum = 0;
			int j;
			for (i = 1; i < (size + 36) / 48; ++i) {
				const uint8_t* block = &cdata[i * 48 - 36];
				_eReaderReedSolomon(block, blockRS[i]);
				unsigned fragmentChecksum = 0;
				for (j = 0; j < 0x30; j += 2) {
					uint16_t halfword;
					fragmentChecksum ^= block[j];
					fragmentChecksum ^= block[j + 1];
					LOAD_16BE(halfword, j, block);
					dataChecksum += halfword;
				}
				block0[0x2F] += fragmentChecksum;
			}
			block0[0x13] = (~dataChecksum) >> 8;
			block0[0x14] = ~dataChecksum;
			for (i = 0; i < 0x2F; ++i) {
				block0[0x2F] += block0[i];
			}
			block0[0x2F] = ~block0[0x2F];
			_eReaderReedSolomon(block0, blockRS[0]);
		} else {
			for (i = 0; i < size / 48; ++i) {
				_eReaderReedSolomon(&cdata[i * 48], blockRS[i]);
			}
		}
	}
	size_t blockId = 0;
	size_t byteOffset = 0;
	for (i = 0; i < blocks; ++i) {
		uint8_t block[1040];
		uint8_t* origin = &ereader->dots[35 * i + 200];
		_eReaderAlignment(&origin[EREADER_DOTCODE_STRIDE * 2]);
		_eReaderAlignment(&origin[EREADER_DOTCODE_STRIDE * 37]);

		const uint8_t* blockData;
		uint8_t parsedBlockData[104];
		if (parsed) {
			memset(parsedBlockData, 0, sizeof(*parsedBlockData));
			const uint8_t* header = BLOCK_HEADER[size == 1344 ? 0 : 1];
			parsedBlockData[0] = header[(2 * i) % 0x18];
			parsedBlockData[1] = header[(2 * i) % 0x18 + 1];
			int j;
			for (j = 2; j < 104; ++j) {
				if (byteOffset >= 0x40) {
					break;
				}
				if (byteOffset >= 0x30) {
					parsedBlockData[j] = blockRS[blockId][byteOffset - 0x30];
				} else if (!reducedHeader) {
					parsedBlockData[j] = cdata[blockId * 0x30 + byteOffset];
				} else {
					if (blockId > 0) {
						parsedBlockData[j] = cdata[blockId * 0x30 + byteOffset - 36];
					} else {
						parsedBlockData[j] = block0[byteOffset];
					}
				}
				++blockId;
				if (blockId * 0x30 >= size) {
					blockId = 0;
					++byteOffset;
				}
			}
			blockData = parsedBlockData;
		} else {
			blockData = &cdata[i * 104];
		}
		int b;
		for (b = 0; b < 104; ++b) {
			const int* nybble5;
			nybble5 = EREADER_NYBBLE_5BIT[blockData[b] >> 4];
			block[b * 10 + 0] = nybble5[0];
			block[b * 10 + 1] = nybble5[1];
			block[b * 10 + 2] = nybble5[2];
			block[b * 10 + 3] = nybble5[3];
			block[b * 10 + 4] = nybble5[4];
			nybble5 = EREADER_NYBBLE_5BIT[blockData[b] & 0xF];
			block[b * 10 + 5] = nybble5[0];
			block[b * 10 + 6] = nybble5[1];
			block[b * 10 + 7] = nybble5[2];
			block[b * 10 + 8] = nybble5[3];
			block[b * 10 + 9] = nybble5[4];
		}

		b = 0;
		int y;
		for (y = 0; y < 3; ++y) {
			memcpy(&origin[EREADER_DOTCODE_STRIDE * (4 + y) + 7], &block[b], 26);
			b += 26;
		}
		for (y = 0; y < 26; ++y) {
			memcpy(&origin[EREADER_DOTCODE_STRIDE * (7 + y) + 3], &block[b], 34);
			b += 34;
		}
		for (y = 0; y < 3; ++y) {
			memcpy(&origin[EREADER_DOTCODE_STRIDE * (33 + y) + 7], &block[b], 26);
			b += 26;
		}
	}
}

void _eReaderReset(struct GBACartEReader* ereader) {
	memset(ereader->data, 0, sizeof(ereader->data));
	ereader->registerUnk = 0;
	ereader->registerReset = 4;
	ereader->registerControl0 = 0;
	ereader->registerControl1 = 0x80;
	ereader->registerLed = 0;
	ereader->state = 0;
	ereader->activeRegister = 0;
}

void _eReaderWriteControl0(struct GBACartEReader* ereader, uint8_t value) {
	EReaderControl0 control = value & 0x7F;
	EReaderControl0 oldControl = ereader->registerControl0;
	if (ereader->state == EREADER_SERIAL_INACTIVE) {
		if (EReaderControl0IsClock(oldControl) && EReaderControl0IsData(oldControl) && !EReaderControl0IsData(control)) {
			ereader->state = EREADER_SERIAL_STARTING;
		}
	} else if (EReaderControl0IsClock(oldControl) && !EReaderControl0IsData(oldControl) && EReaderControl0IsData(control)) {
		ereader->state = EREADER_SERIAL_INACTIVE;

	} else if (ereader->state == EREADER_SERIAL_STARTING) {
		if (EReaderControl0IsClock(oldControl) && !EReaderControl0IsData(oldControl) && !EReaderControl0IsClock(control)) {
			ereader->state = EREADER_SERIAL_BIT_0;
			ereader->command = EREADER_COMMAND_IDLE;
		}
	} else if (EReaderControl0IsClock(oldControl) && !EReaderControl0IsClock(control)) {
		mLOG(GBA_HW, DEBUG, "[e-Reader] Serial falling edge: %c %i", EReaderControl0IsDirection(control) ? '>' : '<', EReaderControl0GetData(control));
		// TODO: Improve direction control
		if (EReaderControl0IsDirection(control)) {
			ereader->byte |= EReaderControl0GetData(control) << (7 - (ereader->state - EREADER_SERIAL_BIT_0));
			++ereader->state;
			if (ereader->state == EREADER_SERIAL_END_BIT) {
				mLOG(GBA_HW, DEBUG, "[e-Reader] Wrote serial byte: %02x", ereader->byte);
				switch (ereader->command) {
				case EREADER_COMMAND_IDLE:
					ereader->command = ereader->byte;
					break;
				case EREADER_COMMAND_SET_INDEX:
					ereader->activeRegister = ereader->byte;
					ereader->command = EREADER_COMMAND_WRITE_DATA;
					break;
				case EREADER_COMMAND_WRITE_DATA:
					switch (ereader->activeRegister & 0x7F) {
					case 0:
					case 0x57:
					case 0x58:
					case 0x59:
					case 0x5A:
						// Read-only
						mLOG(GBA_HW, GAME_ERROR, "Writing to read-only e-Reader serial register: %02X", ereader->activeRegister);
						break;
					default:
						if ((ereader->activeRegister & 0x7F) > 0x5A) {
							mLOG(GBA_HW, GAME_ERROR, "Writing to non-existent e-Reader serial register: %02X", ereader->activeRegister);
							break;
						}
						ereader->serial[ereader->activeRegister & 0x7F] = ereader->byte;
						break;
					}
					++ereader->activeRegister;
					break;
				default:
					mLOG(GBA_HW, ERROR, "Hit undefined state %02X in e-Reader state machine", ereader->command);
					break;
				}
				ereader->state = EREADER_SERIAL_BIT_0;
				ereader->byte = 0;
			}
		} else if (ereader->command == EREADER_COMMAND_READ_DATA) {
			int bit = ereader->serial[ereader->activeRegister & 0x7F] >> (7 - (ereader->state - EREADER_SERIAL_BIT_0));
			control = EReaderControl0SetData(control, bit);
			++ereader->state;
			if (ereader->state == EREADER_SERIAL_END_BIT) {
				++ereader->activeRegister;
				mLOG(GBA_HW, DEBUG, "[e-Reader] Read serial byte: %02x", ereader->serial[ereader->activeRegister & 0x7F]);
			}
		}
	} else if (!EReaderControl0IsDirection(control)) {
		// Clear the error bit
		control = EReaderControl0ClearData(control);
	}
	ereader->registerControl0 = control;
	if (!EReaderControl0IsScan(oldControl) && EReaderControl0IsScan(control)) {
		if (ereader->scanX > 1000) {
			_eReaderScanCard(ereader);
		}
		ereader->scanX = 0;
		ereader->scanY = 0;
	} else if (EReaderControl0IsLedEnable(control) && EReaderControl0IsScan(control) && !EReaderControl1IsScanline(ereader->registerControl1)) {
		_eReaderReadData(ereader);
	}
	mLOG(GBA_HW, STUB, "Unimplemented e-Reader Control0 write: %02X", value);
}

void _eReaderWriteControl1(struct GBACartEReader* ereader, uint8_t value) {
	EReaderControl1 control = (value & 0x32) | 0x80;
	ereader->registerControl1 = control;
	if (EReaderControl0IsScan(ereader->registerControl0) && !EReaderControl1IsScanline(control)) {
		++ereader->scanY;
		if (ereader->scanY == (ereader->serial[0x15] | (ereader->serial[0x14] << 8))) {
			ereader->scanY = 0;
			if (ereader->scanX < 3400) {
				ereader->scanX += 210;
			}
		}
		_eReaderReadData(ereader);
	}
	mLOG(GBA_HW, STUB, "Unimplemented e-Reader Control1 write: %02X", value);
}

void _eReaderReadData(struct GBACartEReader* ereader) {
	memset(ereader->data, 0, EREADER_BLOCK_SIZE);
	if (!ereader->dots) {
		_eReaderScanCard(ereader);
	}
	if (ereader->dots) {
		int y = ereader->scanY - 10;
		if (y < 0 || y >= 120) {
			memset(ereader->data, 0, EREADER_BLOCK_SIZE);
		} else {
			int i;
			uint8_t* origin = &ereader->dots[EREADER_DOTCODE_STRIDE * (y / 3) + 16];
			for (i = 0; i < 20; ++i) {
				uint16_t word = 0;
				int x = ereader->scanX + i * 16;
				word |= origin[(x +  0) / 3] << 8;
				word |= origin[(x +  1) / 3] << 9;
				word |= origin[(x +  2) / 3] << 10;
				word |= origin[(x +  3) / 3] << 11;
				word |= origin[(x +  4) / 3] << 12;
				word |= origin[(x +  5) / 3] << 13;
				word |= origin[(x +  6) / 3] << 14;
				word |= origin[(x +  7) / 3] << 15;
				word |= origin[(x +  8) / 3];
				word |= origin[(x +  9) / 3] << 1;
				word |= origin[(x + 10) / 3] << 2;
				word |= origin[(x + 11) / 3] << 3;
				word |= origin[(x + 12) / 3] << 4;
				word |= origin[(x + 13) / 3] << 5;
				word |= origin[(x + 14) / 3] << 6;
				word |= origin[(x + 15) / 3] << 7;
				STORE_16(word, (19 - i) << 1, ereader->data);
			}
		}
	}
	ereader->registerControl1 = EReaderControl1FillScanline(ereader->registerControl1);
	if (EReaderControl0IsLedEnable(ereader->registerControl0)) {
		uint16_t led = ereader->registerLed * 2;
		if (led > 0x4000) {
			led = 0x4000;
		}
		GBARaiseIRQ(ereader->p, GBA_IRQ_GAMEPAK, -led);
	}
}


void _eReaderScanCard(struct GBACartEReader* ereader) {
	if (ereader->dots) {
		memset(ereader->dots, 0, EREADER_DOTCODE_SIZE);
	}
	int i;
	for (i = 0; i < EREADER_CARDS_MAX; ++i) {
		if (!ereader->cards[i].data) {
			continue;
		}
		GBACartEReaderScan(ereader, ereader->cards[i].data, ereader->cards[i].size);
		free(ereader->cards[i].data);
		ereader->cards[i].data = NULL;
		ereader->cards[i].size = 0;
		break;
	}
}

void GBACartEReaderQueueCard(struct GBA* gba, const void* data, size_t size) {
	struct GBACartEReader* ereader = &gba->memory.ereader;
	int i;
	for (i = 0; i < EREADER_CARDS_MAX; ++i) {
		if (ereader->cards[i].data) {
			continue;
		}
		ereader->cards[i].data = malloc(size);
		memcpy(ereader->cards[i].data, data, size);
		ereader->cards[i].size = size;
		return;
	}
}

#ifdef USE_FFMPEG
struct EReaderAnchor {
	float x;
	float y;

	float top;
	float bottom;
	float left;
	float right;

	size_t nNeighbors;
	struct EReaderAnchor** neighbors;
};

struct EReaderBlock {
	float x[4];
	float y[4];

	uint8_t rawdots[36 * 36];
	unsigned histogram[256];
	uint8_t threshold;
	uint8_t min;
	uint8_t max;

	unsigned errors;
	unsigned missing;
	unsigned extra;

	bool hFlip;
	bool vFlip;
	bool dots[36 * 36];

	uint16_t id;
	uint16_t next;
};

DEFINE_VECTOR(EReaderAnchorList, struct EReaderAnchor);
DEFINE_VECTOR(EReaderBlockList, struct EReaderBlock);

static void _eReaderScanDownsample(struct EReaderScan* scan) {
	// TODO: Replace this logic with a value based on total area
	scan->scale = 400;
	if (scan->srcWidth > scan->srcHeight) {
		scan->height = 400;
		scan->width = scan->srcWidth * 400 / scan->srcHeight;
	} else {
		scan->width = 400;
		scan->height = scan->srcHeight * 400 / scan->srcWidth;
	}
	scan->buffer = malloc(scan->width * scan->height);
	FFmpegScale(scan->srcBuffer, scan->srcWidth, scan->srcHeight, scan->srcWidth, scan->buffer, scan->width, scan->height, scan->width, mCOLOR_L8, 3);
	free(scan->srcBuffer);
	scan->srcBuffer = NULL;
}

static int _compareAnchors(const void* va, const void* vb) {
	const struct EReaderAnchor* a = va;
	const struct EReaderAnchor* b = vb;

	float x = a->x - b->x;
	float y = a->y - b->y;
	float w = a->right - a->left + b->right - b->left;
	float h = a->bottom - a->top + b->bottom - b->top;
	if (x < -w) {
		return -1;
	}
	if (x > w) {
		return 1;
	}
	if (y < -h) {
		return -1;
	}
	if (y > h) {
		return 1;
	}
	return 0;
}

static int _compareBlocks(const void* va, const void* vb) {
	const struct EReaderBlock* a = va;
	const struct EReaderBlock* b = vb;
	if (a->id < b->id) {
		return -1;
	}
	if (a->id > b->id) {
		return 1;
	}
	return 0;
}

struct EReaderScan* EReaderScanCreate(unsigned width, unsigned height) {
	struct EReaderScan* scan = calloc(1, sizeof(*scan));
	scan->srcWidth = width;
	scan->srcHeight = height;
	scan->srcBuffer = calloc(width, height);

	scan->min = 255;
	scan->max = 0;
	scan->mean = 128;
	scan->anchorThreshold = 128;

	EReaderAnchorListInit(&scan->anchors, 64);
	EReaderBlockListInit(&scan->blocks, 32);

	return scan;
}

void EReaderScanDestroy(struct EReaderScan* scan) {
	free(scan->buffer);
	size_t i;
	for (i = 0; i < EReaderAnchorListSize(&scan->anchors); ++i) {
		struct EReaderAnchor* anchor = EReaderAnchorListGetPointer(&scan->anchors, i);
		if (anchor->neighbors) {
			free(anchor->neighbors);
		}
	}
	EReaderAnchorListDeinit(&scan->anchors);
	EReaderBlockListDeinit(&scan->blocks);
	free(scan);
}

#ifdef USE_PNG
struct EReaderScan* EReaderScanLoadImagePNG(const char* filename) {
	struct VFile* vf = VFileOpen(filename, O_RDONLY);
	if (!vf) {
		return NULL;
	}
	png_structp png = PNGReadOpen(vf, 0);
	if (!png) {
		vf->close(vf);
		return NULL;
	}
	png_infop info = png_create_info_struct(png);
	png_infop end = png_create_info_struct(png);
	if (!PNGReadHeader(png, info)) {
		PNGReadClose(png, info, end);
		vf->close(vf);
		return NULL;
	}
	unsigned height = png_get_image_height(png, info);
	unsigned width = png_get_image_width(png, info);
	int type = png_get_color_type(png, info);
	int depth = png_get_bit_depth(png, info);
	void* image = NULL;
	switch (type) {
	case PNG_COLOR_TYPE_RGB:
		if (depth != 8) {
			break;
		}
		image = malloc(height * width * 3);
		if (!image) {
			goto out;
		}
		if (!PNGReadPixels(png, info, image, width, height, width)) {
			free(image);
			image = NULL;
			goto out;
		}
		break;
	case PNG_COLOR_TYPE_RGBA:
		if (depth != 8) {
			break;
		}
		image = malloc(height * width * 4);
		if (!image) {
			goto out;
		}
		if (!PNGReadPixelsA(png, info, image, width, height, width)) {
			free(image);
			image = NULL;
			goto out;
		}
		break;
	default:
		break;
	}
	PNGReadFooter(png, end);
out:
	PNGReadClose(png, info, end);
	vf->close(vf);
	if (!image) {
		return NULL;
	}
	struct EReaderScan* scan = NULL;
	switch (type) {
	case PNG_COLOR_TYPE_RGB:
		scan = EReaderScanLoadImage(image, width, height, width);
		break;
	case PNG_COLOR_TYPE_RGBA:
		scan = EReaderScanLoadImageA(image, width, height, width);
		break;
	default:
		break;
	}
	free(image);
	return scan;
}
#endif

struct EReaderScan* EReaderScanLoadImage(const void* pixels, unsigned width, unsigned height, unsigned stride) {
	struct EReaderScan* scan = EReaderScanCreate(width, height);
	unsigned y;
	for (y = 0; y < height; ++y) {
		const uint8_t* irow = pixels;
		uint8_t* orow = &scan->srcBuffer[y * width];
		irow = &irow[y * stride];
		unsigned x;
		for (x = 0; x < width; ++x) {
			orow[x] = irow[x * 3 + 2];
		}
	}
	_eReaderScanDownsample(scan);
	return scan;
}

struct EReaderScan* EReaderScanLoadImageA(const void* pixels, unsigned width, unsigned height, unsigned stride) {
	struct EReaderScan* scan = EReaderScanCreate(width, height);
	unsigned y;
	for (y = 0; y < height; ++y) {
		const uint8_t* irow = pixels;
		uint8_t* orow = &scan->srcBuffer[y * width];
		irow = &irow[y * stride];
		unsigned x;
		for (x = 0; x < width; ++x) {
			orow[x] = irow[x * 4 + 2];
		}
	}
	_eReaderScanDownsample(scan);
	return scan;
}

struct EReaderScan* EReaderScanLoadImage8(const void* pixels, unsigned width, unsigned height, unsigned stride) {
	struct EReaderScan* scan = EReaderScanCreate(width, height);
	unsigned y;
	for (y = 0; y < height; ++y) {
		const uint8_t* row = pixels;
		row = &row[y * stride];
		memcpy(&scan->srcBuffer[y * width], row, width);
	}
	_eReaderScanDownsample(scan);
	return scan;
}

void EReaderScanDetectParams(struct EReaderScan* scan) {
	size_t sum = 0;
	unsigned y;
	for (y = 0; y < scan->height; ++y) {
		const uint8_t* row = &scan->buffer[scan->width * y];
		unsigned x;
		for (x = 0; x < scan->width; ++x) {
			uint8_t color = row[x];
			sum += color;
			if (color < scan->min) {
				scan->min = color;
			}
			if (color > scan->max) {
				scan->max = color;
			}
		}
	}
	scan->mean = sum / (scan->width * scan->height);
	scan->anchorThreshold = 2 * (scan->mean - scan->min) / 5 + scan->min;
}

void EReaderScanDetectAnchors(struct EReaderScan* scan) {
	uint8_t* output = malloc(scan->width * scan->height);
	unsigned dim = scan->scale;
	// TODO: Codify this magic constant
	size_t dims[] = {dim / 30, dim / 30};
	struct ConvolutionKernel kern;
	ConvolutionKernelCreate(&kern, 2, dims);
	ConvolutionKernelFillRadial(&kern, true);
	Convolve2DClampPacked8(scan->buffer, output, scan->width, scan->height, scan->width, &kern);
	ConvolutionKernelDestroy(&kern);

	unsigned y;
	for (y = 0; y < scan->height; ++y) {
		const uint8_t* row = &output[scan->width * y];
		unsigned x;
		for (x = 0; x < scan->width; ++x) {
			uint8_t color = row[x];
			if (color < scan->anchorThreshold) {
				bool mustInsert = true;
				size_t i;
				for (i = 0; i < EReaderAnchorListSize(&scan->anchors); ++i) {
					struct EReaderAnchor* anchor = EReaderAnchorListGetPointer(&scan->anchors, i);
					float diffX = anchor->x - x;
					float diffY = anchor->y - y;
					float distance = hypotf(diffX, diffY);
					float radius = sqrtf((anchor->right - anchor->left) * (anchor->bottom - anchor->top)) / 2.f; // TODO: This should be M_PI, not 2
					if (radius + dim / 45.f > distance) { // TODO: Codify this magic constant
						if (x < anchor->left) {
							anchor->left = x;
						}
						if (x > anchor->right) {
							anchor->right = x;
						}
						if (y < anchor->top) {
							anchor->top = y;
						}
						if (y > anchor->bottom) {
							anchor->bottom = y;
						}
						anchor->x = (anchor->left + anchor->right) / 2.f;
						anchor->y = (anchor->bottom + anchor->top) / 2.f;
						mustInsert = false;
						break;
					}
				}
				if (mustInsert) {
					struct EReaderAnchor* anchor = EReaderAnchorListAppend(&scan->anchors);
					anchor->neighbors = NULL;
					anchor->left = x - 0.5f;
					anchor->right = x + 0.5f;
					anchor->x = x;
					anchor->top = y - 0.5f;
					anchor->bottom = y + 0.5f;
					anchor->y = y;
				}
			}
		}
	}
	free(output);
}

void EReaderScanFilterAnchors(struct EReaderScan* scan) {
	unsigned dim = scan->scale;
	float areaMean = 0;
	size_t i;
	for (i = 0; i < EReaderAnchorListSize(&scan->anchors); ++i) {
		struct EReaderAnchor* anchor = EReaderAnchorListGetPointer(&scan->anchors, i);
		float width = anchor->right - anchor->left;
		float height = anchor->bottom - anchor->top;
		float area = width * height;
		// TODO: Codify these magic constants
		float portion = dim / sqrtf(area);
		bool keep = portion > 9. && portion < 30.;
		if (width > height) {
			if (width / height > 1.2f) {
				keep = false;
			}
		} else {
			if (height / width > 1.2f) {
				keep = false;
			}
		}
		if (!keep) {
			EReaderAnchorListShift(&scan->anchors, i, 1);
			--i;
		} else {
			areaMean += portion;
		}
	}
	areaMean /= EReaderAnchorListSize(&scan->anchors);
	for (i = 0; i < EReaderAnchorListSize(&scan->anchors); ++i) {
		struct EReaderAnchor* anchor = EReaderAnchorListGetPointer(&scan->anchors, i);
		float area = (anchor->right - anchor->left) * (anchor->bottom - anchor->top);
		// TODO: Codify these magic constants
		float portion = dim / sqrtf(area);
		if (fabsf(portion - areaMean) / areaMean > 0.5) {
			EReaderAnchorListShift(&scan->anchors, i, 1);
			--i;
		}
	}

	qsort(EReaderAnchorListGetPointer(&scan->anchors, 0), EReaderAnchorListSize(&scan->anchors), sizeof(struct EReaderAnchor), _compareAnchors);
}

void EReaderScanConnectAnchors(struct EReaderScan* scan) {
	size_t i;
	for (i = 0; i < EReaderAnchorListSize(&scan->anchors); ++i) {
		struct EReaderAnchor* anchor = EReaderAnchorListGetPointer(&scan->anchors, i);
		float closest = scan->scale * 2.f;
		float threshold;
		size_t j;
		for (j = 0; j < EReaderAnchorListSize(&scan->anchors); ++j) {
			if (i == j) {
				continue;
			}
			struct EReaderAnchor* candidate = EReaderAnchorListGetPointer(&scan->anchors, j);
			float dx = anchor->x - candidate->x;
			float dy = anchor->y - candidate->y;
			float distance = hypotf(dx, dy);
			if (distance < closest) {
				closest = distance;
				threshold = 1.25 * closest;
			}
		}
		if (closest >= scan->scale) {
			continue;
		}
		if (anchor->neighbors) {
			free(anchor->neighbors);
		}
		// This is an intentional over-estimate which we prune down later
		anchor->neighbors = calloc(EReaderAnchorListSize(&scan->anchors) - 1, sizeof(struct EReaderAnchor*));
		size_t matches = 0;
		for (j = 0; j < EReaderAnchorListSize(&scan->anchors); ++j) {
			if (i == j) {
				continue;
			}
			struct EReaderAnchor* candidate = EReaderAnchorListGetPointer(&scan->anchors, j);
			float dx = anchor->x - candidate->x;
			float dy = anchor->y - candidate->y;
			float distance = hypotf(dx, dy);
			if (distance <= threshold) {
				anchor->neighbors[matches] = candidate;
				++matches;
			}
		}
		if (matches) {
			anchor->neighbors = realloc(anchor->neighbors, matches * sizeof(struct EReaderAnchor*));
			anchor->nNeighbors = matches;
		} else {
			free(anchor->neighbors);
			anchor->neighbors = NULL;
		}
	}
}

void EReaderScanCreateBlocks(struct EReaderScan* scan) {
	size_t i;
	for (i = 0; i < EReaderAnchorListSize(&scan->anchors); ++i) {
		struct EReaderAnchor* anchor = EReaderAnchorListGetPointer(&scan->anchors, i);
		if (anchor->nNeighbors < 2) {
			continue;
		}
		struct EReaderAnchor* neighbors[3] = {anchor->neighbors[0], anchor->neighbors[1]};
		size_t j;
		for (j = 0; j < neighbors[0]->nNeighbors; ++j) {
			if (neighbors[0]->neighbors[j] == anchor) {
				size_t remaining = neighbors[0]->nNeighbors - j - 1;
				--neighbors[0]->nNeighbors;
				if (neighbors[0]->nNeighbors) {
					memmove(&neighbors[0]->neighbors[j], &neighbors[0]->neighbors[j + 1], remaining * sizeof(struct EReaderAnchor*));
				}
			}
		}
		for (j = 0; j < neighbors[1]->nNeighbors; ++j) {
			if (neighbors[1]->neighbors[j] == anchor) {
				size_t remaining = neighbors[1]->nNeighbors - j - 1;
				--neighbors[1]->nNeighbors;
				if (neighbors[1]->nNeighbors) {
					memmove(&neighbors[1]->neighbors[j], &neighbors[1]->neighbors[j + 1], remaining * sizeof(struct EReaderAnchor*));
				}
			}
		}

		// TODO: Codify this constant
		if (fabsf(neighbors[0]->x - neighbors[1]->x) < 6) {
			struct EReaderAnchor* neighbor = neighbors[0];
			neighbors[0] = neighbors[1];
			neighbors[1] = neighbor;
		}
		bool found = false;
		for (j = 0; j < neighbors[0]->nNeighbors && !found; ++j) {
			size_t k;
			for (k = 0; k < neighbors[1]->nNeighbors; ++k) {
				if (neighbors[0]->neighbors[j] == neighbors[1]->neighbors[k]) {
					neighbors[2] = neighbors[0]->neighbors[j];
					found = true;
					break;
				}
			}
		}
		if (!found) {
			continue;
		}

		struct EReaderBlock* block = EReaderBlockListAppend(&scan->blocks);
		memset(block, 0, sizeof(*block));
		block->x[0] = anchor->x;
		block->x[1] = neighbors[0]->x;
		block->x[2] = neighbors[1]->x;
		block->x[3] = neighbors[2]->x;
		block->y[0] = anchor->y;
		block->y[1] = neighbors[0]->y;
		block->y[2] = neighbors[1]->y;
		block->y[3] = neighbors[2]->y;
		block->min = scan->min;
		block->max = scan->max;
		block->threshold = scan->mean;

		unsigned y;
		for (y = 0; y < 36; ++y) {
			unsigned x;
			for (x = 0; x < 36; ++x) {
				float topX = (block->x[1] - block->x[0]) * x / 35.f + block->x[0];
				float topY = (block->y[1] - block->y[0]) * x / 35.f + block->y[0];
				float bottomX = (block->x[3] - block->x[2]) * x / 35.f + block->x[2];
				float bottomY = (block->y[3] - block->y[2]) * x / 35.f + block->y[2];
				unsigned midX = (bottomX - topX) * y / 35.f + topX;
				unsigned midY = (bottomY - topY) * y / 35.f + topY;
				uint8_t color = scan->buffer[midY * scan->width + midX];
				block->rawdots[y * 36 + x] = color;
				if ((x >= 5 && x <= 30) || (y >= 5 && y <= 30)) {
					++block->histogram[color];
				}
			}
		}
	}
}

void EReaderScanDetectBlockThreshold(struct EReaderScan* scan, int blockId) {
	if (blockId < 0 || (unsigned) blockId >= EReaderBlockListSize(&scan->blocks)) {
		return;
	}
	struct EReaderBlock* block = EReaderBlockListGetPointer(&scan->blocks, blockId);
	unsigned histograms[0xF8] = {0};
	unsigned* histogram[] = {
		block->histogram,
		&histograms[0x00],
		&histograms[0x80],
		&histograms[0xC0],
		&histograms[0xE0],
		&histograms[0xF0]
	};

	size_t i;
	for (i = 0; i < 256; ++i) {
		unsigned baseline = histogram[0][i];
		histogram[1][i >> 1] += baseline;
		histogram[2][i >> 2] += baseline;
		histogram[3][i >> 3] += baseline;
		histogram[4][i >> 4] += baseline;
		histogram[5][i >> 5] += baseline;
	}

	int offset;
	for (offset = 0; offset < 7; ++offset) {
		if (histogram[5][offset] > histogram[5][offset + 1]) {
			break;
		}
	}
	for (; offset < 7; ++offset) {
		if (histogram[5][offset] < histogram[5][offset + 1]) {
			break;
		}
	}

	for (i = 6; i--; offset *= 2) {
		if (histogram[i][offset] > histogram[i][offset + 1]) {
			++offset;
		}
	}
	block->threshold = offset / 2;
}

bool EReaderScanRecalibrateBlock(struct EReaderScan* scan, int blockId) {
	if (blockId < 0 || (unsigned) blockId >= EReaderBlockListSize(&scan->blocks)) {
		return false;
	}
	struct EReaderBlock* block = EReaderBlockListGetPointer(&scan->blocks, blockId);
	if (block->missing && block->extra) {
		return false;
	}
	int errors = block->errors;
	if (block->missing) {
		while (errors > 0) {
			errors -= block->histogram[block->threshold];
			while (!block->histogram[block->threshold] && block->threshold < 254) {
				++block->threshold;
			}
			++block->threshold;
			if (block->threshold >= 255) {
				return false;
			}
		}
	} else {
		return false;
	}
	return true;
}

bool EReaderScanScanBlock(struct EReaderScan* scan, int blockId, bool strict) {
	if (blockId < 0 || (unsigned) blockId >= EReaderBlockListSize(&scan->blocks)) {
		return false;
	}
	struct EReaderBlock* block = EReaderBlockListGetPointer(&scan->blocks, blockId);
	block->errors = 0;
	block->missing = 0;
	block->extra = 0;

	bool dots[36 * 36] = {0};
	size_t y;
	for (y = 0; y < 36; ++y) {
		size_t x;
		for (x = 0; x < 36; ++x) {
			if ((x < 5 || x > 30) && (y < 5 || y > 30)) {
				continue;
			}
			uint8_t color = block->rawdots[y * 36 + x];
			if (color < block->threshold) {
				dots[y * 36 + x] = true;
			} else {
				dots[y * 36 + x] = false;
			}
		}
	}
	int horizontal = 0;
	int vertical = 0;
	int hMissing = 0;
	int hExtra = 0;
	int vMissing = 0;
	int vExtra = 0;
	static const bool alignment[36] = { 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0 };
	int i;
	for (i = 3; i < 33; ++i) {
		if (dots[i] != alignment[i]) {
			++horizontal;
			if (alignment[i]) {
				++hMissing;
			} else {
				++hExtra;
			}
		}
		if (dots[i + 36 * 35] != alignment[i]) {
			++horizontal;
			if (alignment[i]) {
				++hMissing;
			} else {
				++hExtra;
			}
		}
		if (dots[i * 36] != alignment[i]) {
			++vertical;
			if (alignment[i]) {
				++vMissing;
			} else {
				++vExtra;
			}
		}
		if (dots[i * 36 + 35] != alignment[i]) {
			++vertical;
			if (alignment[i]) {
				++vMissing;
			} else {
				++vExtra;
			}
		}
	}

	int errors;
	if (horizontal < vertical) {
		errors = horizontal;
		block->errors += horizontal;
		block->extra += hExtra;
		block->missing += hMissing;
		horizontal = 0;
	} else {
		errors = vertical;
		block->errors += vertical;
		block->extra += vExtra;
		block->missing += vMissing;
		vertical = 0;
	}
	if (strict && errors) {
		return false;
	}

	int sector[2] = {0};
	if (!vertical) {
		for (i = 5; i < 30; ++i) {
			sector[0] |= dots[i] << (29 - i);
			sector[1] |= dots[i + 36 * 35] << (29 - i);
		}
	} else {
		for (i = 5; i < 30; ++i) {
			sector[0] |= dots[i * 36] << (29 - i);
			sector[1] |= dots[i * 36 + 35] << (29 - i);
		}
	}

	int xFlip = 1;
	int yFlip = 1;
	int xBias = 0;
	int yBias = 0;
	block->hFlip = false;
	block->vFlip = false;
	if ((sector[0] & 0x1FF) == 1) {
		yFlip = -1;
		yBias = 35;
		block->vFlip = true;
		int s0 = 0;
		int s1 = 0;
		for (i = 0; i < 16; ++i) {
			s0 |= ((sector[0] >> (i + 9)) & 1) << (15 - i);
			s1 |= ((sector[1] >> (i + 9)) & 1) << (15 - i);
		}
		sector[0] = s0;
		sector[1] = s1;
	}
	sector[0] &= 0xFFFF;
	sector[1] &= 0xFFFF;
	if (sector[1] < sector[0]) {
		xFlip = -1;
		xBias = 35;
		block->hFlip = true;
		int sector0 = sector[0];
		sector[0] = sector[1];
		sector[1] = sector0;
	}

	memset(block->dots, 0, sizeof(block->dots));
	if (!horizontal) {
		block->id = sector[0];
		block->next = sector[1];
		int y;
		for (y = 0; y < 36; ++y) {
			int ty = y * yFlip + yBias;
			int x;
			for (x = 0; x < 36; ++x) {
				int tx = x * xFlip + xBias;
				block->dots[ty * 36 + tx] = dots[y * 36 + x];
			}
		}
	} else {
		block->id = sector[0];
		block->next = sector[1];
		int y;
		for (y = 0; y < 36; ++y) {
			int tx = y * xFlip + xBias;
			int x;
			for (x = 0; x < 36; ++x) {
				int ty = x * yFlip + yBias;
				block->dots[ty * 36 + tx] = dots[y * 36 + x];
			}
		}
	}
	return true;
}

bool EReaderScanCard(struct EReaderScan* scan) {
	EReaderScanDetectParams(scan);
	EReaderScanDetectAnchors(scan);
	EReaderScanFilterAnchors(scan);
	if (EReaderAnchorListSize(&scan->anchors) & 1 || EReaderAnchorListSize(&scan->anchors) < 36) {
		return false;
	}
	EReaderScanConnectAnchors(scan);
	EReaderScanCreateBlocks(scan);
	size_t blocks = EReaderBlockListSize(&scan->blocks);
	size_t i;
	for (i = 0; i < blocks; ++i) {
		EReaderScanDetectBlockThreshold(scan, i);
		int errors = 36 * 36;
		while (!EReaderScanScanBlock(scan, i, true)) {
			if (errors < EReaderBlockListGetPointer(&scan->blocks, i)->errors) {
				return false;
			}
			errors = EReaderBlockListGetPointer(&scan->blocks, i)->errors;
			if (!EReaderScanRecalibrateBlock(scan, i)) {
				return false;
			}
		}
	}
	qsort(EReaderBlockListGetPointer(&scan->blocks, 0), EReaderBlockListSize(&scan->blocks), sizeof(struct EReaderBlock), _compareBlocks);
	return true;
}

static void _eReaderBitAnchor(uint8_t* output, size_t stride, int offset) {
	static const uint8_t anchor[5][5] = {
		{ 0, 1, 1, 1, 0 },
		{ 1, 1, 1, 1, 1 },
		{ 1, 1, 1, 1, 1 },
		{ 1, 1, 1, 1, 1 },
		{ 0, 1, 1, 1, 0 },
	};
	size_t y;
	for (y = 0; y < 5; ++y) {
		uint8_t* line = &output[y * stride];
		size_t x;
		for (x = 0; x < 5; ++x) {
			size_t xo = x + offset;
			line[xo >> 3] |= 1 << (~xo & 7);
			line[xo >> 3] &= ~(anchor[y][x] << (~xo & 7));
		}
	}
}

void EReaderScanOutputBitmap(const struct EReaderScan* scan, void* output, size_t stride) {
	size_t blocks = EReaderBlockListSize(&scan->blocks);
	uint8_t* rows = output;

	memset(rows, 0xFF, stride * 44);

	size_t i;
	size_t y;
	for (y = 0; y < 36; ++y) {
		uint8_t* line = &rows[(y + 4) * stride];
		size_t offset = 4;
		for (i = 0; i < blocks; ++i) {
			const struct EReaderBlock* block = EReaderBlockListGetConstPointer(&scan->blocks, i);
			size_t x;
			for (x = 0; x < 35; ++x, ++offset) {
				bool dot = block->dots[y * 36 + x];
				line[offset >> 3] &= ~(dot << (~offset & 7));
			}
			if (i + 1 == blocks) {
				bool dot = block->dots[y * 36 + 35];
				line[offset >> 3] &= ~(dot << (~offset & 7));
			}
		}
	}
	for (i = 0; i < blocks + 1; ++i) {
		_eReaderBitAnchor(&rows[stride * 2], stride, i * 35 + 2);
		_eReaderBitAnchor(&rows[stride * 37], stride, i * 35 + 2);
	}
}

bool EReaderScanSaveRaw(const struct EReaderScan* scan, const char* filename, bool strict) {
	size_t blocks = EReaderBlockListSize(&scan->blocks);
	if (!blocks) {
		return false;
	}
	uint8_t* data = malloc(104 * blocks);
	size_t i;
	for (i = 0; i < blocks; ++i) {
		const struct EReaderBlock* block = EReaderBlockListGetConstPointer(&scan->blocks, i);
		uint8_t* datablock = &data[104 * i];
		bool bits[1040] = {0};
		size_t offset = 0;
		size_t y;
		for (y = 2; y < 34; ++y) {
			size_t x;
			for (x = 1; x < 35; ++x) {
				if ((x < 5 || x > 30) && (y < 5 || y > 30)) {
					continue;
				}
				bits[offset] = block->dots[y * 36 + x];
				++offset;
			}
		}
		for (y = 0; y < 104; ++y) {
			int hi = 0;
			hi |= bits[y * 10 + 0] << 4;
			hi |= bits[y * 10 + 1] << 3;
			hi |= bits[y * 10 + 2] << 2;
			hi |= bits[y * 10 + 3] << 1;
			hi |= bits[y * 10 + 4];
			hi = EREADER_NYBBLE_LOOKUP[hi];

			int lo = 0;
			lo |= bits[y * 10 + 5] << 4;
			lo |= bits[y * 10 + 6] << 3;
			lo |= bits[y * 10 + 7] << 2;
			lo |= bits[y * 10 + 8] << 1;
			lo |= bits[y * 10 + 9];
			lo = EREADER_NYBBLE_LOOKUP[lo];

			if (hi < 0) {
				if (strict) {
					free(data);
					return false;
				}
				hi = 0xF;
			}
			if (lo < 0) {
				if (strict) {
					free(data);
					return false;
				}
				lo = 0xF;
			}

			datablock[y] = (hi << 4) | lo;
		}
	}

	struct VFile* vf = VFileOpen(filename, O_CREAT | O_WRONLY | O_TRUNC);
	if (!vf) {
		free(data);
		return false;
	}
	vf->write(vf, data, 104 * blocks);
	vf->close(vf);
	free(data);
	return true;
}

#endif
