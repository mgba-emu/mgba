/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba-video.h"

#include "gba.h"
#include "gba-io.h"
#include "gba-rr.h"
#include "gba-serialize.h"
#include "gba-thread.h"

#include "util/memory.h"

static void GBAVideoDummyRendererInit(struct GBAVideoRenderer* renderer);
static void GBAVideoDummyRendererReset(struct GBAVideoRenderer* renderer);
static void GBAVideoDummyRendererDeinit(struct GBAVideoRenderer* renderer);
static uint16_t GBAVideoDummyRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static void GBAVideoDummyRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static void GBAVideoDummyRendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam);
static void GBAVideoDummyRendererDrawScanline(struct GBAVideoRenderer* renderer, int y);
static void GBAVideoDummyRendererFinishFrame(struct GBAVideoRenderer* renderer);
static void GBAVideoDummyRendererGetPixels(struct GBAVideoRenderer* renderer, unsigned* stride, void** pixels);

static struct GBAVideoRenderer dummyRenderer = {
	.init = GBAVideoDummyRendererInit,
	.reset = GBAVideoDummyRendererReset,
	.deinit = GBAVideoDummyRendererDeinit,
	.writeVideoRegister = GBAVideoDummyRendererWriteVideoRegister,
	.writePalette = GBAVideoDummyRendererWritePalette,
	.writeOAM = GBAVideoDummyRendererWriteOAM,
	.drawScanline = GBAVideoDummyRendererDrawScanline,
	.finishFrame = GBAVideoDummyRendererFinishFrame,
	.getPixels = GBAVideoDummyRendererGetPixels
};

void GBAVideoInit(struct GBAVideo* video) {
	video->renderer = &dummyRenderer;
	video->vram = 0;
}

void GBAVideoReset(struct GBAVideo* video) {
	video->dispstat = 0;
	video->vcount = 0;

	video->lastHblank = 0;
	video->nextHblank = VIDEO_HDRAW_LENGTH;
	video->nextEvent = video->nextHblank;
	video->eventDiff = video->nextEvent;

	video->nextHblankIRQ = 0;
	video->nextVblankIRQ = 0;
	video->nextVcounterIRQ = 0;

	if (video->vram) {
		mappedMemoryFree(video->vram, SIZE_VRAM);
	}
	video->vram = anonymousMemoryMap(SIZE_VRAM);
	video->renderer->vram = video->vram;

	int i;
	for (i = 0; i < 128; ++i) {
		video->oam.raw[i * 4] = 0x0200;
		video->oam.raw[i * 4 + 1] = 0x0000;
		video->oam.raw[i * 4 + 2] = 0x0000;
		video->oam.raw[i * 4 + 3] = 0x0000;
	}

	video->renderer->deinit(video->renderer);
	video->renderer->init(video->renderer);
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

		if (GBARegisterDISPSTATIsInHblank(video->dispstat)) {
			// End Hblank
			video->dispstat = GBARegisterDISPSTATClearInHblank(video->dispstat);
			video->nextEvent = video->nextHblank;

			++video->vcount;
			video->p->memory.io[REG_VCOUNT >> 1] = video->vcount;

			switch (video->vcount) {
			case VIDEO_VERTICAL_PIXELS:
				video->dispstat = GBARegisterDISPSTATFillInVblank(video->dispstat);
				if (GBASyncDrawingFrame(video->p->sync)) {
					video->renderer->finishFrame(video->renderer);
				}
				video->nextVblankIRQ = video->nextEvent + VIDEO_TOTAL_LENGTH;
				GBAMemoryRunVblankDMAs(video->p, lastEvent);
				if (GBARegisterDISPSTATIsVblankIRQ(video->dispstat)) {
					GBARaiseIRQ(video->p, IRQ_VBLANK);
				}
				GBASyncPostFrame(video->p->sync);
				break;
			case VIDEO_VERTICAL_TOTAL_PIXELS - 1:
				if (video->p->rr) {
					GBARRNextFrame(video->p->rr);
				}
				video->dispstat = GBARegisterDISPSTATClearInVblank(video->dispstat);
				break;
			case VIDEO_VERTICAL_TOTAL_PIXELS:
				video->vcount = 0;
				video->p->memory.io[REG_VCOUNT >> 1] = 0;
				break;
			}

			if (video->vcount == GBARegisterDISPSTATGetVcountSetting(video->dispstat)) {
				video->dispstat = GBARegisterDISPSTATFillVcounter(video->dispstat);
				if (GBARegisterDISPSTATIsVcounterIRQ(video->dispstat)) {
					GBARaiseIRQ(video->p, IRQ_VCOUNTER);
					video->nextVcounterIRQ += VIDEO_TOTAL_LENGTH;
				}
			} else {
				video->dispstat = GBARegisterDISPSTATClearVcounter(video->dispstat);
			}
		} else {
			// Begin Hblank
			video->dispstat = GBARegisterDISPSTATFillInHblank(video->dispstat);
			video->lastHblank = video->nextHblank;
			video->nextEvent = video->lastHblank + VIDEO_HBLANK_LENGTH;
			video->nextHblank = video->nextEvent + VIDEO_HDRAW_LENGTH;
			video->nextHblankIRQ = video->nextHblank;

			if (video->vcount < VIDEO_VERTICAL_PIXELS && GBASyncDrawingFrame(video->p->sync)) {
				video->renderer->drawScanline(video->renderer, video->vcount);
			}

			if (video->vcount < VIDEO_VERTICAL_PIXELS) {
				GBAMemoryRunHblankDMAs(video->p, lastEvent);
			}
			if (GBARegisterDISPSTATIsHblankIRQ(video->dispstat)) {
				GBARaiseIRQ(video->p, IRQ_HBLANK);
			}
		}

		video->eventDiff = 0;
	}
	video->p->memory.io[REG_DISPSTAT >> 1] &= 0xFFF8;
	video->p->memory.io[REG_DISPSTAT >> 1] |= video->dispstat & 0x7;
	return video->nextEvent;
}

