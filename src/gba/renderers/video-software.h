#ifndef VIDEO_SOFTWARE_H
#define VIDEO_SOFTWARE_H

#include "common.h"

#include "gba-video.h"

#ifdef COLOR_16_BIT
typedef uint16_t color_t;
#else
typedef uint32_t color_t;
#endif

struct GBAVideoSoftwareSprite {
	union {
		struct GBAObj obj;
		struct GBATransformedObj tobj;
	};
	int y;
	int endY;
};

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
#ifdef COLOR_5_6_5
	GBA_COLOR_WHITE = 0xFFDF,
#else
	GBA_COLOR_WHITE = 0x7FFF,
#endif
#else
	GBA_COLOR_WHITE = 0x00F8F8F8,
#endif
	OFFSET_PRIORITY = 30,
	OFFSET_INDEX = 28,
};

enum PixelFlags {
	FLAG_PRIORITY = 0xC0000000,
	FLAG_INDEX = 0x30000000,
	FLAG_IS_BACKGROUND = 0x08000000,
	FLAG_UNWRITTEN = 0xFC000000,
	FLAG_TARGET_1 = 0x02000000,
	FLAG_TARGET_2 = 0x01000000,
	FLAG_OBJWIN = 0x01000000,

	FLAG_ORDER_MASK = 0xF8000000
};

#define IS_WRITABLE(PIXEL) ((PIXEL) & 0xFE000000)

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
	int anyTarget2;

	uint16_t blda;
	uint16_t bldb;
	uint16_t bldy;

	union {
		struct {
			unsigned bgH : 4;
			unsigned bgV : 4;
			unsigned objH : 4;
			unsigned objV : 4;
		};
		uint16_t packed;
	} mosaic;

	struct WindowN {
		union WindowRegion h;
		union WindowRegion v;
		union WindowControl control;
	} winN[2];

	union WindowControl winout;
	union WindowControl objwin;

	union WindowControl currentWindow;

	int nWindows;
	struct Window windows[MAX_WINDOW];

	struct GBAVideoSoftwareBackground bg[4];

	int oamDirty;
	int oamMax;
	struct GBAVideoSoftwareSprite sprites[128];

	int start;
	int end;
};

void GBAVideoSoftwareRendererCreate(struct GBAVideoSoftwareRenderer* renderer);

#endif
