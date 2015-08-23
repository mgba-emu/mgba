/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_VIDEO_H
#define GBA_VIDEO_H

#include "util/common.h"

#include "gba/memory.h"
#include "macros.h"

#ifdef COLOR_16_BIT
#define BYTES_PER_PIXEL 2
#else
#define BYTES_PER_PIXEL 4
#endif

#define GBA_R5(X) ((X) & 0x1F)
#define GBA_G5(X) (((X) >> 5) & 0x1F)
#define GBA_B5(X) (((X) >> 10) & 0x1F)

#define GBA_R8(X) (((X) << 3) & 0xF8)
#define GBA_G8(X) (((X) >> 2) & 0xF8)
#define GBA_B8(X) (((X) >> 7) & 0xF8)

enum {
	VIDEO_HORIZONTAL_PIXELS = 240,
	VIDEO_HBLANK_PIXELS = 68,
	VIDEO_HDRAW_LENGTH = 1006,
	VIDEO_HBLANK_LENGTH = 226,
	VIDEO_HORIZONTAL_LENGTH = VIDEO_HDRAW_LENGTH + VIDEO_HBLANK_LENGTH,

	VIDEO_VERTICAL_PIXELS = 160,
	VIDEO_VBLANK_PIXELS = 68,
	VIDEO_VERTICAL_TOTAL_PIXELS = VIDEO_VERTICAL_PIXELS + VIDEO_VBLANK_PIXELS,

	VIDEO_TOTAL_LENGTH = VIDEO_HORIZONTAL_LENGTH * VIDEO_VERTICAL_TOTAL_PIXELS,

	REG_DISPSTAT_MASK = 0xFF38,

	BASE_TILE = 0x00010000
};

enum ObjMode {
	OBJ_MODE_NORMAL = 0,
	OBJ_MODE_SEMITRANSPARENT = 1,
	OBJ_MODE_OBJWIN = 2
};

enum ObjShape {
	OBJ_SHAPE_SQUARE = 0,
	OBJ_SHAPE_HORIZONTAL = 1,
	OBJ_SHAPE_VERTICAL = 2
};

DECL_BITFIELD(GBAObjAttributesA, uint16_t);
DECL_BITS(GBAObjAttributesA, Y, 0, 8);
DECL_BIT(GBAObjAttributesA, Transformed, 8);
DECL_BIT(GBAObjAttributesA, Disable, 9);
DECL_BIT(GBAObjAttributesA, DoubleSize, 9);
DECL_BITS(GBAObjAttributesA, Mode, 10, 2);
DECL_BIT(GBAObjAttributesA, Mosaic, 12);
DECL_BIT(GBAObjAttributesA, 256Color, 13);
DECL_BITS(GBAObjAttributesA, Shape, 14, 2);

DECL_BITFIELD(GBAObjAttributesB, uint16_t);
DECL_BITS(GBAObjAttributesB, X, 0, 9);
DECL_BITS(GBAObjAttributesB, MatIndex, 9, 5);
DECL_BIT(GBAObjAttributesB, HFlip, 12);
DECL_BIT(GBAObjAttributesB, VFlip, 13);
DECL_BITS(GBAObjAttributesB, Size, 14, 2);

DECL_BITFIELD(GBAObjAttributesC, uint16_t);
DECL_BITS(GBAObjAttributesC, Tile, 0, 10);
DECL_BITS(GBAObjAttributesC, Priority, 10, 2);
DECL_BITS(GBAObjAttributesC, Palette, 12, 4);

struct GBAObj {
	GBAObjAttributesA a;
	GBAObjAttributesB b;
	GBAObjAttributesC c;
	uint16_t d;
};

union GBAOAM {
	struct GBAObj obj[128];

	struct GBAOAMMatrix {
		int16_t padding0[3];
		int16_t a;
		int16_t padding1[3];
		int16_t b;
		int16_t padding2[3];
		int16_t c;
		int16_t padding3[3];
		int16_t d;
	} mat[32];

	uint16_t raw[512];
};

#define GBA_TEXT_MAP_TILE(MAP) ((MAP) & 0x03FF)
#define GBA_TEXT_MAP_HFLIP(MAP) ((MAP) & 0x0400)
#define GBA_TEXT_MAP_VFLIP(MAP) ((MAP) & 0x0800)
#define GBA_TEXT_MAP_PALETTE(MAP) (((MAP) & 0xF000) >> 12)

DECL_BITFIELD(GBARegisterDISPCNT, uint16_t);
DECL_BITS(GBARegisterDISPCNT, Mode, 0, 3);
DECL_BIT(GBARegisterDISPCNT, Cgb, 3);
DECL_BIT(GBARegisterDISPCNT, FrameSelect, 4);
DECL_BIT(GBARegisterDISPCNT, HblankIntervalFree, 5);
DECL_BIT(GBARegisterDISPCNT, ObjCharacterMapping, 6);
DECL_BIT(GBARegisterDISPCNT, ForcedBlank, 7);
DECL_BIT(GBARegisterDISPCNT, Bg0Enable, 8);
DECL_BIT(GBARegisterDISPCNT, Bg1Enable, 9);
DECL_BIT(GBARegisterDISPCNT, Bg2Enable, 10);
DECL_BIT(GBARegisterDISPCNT, Bg3Enable, 11);
DECL_BIT(GBARegisterDISPCNT, ObjEnable, 12);
DECL_BIT(GBARegisterDISPCNT, Win0Enable, 13);
DECL_BIT(GBARegisterDISPCNT, Win1Enable, 14);
DECL_BIT(GBARegisterDISPCNT, ObjwinEnable, 15);

