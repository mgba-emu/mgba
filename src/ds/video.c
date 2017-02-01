/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/video.h>

#include <mgba/core/sync.h>
#include <mgba/internal/ds/ds.h>
#include <mgba/internal/gba/video.h>

#include <mgba-util/memory.h>

mLOG_DEFINE_CATEGORY(DS_VIDEO, "DS Video");

static void _startHblank7(struct mTiming*, void* context, uint32_t cyclesLate);
static void _startHdraw7(struct mTiming*, void* context, uint32_t cyclesLate);
static void _startHblank9(struct mTiming*, void* context, uint32_t cyclesLate);
static void _startHdraw9(struct mTiming*, void* context, uint32_t cyclesLate);

void DSVideoInit(struct DSVideo* video) {
	video->vram = NULL;
	video->frameskip = 0;
	video->event7.name = "DS7 Video";
	video->event7.callback = NULL;
	video->event7.context = video;
	video->event7.priority = 8;
	video->event9.name = "DS9 Video";
	video->event9.callback = NULL;
	video->event9.context = video;
	video->event9.priority = 8;
}

void DSVideoReset(struct DSVideo* video) {
	video->vcount = 0;
	video->p->ds7.memory.io[DS_REG_VCOUNT >> 1] = video->vcount;
	video->p->ds9.memory.io[DS_REG_VCOUNT >> 1] = video->vcount;

	video->event7.callback = _startHblank7;
	video->event9.callback = _startHblank9;
	mTimingSchedule(&video->p->ds7.timing, &video->event7, DS_VIDEO_HORIZONTAL_LENGTH - DS7_VIDEO_HBLANK_LENGTH);
	mTimingSchedule(&video->p->ds9.timing, &video->event9, DS_VIDEO_HORIZONTAL_LENGTH - DS9_VIDEO_HBLANK_LENGTH);

	video->frameCounter = 0;
	video->frameskipCounter = 0;

	if (video->vram) {
		mappedMemoryFree(video->vram, DS_SIZE_VRAM);
	}
	video->vram = anonymousMemoryMap(DS_SIZE_VRAM);
}

void DSVideoDeinit(struct DSVideo* video) {
	mappedMemoryFree(video->vram, DS_SIZE_VRAM);
}

void _startHdraw7(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSVideo* video = context;
	GBARegisterDISPSTAT dispstat = video->p->ds7.memory.io[DS_REG_DISPSTAT >> 1];
	dispstat = GBARegisterDISPSTATClearInHblank(dispstat);
	video->event7.callback = _startHblank7;
	mTimingSchedule(timing, &video->event7, DS_VIDEO_HORIZONTAL_LENGTH - DS7_VIDEO_HBLANK_LENGTH - cyclesLate);

	++video->vcount;
	if (video->vcount == DS_VIDEO_VERTICAL_TOTAL_PIXELS) {
		video->vcount = 0;
	}
	video->p->ds7.memory.io[DS_REG_VCOUNT >> 1] = video->vcount;

	if (video->vcount == GBARegisterDISPSTATGetVcountSetting(dispstat)) {
		dispstat = GBARegisterDISPSTATFillVcounter(dispstat);
		if (GBARegisterDISPSTATIsVcounterIRQ(dispstat)) {
			DSRaiseIRQ(video->p->ds7.cpu, video->p->ds7.memory.io, DS_IRQ_VCOUNTER);
		}
	} else {
		dispstat = GBARegisterDISPSTATClearVcounter(dispstat);
	}
	video->p->ds7.memory.io[DS_REG_DISPSTAT >> 1] = dispstat;

	switch (video->vcount) {
	case DS_VIDEO_VERTICAL_PIXELS:
		video->p->ds7.memory.io[DS_REG_DISPSTAT >> 1] = GBARegisterDISPSTATFillInVblank(dispstat);
		if (GBARegisterDISPSTATIsVblankIRQ(dispstat)) {
			DSRaiseIRQ(video->p->ds7.cpu, video->p->ds7.memory.io, DS_IRQ_VBLANK);
		}
		break;
	case DS_VIDEO_VERTICAL_TOTAL_PIXELS - 1:
		video->p->ds7.memory.io[DS_REG_DISPSTAT >> 1] = GBARegisterDISPSTATClearInVblank(dispstat);
		break;
	}
}

