#ifndef GBA_VIDEO_H
#define GBA_VIDEO_H

#include "gba-memory.h"

#include <stdint.h>

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

	REG_DISPSTAT_MASK = 0xFF38
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

union GBAOAM {
	struct {
		int y : 8;
		unsigned transformed : 1;
		union {
			unsigned doublesize : 1;
			unsigned disable : 1;
		};
		enum ObjMode mode : 2;
		unsigned mosaic : 1;
		unsigned multipalette : 1;
		enum ObjShape shape : 2;

		int x : 9;
		union {
			unsigned matIndex : 5;
			struct {
				int : 3;
				unsigned hflip : 1;
				unsigned vflip : 1;
			};
		};
		unsigned size : 2;

		unsigned tile : 10;
		unsigned priority : 2;
		unsigned palette : 4;

		int : 16;
	} obj[128];

	struct {
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

struct GBAVideoRenderer {
	void (*deinit)(struct GBAVideoRenderer* renderer);

	uint16_t (*writeVideoRegister)(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
	void (*drawScanline)(struct GBAVideoRenderer* renderer, int y);
	void (*finishFrame)(struct GBAVideoRenderer* renderer);

	uint16_t palette[SIZE_PALETTE_RAM >> 1];
	uint16_t vram[SIZE_VRAM >> 1];
	union GBAOAM oam;
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
};

void GBAVideoInit(struct GBAVideo* video);
int32_t GBAVideoProcessEvents(struct GBAVideo* video, int32_t cycles);

void GBAVideoWriteDISPSTAT(struct GBAVideo* video, uint16_t value);
uint16_t GBAVideoReadDISPSTAT(struct GBAVideo* video);

#endif
