/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "software.h"

#include "util/memory.h"

static void GBVideoSoftwareRendererInit(struct GBVideoRenderer* renderer);
static void GBVideoSoftwareRendererDeinit(struct GBVideoRenderer* renderer);
static void GBVideoSoftwareRendererReset(struct GBVideoRenderer* renderer);
static void GBVideoSoftwareRendererWriteVRAM(struct GBVideoRenderer* renderer, uint16_t address);
static uint8_t GBVideoSoftwareRendererWriteVideoRegister(struct GBVideoRenderer* renderer, uint16_t address, uint8_t value);
static void GBVideoSoftwareRendererDrawScanline(struct GBVideoRenderer* renderer, int y);
static void GBVideoSoftwareRendererFinishFrame(struct GBVideoRenderer* renderer);
static void GBVideoSoftwareRendererGetPixels(struct GBVideoRenderer* renderer, unsigned* stride, const void** pixels);
static void GBVideoSoftwareRendererPutPixels(struct GBVideoRenderer* renderer, unsigned stride, void* pixels);

#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
static const color_t GB_PALETTE[4] = { 0xFFFF, 0x39C7, 0x18C3, 0x0000};
#else
static const color_t GB_PALETTE[4] = { 0x7FFF, 0x1DE7, 0x0C63, 0x0000};
#endif
#else
static const color_t GB_PALETTE[4] = { 0xFFFFFF, 0x808080, 0x404040, 0x000000};
#endif

void GBVideoSoftwareRendererCreate(struct GBVideoSoftwareRenderer* renderer) {
	renderer->d.init = GBVideoSoftwareRendererInit;
	renderer->d.reset = GBVideoSoftwareRendererReset;
	renderer->d.deinit = GBVideoSoftwareRendererDeinit;
	renderer->d.writeVideoRegister = GBVideoSoftwareRendererWriteVideoRegister;
	renderer->d.writeVRAM = GBVideoSoftwareRendererWriteVRAM;
	renderer->d.drawScanline = GBVideoSoftwareRendererDrawScanline;
	renderer->d.finishFrame = GBVideoSoftwareRendererFinishFrame;
	renderer->d.getPixels = 0;
	renderer->d.putPixels = 0;

	renderer->temporaryBuffer = 0;
}

static void GBVideoSoftwareRendererInit(struct GBVideoRenderer* renderer) {
	GBVideoSoftwareRendererReset(renderer);

	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;

	int y;
	for (y = 0; y < GB_VIDEO_VERTICAL_PIXELS; ++y) {
		color_t* row = &softwareRenderer->outputBuffer[softwareRenderer->outputBufferStride * y];
		int x;
		for (x = 0; x < GB_VIDEO_HORIZONTAL_PIXELS; ++x) {
			row[x] = GB_PALETTE[0];
		}
	}
}

static void GBVideoSoftwareRendererReset(struct GBVideoRenderer* renderer) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	int i;

	// TODO
}

static void GBVideoSoftwareRendererDeinit(struct GBVideoRenderer* renderer) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	UNUSED(softwareRenderer);
}

static void GBVideoSoftwareRendererWriteVRAM(struct GBVideoRenderer* renderer, uint16_t address) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	// TODO
}
static uint8_t GBVideoSoftwareRendererWriteVideoRegister(struct GBVideoRenderer* renderer, uint16_t address, uint8_t value) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	// TODO
	return value;
}

static void GBVideoSoftwareRendererDrawScanline(struct GBVideoRenderer* renderer, int y) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;

	color_t* row = &softwareRenderer->outputBuffer[softwareRenderer->outputBufferStride * y];

	// TODO
	size_t x;
#ifdef COLOR_16_BIT
#if defined(__ARM_NEON) && !defined(__APPLE__)
	_to16Bit(row, softwareRenderer->row, GB_VIDEO_HORIZONTAL_PIXELS);
#else
	for (x = 0; x < GB_VIDEO_HORIZONTAL_PIXELS; ++x) {
		row[x] = softwareRenderer->row[x];
	}
#endif
#else
	memcpy(row, softwareRenderer->row, GB_VIDEO_HORIZONTAL_PIXELS * sizeof(*row));
#endif
}

static void GBVideoSoftwareRendererFinishFrame(struct GBVideoRenderer* renderer) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;

	if (softwareRenderer->temporaryBuffer) {
		mappedMemoryFree(softwareRenderer->temporaryBuffer, GB_VIDEO_HORIZONTAL_PIXELS * GB_VIDEO_VERTICAL_PIXELS * 4);
		softwareRenderer->temporaryBuffer = 0;
	}
}
