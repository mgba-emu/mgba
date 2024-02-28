/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_VIDEO_H
#define GB_VIDEO_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>
#include <mgba/core/timing.h>
#include <mgba/gb/interface.h>

mLOG_DECLARE_CATEGORY(GB_VIDEO);

enum {
	GB_VIDEO_HORIZONTAL_PIXELS = 160,
	GB_VIDEO_VERTICAL_PIXELS = 144,
	GB_VIDEO_VBLANK_PIXELS = 10,
	GB_VIDEO_VERTICAL_TOTAL_PIXELS = 154,

	SGB_VIDEO_HORIZONTAL_PIXELS = 256,
	SGB_VIDEO_VERTICAL_PIXELS = 224,

	// TODO: Figure out exact lengths
	GB_VIDEO_MODE_2_LENGTH = 80,
	GB_VIDEO_MODE_3_LENGTH_BASE = 172,
	GB_VIDEO_MODE_0_LENGTH_BASE = 204,

	GB_VIDEO_HORIZONTAL_LENGTH = 456,

	GB_VIDEO_TOTAL_LENGTH = 70224,

	GB_VIDEO_MAX_OBJ = 40,
	GB_VIDEO_MAX_LINE_OBJ = 10,

	GB_BASE_MAP = 0x1800,
	GB_SIZE_MAP = 0x0400,

	SGB_SIZE_CHAR_RAM = 0x2000,
	SGB_SIZE_MAP_RAM = 0x1000,
	SGB_SIZE_PAL_RAM = 0x1000,
	SGB_SIZE_ATF_RAM = 0x1000
};

DECL_BITFIELD(GBObjAttributes, uint8_t);
DECL_BITS(GBObjAttributes, CGBPalette, 0, 3);
DECL_BIT(GBObjAttributes, Bank, 3);
DECL_BIT(GBObjAttributes, Palette, 4);
DECL_BIT(GBObjAttributes, XFlip, 5);
DECL_BIT(GBObjAttributes, YFlip, 6);
DECL_BIT(GBObjAttributes, Priority, 7);

DECL_BITFIELD(SGBBgAttributes, uint16_t);
DECL_BITS(SGBBgAttributes, Tile, 0, 10);
DECL_BITS(SGBBgAttributes, Palette, 10, 3);
DECL_BIT(SGBBgAttributes, Priority, 13);
DECL_BIT(SGBBgAttributes, XFlip, 14);
DECL_BIT(SGBBgAttributes, YFlip, 15);

struct GBObj {
	uint8_t y;
	uint8_t x;
	uint8_t tile;
	GBObjAttributes attr;
};

union GBOAM {
	struct GBObj obj[GB_VIDEO_MAX_OBJ];
	uint8_t raw[GB_VIDEO_MAX_OBJ * 4];
};

struct mCacheSet;
struct GBVideoRenderer {
	void (*init)(struct GBVideoRenderer* renderer, enum GBModel model, bool borders);
	void (*deinit)(struct GBVideoRenderer* renderer);

	uint8_t (*writeVideoRegister)(struct GBVideoRenderer* renderer, uint16_t address, uint8_t value);
	void (*writeSGBPacket)(struct GBVideoRenderer* renderer, uint8_t* data);
	void (*writeVRAM)(struct GBVideoRenderer* renderer, uint16_t address);
	void (*writePalette)(struct GBVideoRenderer* renderer, int index, uint16_t value);
	void (*writeOAM)(struct GBVideoRenderer* renderer, uint16_t oam);
	void (*drawRange)(struct GBVideoRenderer* renderer, int startX, int endX, int y);
	void (*finishScanline)(struct GBVideoRenderer* renderer, int y);
	void (*finishFrame)(struct GBVideoRenderer* renderer);
	void (*enableSGBBorder)(struct GBVideoRenderer* renderer, bool enable);

	void (*getPixels)(struct GBVideoRenderer* renderer, size_t* stride, const void** pixels);
	void (*putPixels)(struct GBVideoRenderer* renderer, size_t stride, const void* pixels);

