#include "gba-video.h"

#include "gba.h"
#include "gba-io.h"
#include "gba-serialize.h"
#include "gba-thread.h"

#include "util/memory.h"

static void GBAVideoDummyRendererInit(struct GBAVideoRenderer* renderer);
static void GBAVideoDummyRendererDeinit(struct GBAVideoRenderer* renderer);
static uint16_t GBAVideoDummyRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static void GBAVideoDummyRendererDrawScanline(struct GBAVideoRenderer* renderer, int y);
static void GBAVideoDummyRendererFinishFrame(struct GBAVideoRenderer* renderer);

static struct GBAVideoRenderer dummyRenderer = {
	.init = GBAVideoDummyRendererInit,
	.deinit = GBAVideoDummyRendererDeinit,
	.writeVideoRegister = GBAVideoDummyRendererWriteVideoRegister,
	.drawScanline = GBAVideoDummyRendererDrawScanline,
	.finishFrame = GBAVideoDummyRendererFinishFrame
};

void GBAVideoInit(struct GBAVideo* video) {
	video->renderer = &dummyRenderer;

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
	video->eventDiff = video->nextEvent;

	video->nextHblankIRQ = 0;
	video->nextVblankIRQ = 0;
	video->nextVcounterIRQ = 0;

	video->vram = anonymousMemoryMap(SIZE_VRAM);

	int i;
	for (i = 0; i < 128; ++i) {
		video->oam.obj[i].disable = 1;
	}
}

void GBAVideoDeinit(struct GBAVideo* video) {
	GBAVideoAssociateRenderer(video, &dummyRenderer);
	mappedMemoryFree(video->vram, SIZE_VRAM);
}

void GBAVideoAssociateRenderer(struct GBAVideo* video, struct GBAVideoRenderer* renderer) {
	video->renderer->deinit(video->renderer);
	video->renderer = renderer;
	renderer->palette = video->palette;
	renderer->vram = video->vram;
	renderer->oam = &video->oam;
	video->renderer->init(video->renderer);
}

int32_t GBAVideoProcessEvents(struct GBAVideo* video, int32_t cycles) {
	video->nextEvent -= cycles;
	video->eventDiff += cycles;
	if (video->nextEvent <= 0) {
		int32_t lastEvent = video->nextEvent;
		video->lastHblank -= video->eventDiff;
		video->nextHblank -= video->eventDiff;
		video->nextHblankIRQ -= video->eventDiff;
		video->nextVcounterIRQ -= video->eventDiff;

		if (video->inHblank) {
			// End Hblank
			video->inHblank = 0;
			video->nextEvent = video->nextHblank;

			++video->vcount;
			video->p->memory.io[REG_VCOUNT >> 1] = video->vcount;

			switch (video->vcount) {
			case VIDEO_VERTICAL_PIXELS:
				video->inVblank = 1;
				if (GBASyncDrawingFrame(video->p->sync)) {
					video->renderer->finishFrame(video->renderer);
				}
				video->nextVblankIRQ = video->nextEvent + VIDEO_TOTAL_LENGTH;
				GBAMemoryRunVblankDMAs(video->p, lastEvent);
				if (video->vblankIRQ) {
					GBARaiseIRQ(video->p, IRQ_VBLANK);
				}
				GBASyncPostFrame(video->p->sync);
				break;
			case VIDEO_VERTICAL_TOTAL_PIXELS - 1:
				video->inVblank = 0;
				break;
			case VIDEO_VERTICAL_TOTAL_PIXELS:
				video->vcount = 0;
				video->p->memory.io[REG_VCOUNT >> 1] = 0;
				break;
			}

			video->vcounter = video->vcount == video->vcountSetting;
			if (video->vcounter && video->vcounterIRQ) {
				GBARaiseIRQ(video->p, IRQ_VCOUNTER);
				video->nextVcounterIRQ += VIDEO_TOTAL_LENGTH;
			}

			if (video->vcount < VIDEO_VERTICAL_PIXELS && GBASyncDrawingFrame(video->p->sync)) {
				video->renderer->drawScanline(video->renderer, video->vcount);
			}
		} else {
			// Begin Hblank
			video->inHblank = 1;
			video->lastHblank = video->nextHblank;
			video->nextEvent = video->lastHblank + VIDEO_HBLANK_LENGTH;
			video->nextHblank = video->nextEvent + VIDEO_HDRAW_LENGTH;
			video->nextHblankIRQ = video->nextHblank;

			if (video->vcount < VIDEO_VERTICAL_PIXELS) {
				GBAMemoryRunHblankDMAs(video->p, lastEvent);
			}
			if (video->hblankIRQ) {
				GBARaiseIRQ(video->p, IRQ_HBLANK);
			}
		}

		video->eventDiff = 0;
	}
	video->p->memory.io[REG_DISPSTAT >> 1] &= 0xFFF8;
	video->p->memory.io[REG_DISPSTAT >> 1] |= (video->inVblank) | (video->inHblank << 1) | (video->vcounter << 2);
	return video->nextEvent;
}

