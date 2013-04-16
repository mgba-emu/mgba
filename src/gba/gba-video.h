#ifndef GBA_VIDEO_H
#define GBA_VIDEO_H

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

struct GBAVideoRenderer {
	void (*init)(struct GBAVideoRenderer* renderer);
	void (*deinit)(struct GBAVideoRenderer* renderer);

	uint16_t (*writeVideoRegister)(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
	void (*drawScanline)(struct GBAVideoRenderer* video, int y);
	void (*finishFrame)(struct GBAVideoRenderer* video);
};

struct GBAVideo {
	struct GBAVideoRenderer renderer;

	// DISPSTAT
	int inHblank;
	int inVblank;
	int vcounter;
	int blankIRQ;
	int vblankIRQ;
	int hblankIRQ;
	int vcounterIRQ;
	int vcountSetting;

	// VCOUNT
	int vcount;

	int32_t lastHblank;
	int32_t nextHblank;
	int32_t nextEvent;

	int32_t nextHblankIRQ;
	int32_t nextVblankIRQ;
	int32_t nextVcounterIRQ;
};

void GBAVideoInit(struct GBAVideo* video);
int32_t GBAVideoProcessEvents(struct GBAVideo* video, int32_t cycles);

#endif