void _startHblank7(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSVideo* video = context;
	GBARegisterDISPSTAT dispstat = video->p->ds7.memory.io[DS_REG_DISPSTAT >> 1];
	dispstat = GBARegisterDISPSTATFillInHblank(dispstat);
	video->event7.callback = _startHdraw7;
	mTimingSchedule(timing, &video->event7, DS7_VIDEO_HBLANK_LENGTH - cyclesLate);

	// Begin Hblank
	dispstat = GBARegisterDISPSTATFillInHblank(dispstat);

	if (GBARegisterDISPSTATIsHblankIRQ(dispstat)) {
		DSRaiseIRQ(video->p->ds7.cpu, video->p->ds7.memory.io, DS_IRQ_HBLANK);
	}
	video->p->ds7.memory.io[DS_REG_DISPSTAT >> 1] = dispstat;
}

void _startHdraw9(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSVideo* video = context;
	GBARegisterDISPSTAT dispstat = video->p->ds9.memory.io[DS_REG_DISPSTAT >> 1];
	dispstat = GBARegisterDISPSTATClearInHblank(dispstat);
	video->event9.callback = _startHblank9;
	mTimingSchedule(timing, &video->event9, DS_VIDEO_HORIZONTAL_LENGTH - DS9_VIDEO_HBLANK_LENGTH - cyclesLate);

	++video->vcount;
	if (video->vcount == DS_VIDEO_VERTICAL_TOTAL_PIXELS) {
		video->vcount = 0;
	}
	video->p->ds9.memory.io[DS_REG_VCOUNT >> 1] = video->vcount;

	if (video->vcount == GBARegisterDISPSTATGetVcountSetting(dispstat)) {
		dispstat = GBARegisterDISPSTATFillVcounter(dispstat);
		if (GBARegisterDISPSTATIsVcounterIRQ(dispstat)) {
			DSRaiseIRQ(video->p->ds9.cpu, video->p->ds9.memory.io, DS_IRQ_VCOUNTER);
		}
	} else {
		dispstat = GBARegisterDISPSTATClearVcounter(dispstat);
	}
	video->p->ds9.memory.io[DS_REG_DISPSTAT >> 1] = dispstat;

	// Note: state may be recorded during callbacks, so ensure it is consistent!
	switch (video->vcount) {
	case 0:
		DSFrameStarted(video->p);
		break;
	case DS_VIDEO_VERTICAL_PIXELS:
		video->p->ds9.memory.io[DS_REG_DISPSTAT >> 1] = GBARegisterDISPSTATFillInVblank(dispstat);
		if (GBARegisterDISPSTATIsVblankIRQ(dispstat)) {
			DSRaiseIRQ(video->p->ds9.cpu, video->p->ds9.memory.io, DS_IRQ_VBLANK);
		}
		DSFrameEnded(video->p);
		--video->frameskipCounter;
		if (video->frameskipCounter < 0) {
			mCoreSyncPostFrame(video->p->sync);
			video->frameskipCounter = video->frameskip;
		}
		++video->frameCounter;
		break;
	case DS_VIDEO_VERTICAL_TOTAL_PIXELS - 1:
		video->p->ds9.memory.io[DS_REG_DISPSTAT >> 1] = GBARegisterDISPSTATClearInVblank(dispstat);
		break;
	}
}

void _startHblank9(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSVideo* video = context;
	GBARegisterDISPSTAT dispstat = video->p->ds9.memory.io[DS_REG_DISPSTAT >> 1];
	dispstat = GBARegisterDISPSTATFillInHblank(dispstat);
	video->event9.callback = _startHdraw9;
	mTimingSchedule(timing, &video->event9, DS9_VIDEO_HBLANK_LENGTH - cyclesLate);

	// Begin Hblank
	dispstat = GBARegisterDISPSTATFillInHblank(dispstat);

	if (GBARegisterDISPSTATIsHblankIRQ(dispstat)) {
		DSRaiseIRQ(video->p->ds9.cpu, video->p->ds9.memory.io, DS_IRQ_HBLANK);
	}
	video->p->ds9.memory.io[DS_REG_DISPSTAT >> 1] = dispstat;
}

void DSVideoWriteDISPSTAT(struct DSCommon* dscore, uint16_t value) {
	dscore->memory.io[DS_REG_DISPSTAT >> 1] &= 0x7;
	dscore->memory.io[DS_REG_DISPSTAT >> 1] |= value;
	// TODO: Does a VCounter IRQ trigger on write?
}
