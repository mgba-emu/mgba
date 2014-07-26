#ifndef GBA_VIDEO_H
#define GBA_VIDEO_H

#include "common.h"

#include "gba-memory.h"

#ifdef COLOR_16_BIT
#define BYTES_PER_PIXEL 2
#else
#define BYTES_PER_PIXEL 4
#endif

enum {
	VIDEO_CYCLES_PER_PIXEL = 4,

	VIDEO_HORIZONTAL_PIXELS = 240,
	VIDEO_HBLANK_PIXELS = 68,
	VIDEO_HDRAW_LENGTH = 1006,
	VIDEO_HBLANK_LENGTH = 226,
	VIDEO_HORIZONTAL_LENGTH = 1232,

	VIDEO_VERTICAL_PIXELS = 160,
	VIDEO_VBLANK_PIXELS = 68,
	VIDEO_VERTICAL_TOTAL_PIXELS = 228,

	VIDEO_TOTAL_LENGTH = 280896,

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

union GBAColor {
	struct {
		unsigned r : 5;
		unsigned g : 5;
		unsigned b : 5;
	};
	uint16_t packed;
};

struct GBAObj {
	unsigned y : 8;
	unsigned transformed : 1;
	unsigned disable : 1;
	enum ObjMode mode : 2;
	unsigned mosaic : 1;
	unsigned multipalette : 1;
	enum ObjShape shape : 2;

	int x : 9;
	int : 3;
	unsigned hflip : 1;
	unsigned vflip : 1;
	unsigned size : 2;

	unsigned tile : 10;
	unsigned priority : 2;
	unsigned palette : 4;

	int : 16;
};

struct GBATransformedObj {
	unsigned y : 8;
	unsigned transformed : 1;
	unsigned doublesize : 1;
	enum ObjMode mode : 2;
	unsigned mosaic : 1;
	unsigned multipalette : 1;
	enum ObjShape shape : 2;

	int x : 9;
	unsigned matIndex : 5;
	unsigned size : 2;

	unsigned tile : 10;
	unsigned priority : 2;
	unsigned palette : 4;

	int : 16;
};

union GBAOAM {
	struct GBAObj obj[128];
	struct GBATransformedObj tobj[128];

	struct GBAOAMMatrix {
		int : 16;
		int : 16;
		int : 16;
		int a : 16;
		int : 16;
		int : 16;
		int : 16;
		int b : 16;
		int : 16;
		int : 16;
		int : 16;
		int c : 16;
		int : 16;
		int : 16;
		int : 16;
		int d : 16;
	} mat[32];

	uint16_t raw[512];
};

#define GBA_TEXT_MAP_TILE(MAP) ((MAP) & 0x03FF)
#define GBA_TEXT_MAP_HFLIP(MAP) ((MAP) & 0x0400)
#define GBA_TEXT_MAP_VFLIP(MAP) ((MAP) & 0x0800)
#define GBA_TEXT_MAP_PALETTE(MAP) (((MAP) & 0xF000) >> 12)

union GBARegisterDISPCNT {
	struct {
		unsigned mode : 3;
		unsigned cgb : 1;
		unsigned frameSelect : 1;
		unsigned hblankIntervalFree : 1;
		unsigned objCharacterMapping : 1;
		unsigned forcedBlank : 1;
		unsigned bg0Enable : 1;
		unsigned bg1Enable : 1;
		unsigned bg2Enable : 1;
		unsigned bg3Enable : 1;
		unsigned objEnable : 1;
		unsigned win0Enable : 1;
		unsigned win1Enable : 1;
		unsigned objwinEnable : 1;
	};
	uint16_t packed;
};

union GBARegisterDISPSTAT {
	struct {
		unsigned inVblank : 1;
		unsigned inHblank : 1;
		unsigned vcounter : 1;
		unsigned vblankIRQ : 1;
		unsigned hblankIRQ : 1;
		unsigned vcounterIRQ : 1;
		unsigned : 2;
		unsigned vcountSetting : 8;
	};
	uint32_t packed;
};

union GBARegisterBGCNT {
	struct {
		unsigned priority : 2;
		unsigned charBase : 2;
		unsigned : 2;
		unsigned mosaic : 1;
		unsigned multipalette : 1;
		unsigned screenBase : 5;
		unsigned overflow : 1;
		unsigned size : 2;
	};
	uint16_t packed;
};

struct GBAVideoRenderer {
	void (*init)(struct GBAVideoRenderer* renderer);
	void (*reset)(struct GBAVideoRenderer* renderer);
	void (*deinit)(struct GBAVideoRenderer* renderer);

	uint16_t (*writeVideoRegister)(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
	void (*writePalette)(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
	void (*writeOAM)(struct GBAVideoRenderer* renderer, uint32_t oam);
	void (*drawScanline)(struct GBAVideoRenderer* renderer, int y);
	void (*finishFrame)(struct GBAVideoRenderer* renderer);

	void (*getPixels)(struct GBAVideoRenderer* renderer, unsigned* stride, void** pixels);

	uint16_t* palette;
	uint16_t* vram;
	union GBAOAM* oam;
};

struct GBAVideo {
	struct GBA* p;
	struct GBAVideoRenderer* renderer;

	// DISPSTAT
	int inHblank;
	int inVblank;
	int vcounter;
	int vblankIRQ;
	int hblankIRQ;
	int vcounterIRQ;
	int vcountSetting;

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
};

void GBAVideoInit(struct GBAVideo* video);
void GBAVideoReset(struct GBAVideo* video);
void GBAVideoDeinit(struct GBAVideo* video);
void GBAVideoAssociateRenderer(struct GBAVideo* video, struct GBAVideoRenderer* renderer);
int32_t GBAVideoProcessEvents(struct GBAVideo* video, int32_t cycles);

void GBAVideoWriteDISPSTAT(struct GBAVideo* video, uint16_t value);

struct GBASerializedState;
void GBAVideoSerialize(struct GBAVideo* video, struct GBASerializedState* state);
void GBAVideoDeserialize(struct GBAVideo* video, struct GBASerializedState* state);

#endif