void GBAVideoWriteDISPSTAT(struct GBAVideo* video, uint16_t value) {
	video->dispstat &= 0x7;
	video->dispstat |= value & 0xFFF8;

	if (GBARegisterDISPSTATIsVcounterIRQ(video->dispstat)) {
		// FIXME: this can be too late if we're in the middle of an Hblank
		video->nextVcounterIRQ = video->nextHblank + VIDEO_HBLANK_LENGTH + (GBARegisterDISPSTATGetVcountSetting(video->dispstat) - video->vcount) * VIDEO_HORIZONTAL_LENGTH;
		if (video->nextVcounterIRQ < video->nextEvent) {
			video->nextVcounterIRQ += VIDEO_TOTAL_LENGTH;
		}
	}
}

static void GBAVideoDummyRendererInit(struct GBAVideoRenderer* renderer) {
	UNUSED(renderer);
	// Nothing to do
}

static void GBAVideoDummyRendererReset(struct GBAVideoRenderer* renderer) {
	UNUSED(renderer);
	// Nothing to do
}

static void GBAVideoDummyRendererDeinit(struct GBAVideoRenderer* renderer) {
	UNUSED(renderer);
	// Nothing to do
}

static uint16_t GBAVideoDummyRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	UNUSED(renderer);
	UNUSED(address);
	return value;
}

static void GBAVideoDummyRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	UNUSED(renderer);
	UNUSED(address);
	UNUSED(value);
	// Nothing to do
}

static void GBAVideoDummyRendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam) {
	UNUSED(renderer);
	UNUSED(oam);
	// Nothing to do
}

static void GBAVideoDummyRendererDrawScanline(struct GBAVideoRenderer* renderer, int y) {
	UNUSED(renderer);
	UNUSED(y);
	// Nothing to do
}

static void GBAVideoDummyRendererFinishFrame(struct GBAVideoRenderer* renderer) {
	UNUSED(renderer);
	// Nothing to do
}

static void GBAVideoDummyRendererGetPixels(struct GBAVideoRenderer* renderer, unsigned* stride, void** pixels) {
	UNUSED(renderer);
	UNUSED(stride);
	UNUSED(pixels);
	// Nothing to do
}


void GBAVideoSerialize(struct GBAVideo* video, struct GBASerializedState* state) {
	memcpy(state->vram, video->renderer->vram, SIZE_VRAM);
	memcpy(state->oam, video->oam.raw, SIZE_OAM);
	memcpy(state->pram, video->palette, SIZE_PALETTE_RAM);
	state->io[REG_DISPSTAT >> 1] = video->dispstat;
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
	video->dispstat = state->io[REG_DISPSTAT >> 1];
	video->nextEvent = state->video.nextEvent;
	video->eventDiff = state->video.eventDiff;
	video->lastHblank = state->video.lastHblank;
	video->nextHblank = state->video.nextHblank;
	video->nextHblankIRQ = state->video.nextHblankIRQ;
	video->nextVblankIRQ = state->video.nextVblankIRQ;
	video->nextVcounterIRQ = state->video.nextVcounterIRQ;
	video->vcount = state->io[REG_VCOUNT >> 1];
}
