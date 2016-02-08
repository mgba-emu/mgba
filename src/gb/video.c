/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "video.h"

#include "core/sync.h"
#include "core/thread.h"
#include "gb/gb.h"
#include "gb/io.h"

#include "util/memory.h"

static void GBVideoDummyRendererInit(struct GBVideoRenderer* renderer);
static void GBVideoDummyRendererReset(struct GBVideoRenderer* renderer);
static void GBVideoDummyRendererDeinit(struct GBVideoRenderer* renderer);
static uint8_t GBVideoDummyRendererWriteVideoRegister(struct GBVideoRenderer* renderer, uint16_t address, uint8_t value);
static void GBVideoDummyRendererDrawDot(struct GBVideoRenderer* renderer, int x, int y, struct GBObj** obj, size_t oamMax);
static void GBVideoDummyRendererFinishFrame(struct GBVideoRenderer* renderer);
static void GBVideoDummyRendererGetPixels(struct GBVideoRenderer* renderer, unsigned* stride, const void** pixels);

static void _cleanOAM(struct GBVideo* video, int y);

static struct GBVideoRenderer dummyRenderer = {
	.init = GBVideoDummyRendererInit,
	.reset = GBVideoDummyRendererReset,
	.deinit = GBVideoDummyRendererDeinit,
	.writeVideoRegister = GBVideoDummyRendererWriteVideoRegister,
	.drawDot = GBVideoDummyRendererDrawDot,
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
	video->stat = 1;

	video->nextEvent = INT_MAX;
	video->eventDiff = 0;

	video->nextMode = INT_MAX;
	video->nextDot = INT_MAX;

	video->frameCounter = 0;
	video->frameskipCounter = 0;

	if (video->vram) {
		mappedMemoryFree(video->vram, GB_SIZE_VRAM);
	}
	video->vram = anonymousMemoryMap(GB_SIZE_VRAM);
	video->renderer->vram = video->vram;
	memset(&video->oam, 0, sizeof(video->oam));
	video->renderer->oam = &video->oam;

	video->renderer->deinit(video->renderer);
	video->renderer->init(video->renderer);
}

void GBVideoDeinit(struct GBVideo* video) {
	GBVideoAssociateRenderer(video, &dummyRenderer);
	mappedMemoryFree(video->vram, GB_SIZE_VRAM);
}

void GBVideoAssociateRenderer(struct GBVideo* video, struct GBVideoRenderer* renderer) {
	video->renderer->deinit(video->renderer);
	video->renderer = renderer;
	renderer->vram = video->vram;
	video->renderer->init(video->renderer);
}

int32_t GBVideoProcessEvents(struct GBVideo* video, int32_t cycles) {
	video->eventDiff += cycles;
	if (video->nextEvent != INT_MAX) {
		video->nextEvent -= cycles;
	}
	if (video->nextEvent <= 0) {
		if (video->nextEvent != INT_MAX) {
			video->nextMode -= video->eventDiff;
			video->nextDot -= video->eventDiff;
		}
		video->nextEvent = INT_MAX;
		if (video->nextDot <= 0) {
			video->renderer->drawDot(video->renderer, video->x, video->ly, video->objThisLine, video->objMax);
			++video->x;
			if (video->x < GB_VIDEO_HORIZONTAL_PIXELS) {
				video->nextDot = 1;
			} else {
				video->nextDot = INT_MAX;
			}
		}
		if (video->nextMode <= 0) {
			int lyc = video->p->memory.io[REG_LYC];
			switch (video->mode) {
			case 0:
				++video->ly;
				video->p->memory.io[REG_LY] = video->ly;
				video->stat = GBRegisterSTATSetLYC(video->stat, lyc == video->ly);
				if (video->ly < GB_VIDEO_VERTICAL_PIXELS) {
					video->nextMode = GB_VIDEO_MODE_2_LENGTH;
					video->mode = 2;
					if (GBRegisterSTATIsOAMIRQ(video->stat)) {
						video->p->memory.io[REG_IF] |= (1 << GB_IRQ_LCDSTAT);
					}
				} else {
					video->nextMode = GB_VIDEO_HORIZONTAL_LENGTH;
					video->mode = 1;
					++video->frameCounter;
					video->renderer->finishFrame(video->renderer);
					mCoreSyncPostFrame(video->p->sync);

					struct mCoreThread* thread = mCoreThreadGet();
					if (thread && thread->frameCallback) {
						thread->frameCallback(thread);
					}

					if (GBRegisterSTATIsVblankIRQ(video->stat) || GBRegisterSTATIsOAMIRQ(video->stat)) {
						video->p->memory.io[REG_IF] |= (1 << GB_IRQ_LCDSTAT);
					}
					video->p->memory.io[REG_IF] |= (1 << GB_IRQ_VBLANK);
				}
				if (GBRegisterSTATIsLYCIRQ(video->stat) && lyc == video->ly) {
					video->p->memory.io[REG_IF] |= (1 << GB_IRQ_LCDSTAT);
				}
				GBUpdateIRQs(video->p);
				break;
			case 1:
				// TODO: One M-cycle delay
				++video->ly;
				video->stat = GBRegisterSTATSetLYC(video->stat, lyc == video->ly);
				if (video->ly == GB_VIDEO_VERTICAL_TOTAL_PIXELS) {
					video->ly = 0;
					video->nextMode = GB_VIDEO_MODE_2_LENGTH;
					video->mode = 2;
					if (GBRegisterSTATIsOAMIRQ(video->stat)) {
						video->p->memory.io[REG_IF] |= (1 << GB_IRQ_LCDSTAT);
					}
				} else {
					video->nextMode = GB_VIDEO_HORIZONTAL_LENGTH;
				}
				if (video->ly == GB_VIDEO_VERTICAL_TOTAL_PIXELS - 1) {
					video->p->memory.io[REG_LY] = 0;
				} else {
					video->p->memory.io[REG_LY] = video->ly;
				}
				if (GBRegisterSTATIsLYCIRQ(video->stat) && lyc == video->ly) {
					video->p->memory.io[REG_IF] |= (1 << GB_IRQ_LCDSTAT);
				}
				GBUpdateIRQs(video->p);
				break;
			case 2:
				_cleanOAM(video, video->ly);
				video->nextDot = 1;
				video->nextEvent = video->nextDot;
				video->x = 0;
				video->nextMode = GB_VIDEO_MODE_3_LENGTH_BASE + video->objMax * 8;
				video->mode = 3;
				break;
			case 3:
				video->nextMode = GB_VIDEO_MODE_0_LENGTH_BASE - video->objMax * 8;
				video->mode = 0;
				if (GBRegisterSTATIsHblankIRQ(video->stat)) {
					video->p->memory.io[REG_IF] |= (1 << GB_IRQ_LCDSTAT);
					GBUpdateIRQs(video->p);
				}
				break;
			}
			video->stat = GBRegisterSTATSetMode(video->stat, video->mode);
			video->p->memory.io[REG_STAT] = video->stat;
		}
		if (video->nextDot < video->nextEvent) {
			video->nextEvent = video->nextDot;
		}
		if (video->nextMode < video->nextEvent) {
			video->nextEvent = video->nextMode;
		}
		video->eventDiff = 0;
	}
	return video->nextEvent;
}

