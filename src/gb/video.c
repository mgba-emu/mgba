/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "video.h"

#include "gb/gb.h"
#include "gb/io.h"

#include "util/memory.h"

static void GBVideoDummyRendererInit(struct GBVideoRenderer* renderer);
static void GBVideoDummyRendererReset(struct GBVideoRenderer* renderer);
static void GBVideoDummyRendererDeinit(struct GBVideoRenderer* renderer);
static uint8_t GBVideoDummyRendererWriteVideoRegister(struct GBVideoRenderer* renderer, uint16_t address, uint8_t value);
static void GBVideoDummyRendererWriteVRAM(struct GBVideoRenderer* renderer, uint16_t address);
static void GBVideoDummyRendererDrawScanline(struct GBVideoRenderer* renderer, int y);
static void GBVideoDummyRendererFinishFrame(struct GBVideoRenderer* renderer);
static void GBVideoDummyRendererGetPixels(struct GBVideoRenderer* renderer, unsigned* stride, const void** pixels);

static struct GBVideoRenderer dummyRenderer = {
	.init = GBVideoDummyRendererInit,
	.reset = GBVideoDummyRendererReset,
	.deinit = GBVideoDummyRendererDeinit,
	.writeVideoRegister = GBVideoDummyRendererWriteVideoRegister,
	.writeVRAM = GBVideoDummyRendererWriteVRAM,
	.drawScanline = GBVideoDummyRendererDrawScanline,
	.finishFrame = GBVideoDummyRendererFinishFrame,
	.getPixels = GBVideoDummyRendererGetPixels
};

void GBVideoInit(struct GBVideo* video) {
	video->renderer = &dummyRenderer;
	video->vram = 0;
	video->frameskip = 0;
}

void GBVideoReset(struct GBVideo* video) {
	video->ly = 0;
	video->mode = 1;

	video->nextEvent = INT_MAX;
	video->eventDiff = 0;

	video->nextMode = INT_MAX;

	video->frameCounter = 0;
	video->frameskipCounter = 0;

	if (video->vram) {
		mappedMemoryFree(video->vram, GB_SIZE_VRAM);
	}
	video->vram = anonymousMemoryMap(GB_SIZE_VRAM);
	video->renderer->vram = video->vram;

	video->renderer->deinit(video->renderer);
	video->renderer->init(video->renderer);
}

void GBVideoDeinit(struct GBVideo* video) {
	GBVideoAssociateRenderer(video, &dummyRenderer);
	mappedMemoryFree(video->vram, GB_SIZE_VRAM);
}

void GBVideoAssociateRenderer(struct GBVideo* video, struct GBVideoRenderer* renderer) {
	// TODO
}

int32_t GBVideoProcessEvents(struct GBVideo* video, int32_t cycles) {
	video->eventDiff += cycles;
	if (video->nextEvent != INT_MAX) {
		video->nextEvent -= cycles;
	}
	if (video->nextEvent <= 0) {
		if (video->nextEvent != INT_MAX) {
			video->nextMode -= video->eventDiff;
		}
		if (video->nextMode <= 0) {
			video->mode = (video->mode + 1) & 3;
			switch (video->mode) {
			case 0:
				video->nextMode = GB_VIDEO_MODE_0_LENGTH;
				break;
			case 1:
				video->nextMode = GB_VIDEO_MODE_1_LENGTH;
				break;
			case 2:
				video->nextMode = GB_VIDEO_MODE_2_LENGTH;
				++video->ly;
				if (video->ly >= GB_VIDEO_VERTICAL_TOTAL_PIXELS) {
					video->ly = 0;
					++video->frameCounter;
				}
				video->p->memory.io[REG_LY] = video->ly;
				break;
			case 3:
				video->nextMode = GB_VIDEO_MODE_3_LENGTH;
				break;
			}
		}

		video->nextEvent = video->nextMode;
		video->eventDiff = 0;
	}
	return video->nextEvent;
}

void GBVideoWriteLCDC(struct GBVideo* video, GBRegisterLCDC value) {
	if (!GBRegisterLCDCIsEnable(video->p->memory.io[REG_LCDC]) && GBRegisterLCDCIsEnable(value)) {
		// TODO: Does enabling the LCD start in vblank?
		video->mode = 2;
		video->nextMode = GB_VIDEO_MODE_2_LENGTH;
		video->nextEvent = video->nextMode;
		video->eventDiff = 0;
		if (video->nextEvent < video->p->cpu->nextEvent) {
			video->p->cpu->nextEvent = video->nextEvent;
		}
		return;
	}
}

static void GBVideoDummyRendererInit(struct GBVideoRenderer* renderer) {
	UNUSED(renderer);
	// Nothing to do
}

static void GBVideoDummyRendererReset(struct GBVideoRenderer* renderer) {
	UNUSED(renderer);
	// Nothing to do
}

static void GBVideoDummyRendererDeinit(struct GBVideoRenderer* renderer) {
	UNUSED(renderer);
	// Nothing to do
}

static uint8_t GBVideoDummyRendererWriteVideoRegister(struct GBVideoRenderer* renderer, uint16_t address, uint8_t value) {
	UNUSED(renderer);
	UNUSED(address);
	return value;
}

static void GBVideoDummyRendererWriteVRAM(struct GBVideoRenderer* renderer, uint16_t address) {
	UNUSED(renderer);
	UNUSED(address);
	// Nothing to do
}

static void GBVideoDummyRendererDrawScanline(struct GBVideoRenderer* renderer, int y) {
	UNUSED(renderer);
	UNUSED(y);
	// Nothing to do
}

static void GBVideoDummyRendererFinishFrame(struct GBVideoRenderer* renderer) {
	UNUSED(renderer);
	// Nothing to do
}

static void GBVideoDummyRendererGetPixels(struct GBVideoRenderer* renderer, unsigned* stride, const void** pixels) {
	UNUSED(renderer);
	UNUSED(stride);
	UNUSED(pixels);
	// Nothing to do
}
