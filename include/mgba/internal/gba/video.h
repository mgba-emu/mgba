/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_VIDEO_H
#define GBA_VIDEO_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>
#include <mgba/core/timing.h>
#include <mgba/gba/interface.h>

mLOG_DECLARE_CATEGORY(GBA_VIDEO);

enum {
	VIDEO_HBLANK_PIXELS = 68,
	VIDEO_HDRAW_LENGTH = 1008,
	VIDEO_HBLANK_LENGTH = 224,
	VIDEO_HORIZONTAL_LENGTH = 1232,

	VIDEO_VBLANK_PIXELS = 68,
	VIDEO_VERTICAL_TOTAL_PIXELS = 228,

	VIDEO_TOTAL_LENGTH = 280896,

	OBJ_HBLANK_FREE_LENGTH = 954,
	OBJ_LENGTH = 1210,

	BASE_TILE = 0x00010000
};

enum GBAVideoObjMode {
	OBJ_MODE_NORMAL = 0,
	OBJ_MODE_SEMITRANSPARENT = 1,
	OBJ_MODE_OBJWIN = 2
};

enum GBAVideoObjShape {
	OBJ_SHAPE_SQUARE = 0,
	OBJ_SHAPE_HORIZONTAL = 1,
	OBJ_SHAPE_VERTICAL = 2
};

enum GBAVideoBlendEffect {
	BLEND_NONE = 0,
	BLEND_ALPHA = 1,
	BLEND_BRIGHTEN = 2,
	BLEND_DARKEN = 3
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

struct GBAOAMMatrix {
	int16_t padding0[3];
	int16_t a;
	int16_t padding1[3];
	int16_t b;
	int16_t padding2[3];
	int16_t c;
	int16_t padding3[3];
	int16_t d;
};

union GBAOAM {
	struct GBAObj obj[128];
	struct GBAOAMMatrix mat[32];
	uint16_t raw[512];
};

struct GBAVideoWindowRegion {
	uint8_t end;
	uint8_t start;
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

DECL_BITFIELD(GBAWindowControl, uint8_t);
DECL_BIT(GBAWindowControl, Bg0Enable, 0);
DECL_BIT(GBAWindowControl, Bg1Enable, 1);
DECL_BIT(GBAWindowControl, Bg2Enable, 2);
DECL_BIT(GBAWindowControl, Bg3Enable, 3);
DECL_BIT(GBAWindowControl, ObjEnable, 4);
DECL_BIT(GBAWindowControl, BlendEnable, 5);

DECL_BITFIELD(GBAMosaicControl, uint16_t);
DECL_BITS(GBAMosaicControl, BgH, 0, 4);
DECL_BITS(GBAMosaicControl, BgV, 4, 4);
DECL_BITS(GBAMosaicControl, ObjH, 8, 4);
DECL_BITS(GBAMosaicControl, ObjV, 12, 4);

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

	void (*getPixels)(struct GBAVideoRenderer* renderer, size_t* stride, const void** pixels);
	void (*putPixels)(struct GBAVideoRenderer* renderer, size_t stride, const void* pixels);

	uint16_t* palette;
	uint16_t* vram;
	union GBAOAM* oam;
	struct mCacheSet* cache;

	bool disableBG[4];
	bool disableOBJ;
	bool disableWIN[2];
	bool disableOBJWIN;

	bool highlightBG[4];
	bool highlightOBJ[128];
	color_t highlightColor;
	uint8_t highlightAmount;
};

struct GBAVideo {
	struct GBA* p;
	struct GBAVideoRenderer* renderer;
	struct mTimingEvent event;

	int vcount;
	int shouldStall;

	uint16_t palette[512];
	uint16_t* vram;
	union GBAOAM oam;

	uint32_t frameCounter;
	int frameskip;
	int frameskipCounter;
};

void GBAVideoInit(struct GBAVideo* video);
void GBAVideoReset(struct GBAVideo* video);
void GBAVideoDeinit(struct GBAVideo* video);

void GBAVideoDummyRendererCreate(struct GBAVideoRenderer*);
void GBAVideoAssociateRenderer(struct GBAVideo* video, struct GBAVideoRenderer* renderer);

void GBAVideoWriteDISPSTAT(struct GBAVideo* video, uint16_t value);

struct GBASerializedState;
void GBAVideoSerialize(const struct GBAVideo* video, struct GBASerializedState* state);
void GBAVideoDeserialize(struct GBAVideo* video, const struct GBASerializedState* state);

extern MGBA_EXPORT const int GBAVideoObjSizes[16][2];

CXX_GUARD_END

#endif