static void _cleanOAM(struct GBVideo* video, int y) {
	// TODO: GBC differences
	// TODO: Optimize
	video->objMax = 0;
	int spriteHeight = 8;
	if (GBRegisterLCDCIsObjSize(video->p->memory.io[REG_LCDC])) {
		spriteHeight = 16;
	}
	int o = 0;
	int i;
	for (i = 0; i < 40; ++i) {
		uint8_t oy = video->oam.obj[i].y;
		if (y < oy - 16 || y >= oy - 16 + spriteHeight) {
			continue;
		}
		// TODO: Sort
		video->objThisLine[o] = &video->oam.obj[i];
		++o;
		if (o == 10) {
			break;
		}
	}
	video->objMax = o;
}

void GBVideoWriteLCDC(struct GBVideo* video, GBRegisterLCDC value) {
	if (!GBRegisterLCDCIsEnable(video->p->memory.io[REG_LCDC]) && GBRegisterLCDCIsEnable(value)) {
		video->mode = 2;
		video->nextMode = GB_VIDEO_MODE_2_LENGTH - 5; // TODO: Why is this fudge factor needed? Might be related to T-cycles for load/store differing
		video->nextEvent = video->nextMode;
		video->eventDiff = -video->p->cpu->cycles;
		// TODO: Does this read as 0 for 4 T-cycles?
		video->stat = GBRegisterSTATSetMode(video->stat, 2);
		video->p->memory.io[REG_STAT] = video->stat;
		video->ly = 0;
		video->p->memory.io[REG_LY] = 0;

		if (video->p->cpu->cycles + video->nextEvent < video->p->cpu->nextEvent) {
			video->p->cpu->nextEvent = video->p->cpu->cycles + video->nextEvent;
		}
		return;
	}
	if (GBRegisterLCDCIsEnable(video->p->memory.io[REG_LCDC]) && !GBRegisterLCDCIsEnable(value)) {
		video->mode = 0;
		video->nextMode = INT_MAX;
		video->nextEvent = INT_MAX;
		video->stat = GBRegisterSTATSetMode(video->stat, video->mode);
		video->p->memory.io[REG_STAT] = video->stat;
		video->ly = 0;
		video->p->memory.io[REG_LY] = 0;
	}
}

void GBVideoWriteSTAT(struct GBVideo* video, GBRegisterSTAT value) {
	video->stat = (video->stat & 0x7) | (value & 0x78);
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

static void GBVideoDummyRendererDrawDot(struct GBVideoRenderer* renderer, int x, int y, struct GBObj** obj, size_t oamMax) {
	UNUSED(renderer);
	UNUSED(x);
	UNUSED(y);
	UNUSED(obj);
	UNUSED(oamMax);
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
