/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_VIDEO_H
#define DS_VIDEO_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>
#include <mgba/core/timing.h>
#include <mgba/internal/gba/video.h>

mLOG_DECLARE_CATEGORY(DS_VIDEO);

enum {
	DS_VIDEO_HORIZONTAL_PIXELS = 256,
	DS_VIDEO_HBLANK_PIXELS = 99,
	DS7_VIDEO_HBLANK_LENGTH = 1613,
	DS9_VIDEO_HBLANK_LENGTH = 1606,
	DS_VIDEO_HORIZONTAL_LENGTH = (DS_VIDEO_HORIZONTAL_PIXELS + DS_VIDEO_HBLANK_PIXELS) * 6,

	DS_VIDEO_VERTICAL_PIXELS = 192,
	DS_VIDEO_VBLANK_PIXELS = 71,
	DS_VIDEO_VERTICAL_TOTAL_PIXELS = DS_VIDEO_VERTICAL_PIXELS + DS_VIDEO_VBLANK_PIXELS,

	DS_VIDEO_TOTAL_LENGTH = DS_VIDEO_HORIZONTAL_LENGTH * DS_VIDEO_VERTICAL_TOTAL_PIXELS,
};

union DSOAM {
	union GBAOAM oam[2];
	uint16_t raw[1024];
};

DECL_BITFIELD(DSRegisterDISPCNT, uint32_t);
DECL_BITS(DSRegisterDISPCNT, Mode, 0, 3);
DECL_BIT(DSRegisterDISPCNT, 3D, 3);
DECL_BIT(DSRegisterDISPCNT, TileObjMapping, 4);
DECL_BIT(DSRegisterDISPCNT, BitmapObj2D, 5);
DECL_BIT(DSRegisterDISPCNT, BitmapObjMapping, 6);
DECL_BIT(DSRegisterDISPCNT, ForcedBlank, 7);
DECL_BIT(DSRegisterDISPCNT, Bg0Enable, 8);
DECL_BIT(DSRegisterDISPCNT, Bg1Enable, 9);
DECL_BIT(DSRegisterDISPCNT, Bg2Enable, 10);
DECL_BIT(DSRegisterDISPCNT, Bg3Enable, 11);
DECL_BIT(DSRegisterDISPCNT, ObjEnable, 12);
DECL_BIT(DSRegisterDISPCNT, Win0Enable, 13);
DECL_BIT(DSRegisterDISPCNT, Win1Enable, 14);
DECL_BIT(DSRegisterDISPCNT, ObjwinEnable, 15);
DECL_BITS(DSRegisterDISPCNT, DispMode, 16, 2);
DECL_BITS(DSRegisterDISPCNT, VRAMBlock, 18, 2);
DECL_BITS(DSRegisterDISPCNT, TileBoundary, 20, 2);
// TODO
DECL_BITS(DSRegisterDISPCNT, CharBase, 24, 3);
DECL_BITS(DSRegisterDISPCNT, ScreenBase, 27, 3);
// TODO
DECL_BIT(DSRegisterDISPCNT, BgExtPalette, 30);
DECL_BIT(DSRegisterDISPCNT, ObjExtPalette, 31);

DECL_BITFIELD(DSRegisterPOWCNT1, uint16_t);
// TODO
DECL_BIT(DSRegisterPOWCNT1, Swap, 15);

DECL_BIT(GBARegisterBGCNT, ExtendedMode0, 2);
DECL_BIT(GBARegisterBGCNT, ExtendedMode1, 7);
DECL_BIT(GBARegisterBGCNT, ExtPaletteSlot, 13);

DECL_BITFIELD(DSRegisterMASTER_BRIGHT, uint16_t);
DECL_BITS(DSRegisterMASTER_BRIGHT, Y, 0, 5);
DECL_BITS(DSRegisterMASTER_BRIGHT, Mode, 14, 2);

struct DSGX;
struct DSVideoRenderer {
	void (*init)(struct DSVideoRenderer* renderer);
	void (*reset)(struct DSVideoRenderer* renderer);
	void (*deinit)(struct DSVideoRenderer* renderer);

	uint16_t (*writeVideoRegister)(struct DSVideoRenderer* renderer, uint32_t address, uint16_t value);
	void (*writePalette)(struct DSVideoRenderer* renderer, uint32_t address, uint16_t value);
	void (*writeOAM)(struct DSVideoRenderer* renderer, uint32_t oam);
	void (*invalidateExtPal)(struct DSVideoRenderer* renderer, bool obj, bool engB, int slot);
	void (*drawScanline)(struct DSVideoRenderer* renderer, int y);
	void (*finishFrame)(struct DSVideoRenderer* renderer);

	void (*getPixels)(struct DSVideoRenderer* renderer, size_t* stride, const void** pixels);
	void (*putPixels)(struct DSVideoRenderer* renderer, size_t stride, const void* pixels);

	uint16_t* palette;
	uint16_t* vram;
	uint16_t* vramABG[32];
	uint16_t* vramAOBJ[32];
	uint16_t* vramABGExtPal[4];
	uint16_t* vramAOBJExtPal;
	uint16_t* vramBBG[32];
	uint16_t* vramBOBJ[32];
	uint16_t* vramBBGExtPal[4];
	uint16_t* vramBOBJExtPal;
	union DSOAM* oam;
	struct DSGX* gx;
};

struct DS;
struct DSVideo {
	struct DS* p;
	struct DSVideoRenderer* renderer;
	struct mTimingEvent event7;
	struct mTimingEvent event9;

	// VCOUNT
	int vcount;

	uint16_t palette[1024];
	uint16_t* vram;
	uint16_t* vramABG[32];
	uint16_t* vramAOBJ[32];
	uint16_t* vramABGExtPal[4];
	uint16_t* vramAOBJExtPal;
	uint16_t* vramBBG[32];
	uint16_t* vramBOBJ[32];
	uint16_t* vramBBGExtPal[4];
	uint16_t* vramBOBJExtPal;
	union DSOAM oam;

	int32_t frameCounter;
	int frameskip;
	int frameskipCounter;
};

void DSVideoInit(struct DSVideo* video);
void DSVideoReset(struct DSVideo* video);
void DSVideoDeinit(struct DSVideo* video);
void DSVideoAssociateRenderer(struct DSVideo* video, struct DSVideoRenderer* renderer);

struct DSCommon;
void DSVideoWriteDISPSTAT(struct DSCommon* dscore, uint16_t value);

struct DSMemory;
void DSVideoConfigureVRAM(struct DS* ds, int index, uint8_t value, uint8_t oldValue);

CXX_GUARD_START

#endif
