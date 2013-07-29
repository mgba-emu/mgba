#ifndef VIDEO_SOFTWARE_H
#define VIDEO_SOFTWARE_H

#include "gba-video.h"

#include <pthread.h>

struct GBAVideoSoftwareBackground {
	int index;
	int enabled;
	int priority;
	uint32_t charBase;
	int mosaic;
	int multipalette;
	uint32_t screenBase;
	int overflow;
	int size;
	int target1;
	int target2;
	uint16_t x;
	uint16_t y;
	int32_t refx;
	int32_t refy;
	int16_t dx;
	int16_t dmx;
	int16_t dy;
	int16_t dmy;
	int32_t sx;
	int32_t sy;
};

enum BlendEffect {
	BLEND_NONE = 0,
	BLEND_ALPHA = 1,
	BLEND_BRIGHTEN = 2,
	BLEND_DARKEN = 3
};

enum {
	GBA_COLOR_WHITE = 0x00F8F8F8,
	OFFSET_PRIORITY = 29
};

enum PixelFlags {
	FLAG_FINALIZED = 0x80000000,
	FLAG_PRIORITY = 0x60000000,
	FLAG_IS_BACKGROUND = 0x10000000,
	FLAG_UNWRITTEN = 0x08000000,
	FLAG_TARGET_1 = 0x04000000,
	FLAG_TARGET_2 = 0x02000000
};

union Window {
	struct {
		uint8_t end;
		uint8_t start;
	};
	uint16_t packed;
};

union WindowControl {
	struct {
		unsigned bg0EnableLo : 1;
		unsigned bg1EnableLo : 1;
		unsigned bg2EnableLo : 1;
		unsigned bg3EnableLo : 1;
		unsigned objEnableLo : 1;
		unsigned blendEnableLo : 1;
		unsigned : 2;
		unsigned bg0EnableHi : 1;
		unsigned bg1EnableHi : 1;
		unsigned bg2EnableHi : 1;
		unsigned bg3EnableHi : 1;
		unsigned objEnableHi : 1;
		unsigned blendEnableHi : 1;
		unsigned : 2;
	};
	uint16_t packed;
};

struct GBAVideoSoftwareRenderer {
	struct GBAVideoRenderer d;

	uint32_t* outputBuffer;
	unsigned outputBufferStride;

	union GBARegisterDISPCNT dispcnt;

	uint32_t spriteLayer[VIDEO_HORIZONTAL_PIXELS];

	// BLDCNT
	unsigned target1Obj;
	unsigned target1Bd;
	unsigned target2Obj;
	unsigned target2Bd;
	enum BlendEffect blendEffect;
	uint32_t normalPalette[512];
	uint32_t variantPalette[512];

	uint16_t blda;
	uint16_t bldb;
	uint16_t bldy;

	union Window win0H;
	union Window win0V;
	union Window win1H;
	union Window win1V;

	union WindowControl winin;
	union WindowControl winout;

	struct GBAVideoSoftwareBackground bg[4];

	uint32_t* row;
	int start;
	int end;

	uint32_t enabledBitmap[4];

	pthread_mutex_t mutex;
	pthread_cond_t upCond;
	pthread_cond_t downCond;
};

void GBAVideoSoftwareRendererCreate(struct GBAVideoSoftwareRenderer* renderer);

#endif
