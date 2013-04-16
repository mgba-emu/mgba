#include "gba-video.h"

#include <limits.h>

void GBAVideoInit(struct GBAVideo* video) {
	video->inHblank = 0;
	video->inVblank = 0;
	video->vcounter = 0;
	video->vblankIRQ = 0;
	video->hblankIRQ = 0;
	video->vcounterIRQ = 0;
	video->vcountSetting = 0;

	video->vcount = -1;

	video->lastHblank = 0;
	video->nextHblank = VIDEO_HDRAW_LENGTH;
	video->nextEvent = video->nextHblank;

	video->nextHblankIRQ = 0;
	video->nextVblankIRQ = 0;
	video->nextVcounterIRQ = 0;
}

int32_t GBAVideoProcessEvents(struct GBAVideo* video, int32_t cycles) {
	return INT_MAX;
}