void GBAVideoWriteDISPSTAT(struct GBAVideo* video, uint16_t value) {
	union GBARegisterDISPSTAT dispstat;
	dispstat.packed = value;
	video->vblankIRQ = dispstat.vblankIRQ;
	video->hblankIRQ = dispstat.hblankIRQ;
	video->vcounterIRQ = dispstat.vcounterIRQ;
	video->vcountSetting = dispstat.vcountSetting;

	if (video->vcounterIRQ) {
		// FIXME: this can be too late if we're in the middle of an Hblank
		video->nextVcounterIRQ = video->nextHblank + VIDEO_HBLANK_LENGTH + (video->vcountSetting - video->vcount) * VIDEO_HORIZONTAL_LENGTH;
		if (video->nextVcounterIRQ < video->nextEvent) {
			video->nextVcounterIRQ += VIDEO_TOTAL_LENGTH;
		}
	}
}

static void GBAVideoDummyRendererInit(struct GBAVideoRenderer* renderer) {
	(void)(renderer);
	// Nothing to do
}

static void GBAVideoDummyRendererDeinit(struct GBAVideoRenderer* renderer) {
	(void)(renderer);
	// Nothing to do
}

static uint16_t GBAVideoDummyRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	(void)(renderer);
	(void)(address);
	return value;
}

static void GBAVideoDummyRendererDrawScanline(struct GBAVideoRenderer* renderer, int y) {
	(void)(renderer);
	(void)(y);
	// Nothing to do
}

static void GBAVideoDummyRendererFinishFrame(struct GBAVideoRenderer* renderer) {
	(void)(renderer);
	// Nothing to do
}

void GBAVideoSerialize(struct GBAVideo* video, struct GBASerializedState* state) {
	memcpy(state->vram, video->renderer->vram, SIZE_VRAM);
	memcpy(state->oam, video->oam.raw, SIZE_OAM);
	memcpy(state->pram, video->palette, SIZE_PALETTE_RAM);
	union GBARegisterDISPSTAT dispstat;
	dispstat.inVblank = video->inVblank;
	dispstat.inHblank = video->inHblank;
	dispstat.vcounter = video->vcounter;
	dispstat.vblankIRQ = video->vblankIRQ;
	dispstat.hblankIRQ = video->hblankIRQ;
	dispstat.vcounterIRQ = video->vcounterIRQ;
	dispstat.vcountSetting = video->vcountSetting;
	state->io[REG_DISPSTAT >> 1] = dispstat.packed;
	state->video.nextEvent = video->nextEvent;
	state->video.eventDiff = video->eventDiff;
	state->video.lastHblank = video->lastHblank;
	state->video.nextHblank = video->nextHblank;
	state->video.nextHblankIRQ = video->nextHblankIRQ;
	state->video.nextVblankIRQ = video->nextVblankIRQ;
	state->video.nextVcounterIRQ = video->nextVcounterIRQ;
}

void GBAVideoDeserialize(struct GBAVideo* video, struct GBASerializedState* state) {
	memcpy(video->renderer->vram, state->vram, SIZE_VRAM);
	int i;
	for (i = 0; i < SIZE_OAM; i += 2) {
		GBAStore16(video->p->cpu, BASE_OAM | i, state->oam[i >> 1], 0);
	}
	for (i = 0; i < SIZE_PALETTE_RAM; i += 2) {
		GBAStore16(video->p->cpu, BASE_PALETTE_RAM | i, state->pram[i >> 1], 0);
	}
	union GBARegisterDISPSTAT dispstat;
	dispstat.packed = state->io[REG_DISPSTAT >> 1];
	video->inVblank = dispstat.inVblank;
	video->inHblank = dispstat.inHblank;
	video->vcounter = dispstat.vcounter;
	video->vblankIRQ = dispstat.vblankIRQ;
	video->hblankIRQ = dispstat.hblankIRQ;
	video->vcounterIRQ = dispstat.vcounterIRQ;
	video->vcountSetting = dispstat.vcountSetting;
	video->nextEvent = state->video.nextEvent;
	video->eventDiff = state->video.eventDiff;
	video->lastHblank = state->video.lastHblank;
	video->nextHblank = state->video.nextHblank;
	video->nextHblankIRQ = state->video.nextHblankIRQ;
	video->nextVblankIRQ = state->video.nextVblankIRQ;
	video->nextVcounterIRQ = state->video.nextVcounterIRQ;
	video->vcount = state->io[REG_VCOUNT >> 1];
}