DECL_BITFIELD(GBARegisterDISPSTAT, uint16_t);
DECL_BIT(GBARegisterDISPSTAT, InVblank, 0);
DECL_BIT(GBARegisterDISPSTAT, InHblank, 1);
DECL_BIT(GBARegisterDISPSTAT, Vcounter, 2);
DECL_BIT(GBARegisterDISPSTAT, VblankIRQ, 3);
DECL_BIT(GBARegisterDISPSTAT, HblankIRQ, 4);
DECL_BIT(GBARegisterDISPSTAT, VcounterIRQ, 5);
DECL_BITS(GBARegisterDISPSTAT, VcountSetting, 8, 8);

DECL_BITFIELD(GBARegisterBGCNT, uint16_t);
DECL_BITS(GBARegisterBGCNT, Priority, 0, 2);
DECL_BITS(GBARegisterBGCNT, CharBase, 2, 2);
DECL_BIT(GBARegisterBGCNT, Mosaic, 6);
DECL_BIT(GBARegisterBGCNT, 256Color, 7);
DECL_BITS(GBARegisterBGCNT, ScreenBase, 8, 5);
DECL_BIT(GBARegisterBGCNT, Overflow, 13);
DECL_BITS(GBARegisterBGCNT, Size, 14, 2);

DECL_BITFIELD(GBARegisterBLDCNT, uint16_t);
DECL_BIT(GBARegisterBLDCNT, Target1Bg0, 0);
DECL_BIT(GBARegisterBLDCNT, Target1Bg1, 1);
DECL_BIT(GBARegisterBLDCNT, Target1Bg2, 2);
DECL_BIT(GBARegisterBLDCNT, Target1Bg3, 3);
DECL_BIT(GBARegisterBLDCNT, Target1Obj, 4);
DECL_BIT(GBARegisterBLDCNT, Target1Bd, 5);
DECL_BITS(GBARegisterBLDCNT, Effect, 6, 2);
DECL_BIT(GBARegisterBLDCNT, Target2Bg0, 8);
DECL_BIT(GBARegisterBLDCNT, Target2Bg1, 9);
DECL_BIT(GBARegisterBLDCNT, Target2Bg2, 10);
DECL_BIT(GBARegisterBLDCNT, Target2Bg3, 11);
DECL_BIT(GBARegisterBLDCNT, Target2Obj, 12);
DECL_BIT(GBARegisterBLDCNT, Target2Bd, 13);

struct GBAVideoRenderer {
	void (*init)(struct GBAVideoRenderer* renderer);
	void (*reset)(struct GBAVideoRenderer* renderer);
	void (*deinit)(struct GBAVideoRenderer* renderer);

	uint16_t (*writeVideoRegister)(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
	void (*writeVRAM)(struct GBAVideoRenderer* renderer, uint32_t address);
	void (*writePalette)(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
	void (*writeOAM)(struct GBAVideoRenderer* renderer, uint32_t oam);
	void (*drawScanline)(struct GBAVideoRenderer* renderer, int y);
	void (*finishFrame)(struct GBAVideoRenderer* renderer);

	void (*getPixels)(struct GBAVideoRenderer* renderer, unsigned* stride, void** pixels);
	void (*putPixels)(struct GBAVideoRenderer* renderer, unsigned stride, void* pixels);

	uint16_t* palette;
	uint16_t* vram;
	union GBAOAM* oam;

	bool disableBG[4];
	bool disableOBJ;
};

struct GBAVideo {
	struct GBA* p;
	struct GBAVideoRenderer* renderer;

	// VCOUNT
	int vcount;

	int32_t lastHblank;
	int32_t nextHblank;
	int32_t nextEvent;
	int32_t eventDiff;

	int32_t nextHblankIRQ;
	int32_t nextVblankIRQ;
	int32_t nextVcounterIRQ;

	uint16_t palette[SIZE_PALETTE_RAM >> 1];
	uint16_t* vram;
	union GBAOAM oam;

	int32_t frameCounter;
};

void GBAVideoInit(struct GBAVideo* video);
void GBAVideoReset(struct GBAVideo* video);
void GBAVideoDeinit(struct GBAVideo* video);
void GBAVideoAssociateRenderer(struct GBAVideo* video, struct GBAVideoRenderer* renderer);
int32_t GBAVideoProcessEvents(struct GBAVideo* video, int32_t cycles);

void GBAVideoWriteDISPSTAT(struct GBAVideo* video, uint16_t value);

struct GBASerializedState;
void GBAVideoSerialize(const struct GBAVideo* video, struct GBASerializedState* state);
void GBAVideoDeserialize(struct GBAVideo* video, const struct GBASerializedState* state);

extern const int GBAVideoObjSizes[16][2];

#endif