	uint8_t* vram;
	union GBOAM* oam;
	struct mCacheSet* cache;

	uint8_t* sgbCharRam;
	uint8_t* sgbMapRam;
	uint8_t* sgbPalRam;
	int sgbRenderMode;
	uint8_t* sgbAttributes;
	uint8_t* sgbAttributeFiles;

	bool disableBG;
	bool disableOBJ;
	bool disableWIN;

	bool highlightBG;
	bool highlightOBJ[GB_VIDEO_MAX_OBJ];
	bool highlightWIN;
	color_t highlightColor;
	uint8_t highlightAmount;
};

DECL_BITFIELD(GBRegisterLCDC, uint8_t);
DECL_BIT(GBRegisterLCDC, BgEnable, 0);
DECL_BIT(GBRegisterLCDC, ObjEnable, 1);
DECL_BIT(GBRegisterLCDC, ObjSize, 2);
DECL_BIT(GBRegisterLCDC, TileMap, 3);
DECL_BIT(GBRegisterLCDC, TileData, 4);
DECL_BIT(GBRegisterLCDC, Window, 5);
DECL_BIT(GBRegisterLCDC, WindowTileMap, 6);
DECL_BIT(GBRegisterLCDC, Enable, 7);

DECL_BITFIELD(GBRegisterSTAT, uint8_t);
DECL_BITS(GBRegisterSTAT, Mode, 0, 2);
DECL_BIT(GBRegisterSTAT, LYC, 2);
DECL_BIT(GBRegisterSTAT, HblankIRQ, 3);
DECL_BIT(GBRegisterSTAT, VblankIRQ, 4);
DECL_BIT(GBRegisterSTAT, OAMIRQ, 5);
DECL_BIT(GBRegisterSTAT, LYCIRQ, 6);

struct GBVideo {
	struct GB* p;
	struct GBVideoRenderer* renderer;

	int x;
	int ly;
	GBRegisterSTAT stat;

	int mode;

	struct mTimingEvent modeEvent;
	struct mTimingEvent frameEvent;

	int32_t dotClock;

	uint8_t* vram;
	uint8_t* vramBank;
	int vramCurrentBank;

	union GBOAM oam;
	int objMax;

	int bcpIndex;
	bool bcpIncrement;
	int ocpIndex;
	bool ocpIncrement;
	uint8_t sgbCommandHeader;
	int sgbBufferIndex;
	uint8_t sgbPacketBuffer[128];

	uint16_t dmgPalette[12];
	uint16_t palette[64];

	bool sgbBorders;

	uint32_t frameCounter;
	int frameskip;
	int frameskipCounter;
};

void GBVideoInit(struct GBVideo* video);
void GBVideoReset(struct GBVideo* video);
void GBVideoDeinit(struct GBVideo* video);

void GBVideoDummyRendererCreate(struct GBVideoRenderer*);
void GBVideoAssociateRenderer(struct GBVideo* video, struct GBVideoRenderer* renderer);

void GBVideoSkipBIOS(struct GBVideo* video);
void GBVideoProcessDots(struct GBVideo* video, uint32_t cyclesLate);

void GBVideoWriteLCDC(struct GBVideo* video, GBRegisterLCDC value);
void GBVideoWriteSTAT(struct GBVideo* video, GBRegisterSTAT value);
void GBVideoWriteLYC(struct GBVideo* video, uint8_t value);
void GBVideoWritePalette(struct GBVideo* video, uint16_t address, uint8_t value);
void GBVideoSwitchBank(struct GBVideo* video, uint8_t value);

void GBVideoDisableCGB(struct GBVideo* video);
void GBVideoSetPalette(struct GBVideo* video, unsigned index, uint32_t color);

void GBVideoWriteSGBPacket(struct GBVideo* video, uint8_t* data);

struct GBSerializedState;
void GBVideoSerialize(const struct GBVideo* video, struct GBSerializedState* state);
void GBVideoDeserialize(struct GBVideo* video, const struct GBSerializedState* state);

CXX_GUARD_END

#endif
