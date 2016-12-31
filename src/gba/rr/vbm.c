/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/rr/vbm.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/serialize.h>
#include <mgba-util/vfs.h>

#ifdef USE_ZLIB
#include <zlib.h>
#endif

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
static bool GBAVBMQueryReset(struct GBARRContext*);

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
	vbm->d.queryReset = GBAVBMQueryReset;

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
	if (!rr->isPlaying(rr)) {
		return;
	}

	struct GBAVBMContext* vbm = (struct GBAVBMContext*) rr;
	vbm->vbmFile->seek(vbm->vbmFile, sizeof(uint16_t), SEEK_CUR);
}

uint16_t GBAVBMQueryInput(struct GBARRContext* rr) {
	if (!rr->isPlaying(rr)) {
		return 0;
	}

	struct GBAVBMContext* vbm = (struct GBAVBMContext*) rr;
	uint16_t input;
	vbm->vbmFile->read(vbm->vbmFile, &input, sizeof(input));
	vbm->vbmFile->seek(vbm->vbmFile, -sizeof(input), SEEK_CUR);
	return input & 0x3FF;
}

bool GBAVBMQueryReset(struct GBARRContext* rr) {
	if (!rr->isPlaying(rr)) {
		return false;
	}

	struct GBAVBMContext* vbm = (struct GBAVBMContext*) rr;
	uint16_t input;
	vbm->vbmFile->read(vbm->vbmFile, &input, sizeof(input));
	vbm->vbmFile->seek(vbm->vbmFile, -sizeof(input), SEEK_CUR);
	return input & 0x800;
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
	UNUSED(flags);
#ifndef USE_ZLIB
	UNUSED(rr);
	return 0;
#else
	struct GBAVBMContext* vbm = (struct GBAVBMContext*) rr;
	off_t pos = vbm->vbmFile->seek(vbm->vbmFile, 0, SEEK_CUR);
	uint32_t saveType, flashSize, sramOffset;
	vbm->vbmFile->seek(vbm->vbmFile, 0x18, SEEK_SET);
	vbm->vbmFile->read(vbm->vbmFile, &saveType, sizeof(saveType));
	vbm->vbmFile->read(vbm->vbmFile, &flashSize, sizeof(flashSize));
	vbm->vbmFile->seek(vbm->vbmFile, 0x38, SEEK_SET);
	vbm->vbmFile->read(vbm->vbmFile, &sramOffset, sizeof(sramOffset));
	if (!sramOffset) {
		vbm->vbmFile->seek(vbm->vbmFile, pos, SEEK_SET);
		return 0;
	}
	vbm->vbmFile->seek(vbm->vbmFile, sramOffset, SEEK_SET);
	struct VFile* save = VFileMemChunk(0, 0);
	size_t size;
	switch (saveType) {
	case 1:
		size = SIZE_CART_SRAM;
		break;
	case 2:
		size = flashSize;
		if (size > SIZE_CART_FLASH1M) {
			size = SIZE_CART_FLASH1M;
		}
		break;
	case 3:
		size = SIZE_CART_EEPROM;
		break;
	default:
		size = SIZE_CART_FLASH1M;
		break;
	}
	uLong zlen = vbm->inputOffset - sramOffset;
	char buffer[8761];
	char* zbuffer = malloc(zlen);
	vbm->vbmFile->read(vbm->vbmFile, zbuffer, zlen);
	z_stream zstr;
	zstr.zalloc = Z_NULL;
	zstr.zfree = Z_NULL;
	zstr.opaque = Z_NULL;
	zstr.avail_in = zlen;
	zstr.next_in = (Bytef*) zbuffer;
	zstr.avail_out = 0;
	inflateInit2(&zstr, 31);
	// Skip header, we know where the save file is
	zstr.avail_out = sizeof(buffer);
	zstr.next_out = (Bytef*) &buffer;
	int err = inflate(&zstr, 0);
	while (err != Z_STREAM_END && !zstr.avail_out) {
		zstr.avail_out = sizeof(buffer);
		zstr.next_out = (Bytef*) &buffer;
		int err = inflate(&zstr, 0);
		if (err < 0) {
			break;
		}
		save->write(save, buffer, sizeof(buffer) - zstr.avail_out);
	}
	inflateEnd(&zstr);
	vbm->vbmFile->seek(vbm->vbmFile, pos, SEEK_SET);
	return save;
#endif
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
	if (flags & 2) {
#ifdef USE_ZLIB
		vbm->d.initFrom = INIT_FROM_SAVEGAME;
#else
		// zlib is needed to parse the savegame
		return false;
#endif
	}
	if (flags & 1) {
		// Incompatible savestate format
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
