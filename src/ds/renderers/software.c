/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/renderers/software.h>

static void DSVideoSoftwareRendererInit(struct DSVideoRenderer* renderer);
static void DSVideoSoftwareRendererDeinit(struct DSVideoRenderer* renderer);
static void DSVideoSoftwareRendererReset(struct DSVideoRenderer* renderer);
static uint16_t DSVideoSoftwareRendererWriteVideoRegister(struct DSVideoRenderer* renderer, uint32_t address, uint16_t value);
static void DSVideoSoftwareRendererDrawScanline(struct DSVideoRenderer* renderer, int y);
static void DSVideoSoftwareRendererFinishFrame(struct DSVideoRenderer* renderer);
static void DSVideoSoftwareRendererGetPixels(struct DSVideoRenderer* renderer, size_t* stride, const void** pixels);
static void DSVideoSoftwareRendererPutPixels(struct DSVideoRenderer* renderer, size_t stride, const void* pixels);

void DSVideoSoftwareRendererCreate(struct DSVideoSoftwareRenderer* renderer) {
	renderer->d.init = DSVideoSoftwareRendererInit;
	renderer->d.reset = DSVideoSoftwareRendererReset;
	renderer->d.deinit = DSVideoSoftwareRendererDeinit;
	renderer->d.writeVideoRegister = DSVideoSoftwareRendererWriteVideoRegister;
	renderer->d.drawScanline = DSVideoSoftwareRendererDrawScanline;
	renderer->d.finishFrame = DSVideoSoftwareRendererFinishFrame;
	renderer->d.getPixels = DSVideoSoftwareRendererGetPixels;
	renderer->d.putPixels = DSVideoSoftwareRendererPutPixels;

	renderer->engA.d.cache = NULL;
	GBAVideoSoftwareRendererCreate(&renderer->engA);
	renderer->engB.d.cache = NULL;
	GBAVideoSoftwareRendererCreate(&renderer->engB);
}

static void DSVideoSoftwareRendererInit(struct DSVideoRenderer* renderer) {
	struct DSVideoSoftwareRenderer* softwareRenderer = (struct DSVideoSoftwareRenderer*) renderer;
	softwareRenderer->engA.d.palette = &renderer->palette[0];
	softwareRenderer->engA.d.oam = &renderer->oam->oam[0];
	// TODO: VRAM
	softwareRenderer->engB.d.palette = &renderer->palette[512];
	softwareRenderer->engB.d.oam = &renderer->oam->oam[1];
	// TODO: VRAM

	DSVideoSoftwareRendererReset(renderer);
}

static void DSVideoSoftwareRendererReset(struct DSVideoRenderer* renderer) {
	struct DSVideoSoftwareRenderer* softwareRenderer = (struct DSVideoSoftwareRenderer*) renderer;
	softwareRenderer->engA.d.reset(&softwareRenderer->engA.d);
	softwareRenderer->engB.d.reset(&softwareRenderer->engB.d);
}

static void DSVideoSoftwareRendererDeinit(struct DSVideoRenderer* renderer) {
	struct DSVideoSoftwareRenderer* softwareRenderer = (struct DSVideoSoftwareRenderer*) renderer;
	softwareRenderer->engA.d.deinit(&softwareRenderer->engA.d);
	softwareRenderer->engB.d.deinit(&softwareRenderer->engB.d);
}

static uint16_t DSVideoSoftwareRendererWriteVideoRegister(struct DSVideoRenderer* renderer, uint32_t address, uint16_t value) {
	mLOG(DS_VIDEO, STUB, "Stub video register write: %04X:%04X", address, value);
	return value;
}

static void DSVideoSoftwareRendererDrawScanline(struct DSVideoRenderer* renderer, int y) {
}

static void DSVideoSoftwareRendererFinishFrame(struct DSVideoRenderer* renderer) {
}

static void DSVideoSoftwareRendererGetPixels(struct DSVideoRenderer* renderer, size_t* stride, const void** pixels) {
}

static void DSVideoSoftwareRendererPutPixels(struct DSVideoRenderer* renderer, size_t stride, const void* pixels) {
}
