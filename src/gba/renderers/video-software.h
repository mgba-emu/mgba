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
	uint16_t x;
	uint16_t y;
	uint32_t refx;
	uint32_t refy;
	uint16_t dx;
	uint16_t dmx;
	uint16_t dy;
	uint16_t dmy;
	uint32_t sx;
	uint32_t sy;
	uint16_t internalBuffer[VIDEO_HORIZONTAL_PIXELS];
};

struct GBAVideoSoftwareRenderer {
	struct GBAVideoRenderer d;

	uint16_t* outputBuffer;
	unsigned outputBufferStride;

	union GBARegisterDISPCNT dispcnt;

	struct GBAVideoSoftwareBackground bg[4];
	struct GBAVideoSoftwareBackground* sortedBg[4];

	pthread_mutex_t mutex;
	pthread_cond_t cond;
};

void GBAVideoSoftwareRendererCreate(struct GBAVideoSoftwareRenderer* renderer);

#endif
