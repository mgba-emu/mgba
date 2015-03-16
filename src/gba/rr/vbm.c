/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "vbm.h"

#include "gba/gba.h"
#include "gba/serialize.h"
#include "util/vfs.h"

static const char VBM_MAGIC[] = "VBM\x1A";

static void GBAVBMContextDestroy(struct GBARRContext*);

static bool GBAVBMStartPlaying(struct GBARRContext*, bool autorecord);
static void GBAVBMStopPlaying(struct GBARRContext*);
static bool GBAVBMStartRecording(struct GBARRContext*);
static void GBAVBMStopRecording(struct GBARRContext*);

static bool GBAVBMIsPlaying(const struct GBARRContext*);
static bool GBAVBMIsRecording(const struct GBARRContext*);

static void GBAVBMNextFrame(struct GBARRContext*);
static uint16_t GBAVBMQueryInput(struct GBARRContext*);

static void GBAVBMStateSaved(struct GBARRContext* rr, struct GBASerializedState* state);
static void GBAVBMStateLoaded(struct GBARRContext* rr, const struct GBASerializedState* state);

static struct VFile* GBAVBMOpenSavedata(struct GBARRContext*, int flags);
static struct VFile* GBAVBMOpenSavestate(struct GBARRContext*, int flags);

void GBAVBMContextCreate(struct GBAVBMContext* vbm) {
	memset(vbm, 0, sizeof(*vbm));

	vbm->d.destroy = GBAVBMContextDestroy;

	vbm->d.startPlaying = GBAVBMStartPlaying;
	vbm->d.stopPlaying = GBAVBMStopPlaying;
	vbm->d.startRecording = GBAVBMStartRecording;
	vbm->d.stopRecording = GBAVBMStopRecording;

	vbm->d.isPlaying = GBAVBMIsPlaying;
	vbm->d.isRecording = GBAVBMIsRecording;

	vbm->d.nextFrame = GBAVBMNextFrame;
	vbm->d.logInput = 0;
	vbm->d.queryInput = GBAVBMQueryInput;

	vbm->d.stateSaved = GBAVBMStateSaved;
	vbm->d.stateLoaded = GBAVBMStateLoaded;

	vbm->d.openSavedata = GBAVBMOpenSavedata;
	vbm->d.openSavestate = GBAVBMOpenSavestate;
}

bool GBAVBMStartPlaying(struct GBARRContext* rr, bool autorecord) {
	if (rr->isRecording(rr) || rr->isPlaying(rr) || autorecord) {
		return false;
	}

	struct GBAVBMContext* vbm = (struct GBAVBMContext*) rr;
	vbm->isPlaying = true;
	vbm->vbmFile->seek(vbm->vbmFile, vbm->inputOffset, SEEK_SET);
	return true;
}

void GBAVBMStopPlaying(struct GBARRContext* rr) {
	if (!rr->isPlaying(rr)) {
		return;
	}

	struct GBAVBMContext* vbm = (struct GBAVBMContext*) rr;
	vbm->isPlaying = false;
}

bool GBAVBMStartRecording(struct GBARRContext* rr) {
	UNUSED(rr);
	return false;
}

void GBAVBMStopRecording(struct GBARRContext* rr) {
	UNUSED(rr);
}

bool GBAVBMIsPlaying(const struct GBARRContext* rr) {
	struct GBAVBMContext* vbm = (struct GBAVBMContext*) rr;
	return vbm->isPlaying;
}

bool GBAVBMIsRecording(const struct GBARRContext* rr) {
	UNUSED(rr);
	return false;
}

void GBAVBMNextFrame(struct GBARRContext* rr) {
	UNUSED(rr);
}

uint16_t GBAVBMQueryInput(struct GBARRContext* rr) {
	if (!rr->isPlaying(rr)) {
		return 0;
	}

	struct GBAVBMContext* vbm = (struct GBAVBMContext*) rr;
	uint16_t input;
	vbm->vbmFile->read(vbm->vbmFile, &input, sizeof(input));
	return input & 0x3FF;
}

void GBAVBMStateSaved(struct GBARRContext* rr, struct GBASerializedState* state) {
	UNUSED(rr);
	UNUSED(state);
}

void GBAVBMStateLoaded(struct GBARRContext* rr, const struct GBASerializedState* state) {
	UNUSED(rr);
	UNUSED(state);
}

struct VFile* GBAVBMOpenSavedata(struct GBARRContext* rr, int flags) {
	UNUSED(rr);
	UNUSED(flags);
	return 0;
}

struct VFile* GBAVBMOpenSavestate(struct GBARRContext* rr, int flags) {
	UNUSED(rr);
	UNUSED(flags);
	return 0;
}

void GBAVBMContextDestroy(struct GBARRContext* rr) {
	struct GBAVBMContext* vbm = (struct GBAVBMContext*) rr;
	if (vbm->vbmFile) {
		vbm->vbmFile->close(vbm->vbmFile);
	}
}

bool GBAVBMSetStream(struct GBAVBMContext* vbm, struct VFile* vf) {
	vf->seek(vf, 0, SEEK_SET);
	char magic[4];
	vf->read(vf, magic, sizeof(magic));
	if (memcmp(magic, VBM_MAGIC, sizeof(magic)) != 0) {
		return false;
	}

	uint32_t id;
	vf->read(vf, &id, sizeof(id));
	if (id != 1) {
		return false;
	}

	vf->seek(vf, 4, SEEK_CUR);
	vf->read(vf, &vbm->d.frames, sizeof(vbm->d.frames));
	vf->read(vf, &vbm->d.rrCount, sizeof(vbm->d.rrCount));

	uint8_t flags;
	vf->read(vf, &flags, sizeof(flags));
	if (flags & 1) {
		// Incompatible savestate format
		return false;
	}
	if (flags & 2) {
		// TODO: Implement SRAM loading
		return false;
	}

	vf->seek(vf, 1, SEEK_CUR);
	vf->read(vf, &flags, sizeof(flags));
	if ((flags & 0x7) != 1) {
		// Non-GBA movie
		return false;
	}

	// TODO: parse more flags

	vf->seek(vf, 0x3C, SEEK_SET);
	vf->read(vf, &vbm->inputOffset, sizeof(vbm->inputOffset));
	vf->seek(vf, vbm->inputOffset, SEEK_SET);
	vbm->vbmFile = vf;
	return true;
}
