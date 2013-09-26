#ifndef VIDEO_SOFTWARE_H
#define VIDEO_SOFTWARE_H

#include "gba-video.h"

#include <pthread.h>

#ifdef COLOR_16_BIT
typedef uint16_t color_t;
#else
typedef uint32_t color_t;
#endif

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
#ifdef COLOR_16_BIT
	GBA_COLOR_WHITE = 0x7FFF,
#else
	GBA_COLOR_WHITE = 0x00F8F8F8,
#endif
	OFFSET_PRIORITY = 29
};

enum PixelFlags {
	FLAG_FINALIZED = 0x80000000,
	FLAG_PRIORITY = 0x60000000,
	FLAG_IS_BACKGROUND = 0x10000000,
	FLAG_UNWRITTEN = 0x08000000,
	FLAG_TARGET_1 = 0x04000000,
	FLAG_TARGET_2 = 0x02000000,
	FLAG_OBJWIN = 0x01000000
};

union WindowRegion {
	struct {
		uint8_t end;
		uint8_t start;
	};
	uint16_t packed;
};

union WindowControl {
	struct {
		unsigned bg0Enable : 1;
		unsigned bg1Enable : 1;
		unsigned bg2Enable : 1;
		unsigned bg3Enable : 1;
		unsigned objEnable : 1;
		unsigned blendEnable : 1;
		unsigned : 2;
	};
	uint8_t packed;
	int8_t priority;
};

#define MAX_WINDOW 5

struct Window {
	uint8_t endX;
	union WindowControl control;
};

struct GBAVideoSoftwareRenderer {
	struct GBAVideoRenderer d;

	color_t* outputBuffer;
	unsigned outputBufferStride;

	union GBARegisterDISPCNT dispcnt;

	uint32_t row[VIDEO_HORIZONTAL_PIXELS];
	uint32_t spriteLayer[VIDEO_HORIZONTAL_PIXELS];

	// BLDCNT
	unsigned target1Obj;
	unsigned target1Bd;
	unsigned target2Obj;
	unsigned target2Bd;
	enum BlendEffect blendEffect;
	color_t normalPalette[512];
	color_t variantPalette[512];

	uint16_t blda;
	uint16_t bldb;
	uint16_t bldy;

	union WindowRegion win0H;
	union WindowRegion win0V;
	union WindowRegion win1H;
	union WindowRegion win1V;

	union WindowControl win0;
	union WindowControl win1;
	union WindowControl winout;
	union WindowControl objwin;

	union WindowControl currentWindow;

	int nWindows;
	struct Window windows[MAX_WINDOW];

	struct GBAVideoSoftwareBackground bg[4];

	int start;
	int end;

	uint32_t enabledBitmap[4];

	pthread_mutex_t mutex;
	pthread_cond_t upCond;
	pthread_cond_t downCond;
};

void GBAVideoSoftwareRendererCreate(struct GBAVideoSoftwareRenderer* renderer);

#endif
