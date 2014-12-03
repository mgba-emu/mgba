/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba-rr.h"

#include "gba.h"
#include "gba-serialize.h"
#include "util/vfs.h"

#define BINARY_EXT ".dat"
#define BINARY_MAGIC "GBAb"
#define METADATA_FILENAME "metadata" BINARY_EXT

enum {
	INVALID_INPUT = 0x8000
};

static bool _emitMagic(struct GBARRContext* rr, struct VFile* vf);
static bool _verifyMagic(struct GBARRContext* rr, struct VFile* vf);
static enum GBARRTag _readTag(struct GBARRContext* rr, struct VFile* vf);
static bool _seekTag(struct GBARRContext* rr, struct VFile* vf, enum GBARRTag tag);
static bool _emitTag(struct GBARRContext* rr, struct VFile* vf, uint8_t tag);
static bool _emitEnd(struct GBARRContext* rr, struct VFile* vf);

static bool _parseMetadata(struct GBARRContext* rr, struct VFile* vf);

static bool _markStreamNext(struct GBARRContext* rr, uint32_t newStreamId, bool recursive);
static void _streamEndReached(struct GBARRContext* rr);

static struct VFile* _openSavedata(struct GBARRContext* rr, int flags);
static struct VFile* _openSavestate(struct GBARRContext* rr, int flags);

void GBARRContextCreate(struct GBA* gba) {
	if (gba->rr) {
		return;
	}

	gba->rr = calloc(1, sizeof(*gba->rr));
}

void GBARRContextDestroy(struct GBA* gba) {
	if (!gba->rr) {
		return;
	}

	if (GBARRIsPlaying(gba->rr)) {
		GBARRStopPlaying(gba->rr);
	}
	if (GBARRIsRecording(gba->rr)) {
		GBARRStopRecording(gba->rr);
	}
	if (gba->rr->metadataFile) {
		gba->rr->metadataFile->close(gba->rr->metadataFile);
	}
	if (gba->rr->savedata) {
		gba->rr->savedata->close(gba->rr->savedata);
	}

	free(gba->rr);
	gba->rr = 0;
}

void GBARRSaveState(struct GBA* gba) {
	if (!gba || !gba->rr) {
		return;
	}

	if (gba->rr->initFrom & INIT_FROM_SAVEGAME) {
		if (gba->rr->savedata) {
			gba->rr->savedata->close(gba->rr->savedata);
		}
		gba->rr->savedata = _openSavedata(gba->rr, O_TRUNC | O_CREAT | O_WRONLY);
		GBASavedataClone(&gba->memory.savedata, gba->rr->savedata);
		gba->rr->savedata->close(gba->rr->savedata);
		gba->rr->savedata = _openSavedata(gba->rr, O_RDONLY);
		GBASavedataMask(&gba->memory.savedata, gba->rr->savedata);
	} else {
		GBASavedataMask(&gba->memory.savedata, 0);
	}

	if (gba->rr->initFrom & INIT_FROM_SAVESTATE) {
		struct VFile* vf = _openSavestate(gba->rr, O_TRUNC | O_CREAT | O_RDWR);
		GBASaveStateNamed(gba, vf, false);
		vf->close(vf);
	} else {
		ARMReset(gba->cpu);
	}
}

void GBARRLoadState(struct GBA* gba) {
	if (!gba || !gba->rr) {
		return;
	}

	if (gba->rr->initFrom & INIT_FROM_SAVEGAME) {
		if (gba->rr->savedata) {
			gba->rr->savedata->close(gba->rr->savedata);
		}
		gba->rr->savedata = _openSavedata(gba->rr, O_RDONLY);
		GBASavedataMask(&gba->memory.savedata, gba->rr->savedata);
	} else {
		GBASavedataMask(&gba->memory.savedata, 0);
	}

	if (gba->rr->initFrom & INIT_FROM_SAVESTATE) {
		struct VFile* vf = _openSavestate(gba->rr, O_RDONLY);
		GBALoadStateNamed(gba, vf);
		vf->close(vf);
	} else {
		ARMReset(gba->cpu);
	}
}

bool GBARRInitStream(struct GBARRContext* rr, struct VDir* stream) {
	if (rr->movieStream && !rr->movieStream->close(rr->movieStream)) {
		return false;
	}

	if (rr->metadataFile && !rr->metadataFile->close(rr->metadataFile)) {
		return false;
	}

	rr->streamDir = stream;
	rr->metadataFile = rr->streamDir->openFile(rr->streamDir, METADATA_FILENAME, O_CREAT | O_RDWR);
	rr->currentInput = INVALID_INPUT;
	if (!_parseMetadata(rr, rr->metadataFile)) {
		rr->metadataFile->close(rr->metadataFile);
		rr->metadataFile = 0;
		rr->maxStreamId = 0;
	}
	rr->streamId = 1;
	rr->movieStream = 0;
	return true;
}

bool GBARRReinitStream(struct GBARRContext* rr, enum GBARRInitFrom initFrom) {
	if (!rr) {
		return false;
	}

	if (rr->metadataFile) {
		rr->metadataFile->truncate(rr->metadataFile, 0);
	} else {
		rr->metadataFile = rr->streamDir->openFile(rr->streamDir, METADATA_FILENAME, O_CREAT | O_TRUNC | O_RDWR);
	}
	_emitMagic(rr, rr->metadataFile);

	rr->initFrom = initFrom;
	rr->initFromOffset = rr->metadataFile->seek(rr->metadataFile, 0, SEEK_CUR);
	_emitTag(rr, rr->metadataFile, TAG_INIT | initFrom);

	rr->streamId = 0;
	rr->maxStreamId = 0;
	_emitTag(rr, rr->metadataFile, TAG_MAX_STREAM);
	rr->maxStreamIdOffset = rr->metadataFile->seek(rr->metadataFile, 0, SEEK_CUR);
	rr->metadataFile->write(rr->metadataFile, &rr->maxStreamId, sizeof(rr->maxStreamId));

	rr->rrCount = 0;
	_emitTag(rr, rr->metadataFile, TAG_RR_COUNT);
	rr->rrCountOffset = rr->metadataFile->seek(rr->metadataFile, 0, SEEK_CUR);
	rr->metadataFile->write(rr->metadataFile, &rr->rrCount, sizeof(rr->rrCount));
	return true;
}

bool GBARRLoadStream(struct GBARRContext* rr, uint32_t streamId) {
	if (rr->movieStream && !rr->movieStream->close(rr->movieStream)) {
		return false;
	}
	rr->movieStream = 0;
	rr->streamId = streamId;
	rr->currentInput = INVALID_INPUT;
	char buffer[14];
	snprintf(buffer, sizeof(buffer), "%u" BINARY_EXT, streamId);
	if (GBARRIsRecording(rr)) {
		int flags = O_CREAT | O_RDWR;
		if (streamId > rr->maxStreamId) {
			flags |= O_TRUNC;
		}
		rr->movieStream = rr->streamDir->openFile(rr->streamDir, buffer, flags);
	} else if (GBARRIsPlaying(rr)) {
		rr->movieStream = rr->streamDir->openFile(rr->streamDir, buffer, O_RDONLY);
		rr->peekedTag = TAG_INVALID;
		if (!rr->movieStream || !_verifyMagic(rr, rr->movieStream) || !_seekTag(rr, rr->movieStream, TAG_BEGIN)) {
			GBARRStopPlaying(rr);
		}
	}
	GBALog(0, GBA_LOG_DEBUG, "[RR] Loading segment: %u", streamId);
	rr->frames = 0;
	rr->lagFrames = 0;
	return true;
}

bool GBARRIncrementStream(struct GBARRContext* rr, bool recursive) {
	uint32_t newStreamId = rr->maxStreamId + 1;
	uint32_t oldStreamId = rr->streamId;
	if (GBARRIsRecording(rr) && rr->movieStream) {
		if (!_markStreamNext(rr, newStreamId, recursive)) {
			return false;
		}
	}
	if (!GBARRLoadStream(rr, newStreamId)) {
		return false;
	}
	GBALog(0, GBA_LOG_DEBUG, "[RR] New segment: %u", newStreamId);
	_emitMagic(rr, rr->movieStream);
	rr->maxStreamId = newStreamId;
	_emitTag(rr, rr->movieStream, TAG_PREVIOUSLY);
	rr->movieStream->write(rr->movieStream, &oldStreamId, sizeof(oldStreamId));
	_emitTag(rr, rr->movieStream, TAG_BEGIN);

	rr->metadataFile->seek(rr->metadataFile, rr->maxStreamIdOffset, SEEK_SET);
	rr->metadataFile->write(rr->metadataFile, &rr->maxStreamId, sizeof(rr->maxStreamId));
	rr->previously = oldStreamId;
	return true;
}

bool GBARRStartPlaying(struct GBARRContext* rr, bool autorecord) {
	if (GBARRIsRecording(rr) || GBARRIsPlaying(rr)) {
		return false;
	}

	rr->isPlaying = true;
	if (!GBARRLoadStream(rr, 1)) {
		rr->isPlaying = false;
		return false;
	}
	rr->autorecord = autorecord;
	return true;
}

void GBARRStopPlaying(struct GBARRContext* rr) {
	if (!GBARRIsPlaying(rr)) {
		return;
	}
	rr->isPlaying = false;
	if (rr->movieStream) {
		rr->movieStream->close(rr->movieStream);
		rr->movieStream = 0;
	}
}

bool GBARRStartRecording(struct GBARRContext* rr) {
	if (GBARRIsRecording(rr) || GBARRIsPlaying(rr)) {
		return false;
	}

	if (!rr->maxStreamIdOffset) {
		_emitTag(rr, rr->metadataFile, TAG_MAX_STREAM);
		rr->maxStreamIdOffset = rr->metadataFile->seek(rr->metadataFile, 0, SEEK_CUR);
		rr->metadataFile->write(rr->metadataFile, &rr->maxStreamId, sizeof(rr->maxStreamId));
	}

	rr->isRecording = true;
	return GBARRIncrementStream(rr, false);
}

void GBARRStopRecording(struct GBARRContext* rr) {
	if (!GBARRIsRecording(rr)) {
		return;
	}
	rr->isRecording = false;
	if (rr->movieStream) {
		_emitEnd(rr, rr->movieStream);
		rr->movieStream->close(rr->movieStream);
		rr->movieStream = 0;
	}
}

bool GBARRIsPlaying(struct GBARRContext* rr) {
	return rr && rr->isPlaying;
}

bool GBARRIsRecording(struct GBARRContext* rr) {
	return rr && rr->isRecording;
}

void GBARRNextFrame(struct GBARRContext* rr) {
	if (!GBARRIsRecording(rr) && !GBARRIsPlaying(rr)) {
		return;
	}

	if (GBARRIsPlaying(rr)) {
		while (rr->peekedTag == TAG_INPUT) {
			_readTag(rr, rr->movieStream);
			GBALog(0, GBA_LOG_WARN, "[RR] Desync detected!");
		}
		if (rr->peekedTag == TAG_LAG) {
			GBALog(0, GBA_LOG_DEBUG, "[RR] Lag frame marked in stream");
			if (rr->inputThisFrame) {
				GBALog(0, GBA_LOG_WARN, "[RR] Lag frame in stream does not match movie");
			}
		}
	}

	++rr->frames;
	GBALog(0, GBA_LOG_DEBUG, "[RR] Frame: %u", rr->frames);
	if (!rr->inputThisFrame) {
		++rr->lagFrames;
		GBALog(0, GBA_LOG_DEBUG, "[RR] Lag frame: %u", rr->lagFrames);
	}

	if (GBARRIsRecording(rr)) {
		if (!rr->inputThisFrame) {
			_emitTag(rr, rr->movieStream, TAG_LAG);
		}
		_emitTag(rr, rr->movieStream, TAG_FRAME);
		rr->inputThisFrame = false;
	} else {
		if (!_seekTag(rr, rr->movieStream, TAG_FRAME)) {
			_streamEndReached(rr);
		}
	}
}

void GBARRLogInput(struct GBARRContext* rr, uint16_t keys) {
	if (!GBARRIsRecording(rr)) {
		return;
	}

	if (keys != rr->currentInput) {
		_emitTag(rr, rr->movieStream, TAG_INPUT);
		rr->movieStream->write(rr->movieStream, &keys, sizeof(keys));
		rr->currentInput = keys;
	}
	GBALog(0, GBA_LOG_DEBUG, "[RR] Input log: %03X", rr->currentInput);
	rr->inputThisFrame = true;
}

uint16_t GBARRQueryInput(struct GBARRContext* rr) {
	if (!GBARRIsPlaying(rr)) {
		return 0;
	}

	if (rr->peekedTag == TAG_INPUT) {
		_readTag(rr, rr->movieStream);
	}
	rr->inputThisFrame = true;
	if (rr->currentInput == INVALID_INPUT) {
		GBALog(0, GBA_LOG_WARN, "[RR] Stream did not specify input");
	}
	GBALog(0, GBA_LOG_DEBUG, "[RR] Input replay: %03X", rr->currentInput);
	return rr->currentInput;
}

bool GBARRFinishSegment(struct GBARRContext* rr) {
	if (rr->movieStream) {
		if (!_emitEnd(rr, rr->movieStream)) {
			return false;
		}
	}
	return GBARRIncrementStream(rr, false);
}

bool GBARRSkipSegment(struct GBARRContext* rr) {
	rr->nextTime = 0;
	while (_readTag(rr, rr->movieStream) != TAG_EOF);
	if (!rr->nextTime || !GBARRLoadStream(rr, rr->nextTime)) {
		_streamEndReached(rr);
		return false;
	}
	return true;
}

bool GBARRMarkRerecord(struct GBARRContext* rr) {
	++rr->rrCount;
	rr->metadataFile->seek(rr->metadataFile, rr->rrCountOffset, SEEK_SET);
	rr->metadataFile->write(rr->metadataFile, &rr->rrCount, sizeof(rr->rrCount));
	return true;
}

bool _emitMagic(struct GBARRContext* rr, struct VFile* vf) {
	UNUSED(rr);
	return vf->write(vf, BINARY_MAGIC, 4) == 4;
}

bool _verifyMagic(struct GBARRContext* rr, struct VFile* vf) {
	UNUSED(rr);
	char buffer[4];
	if (vf->read(vf, buffer, sizeof(buffer)) != sizeof(buffer)) {
		return false;
	}
	if (memcmp(buffer, BINARY_MAGIC, sizeof(buffer)) != 0) {
		return false;
	}
	return true;
}

enum GBARRTag _readTag(struct GBARRContext* rr, struct VFile* vf) {
	if (!rr || !vf) {
		return TAG_EOF;
	}

	enum GBARRTag tag = rr->peekedTag;
	switch (tag) {
	case TAG_INPUT:
		vf->read(vf, &rr->currentInput, sizeof(uint16_t));
		break;
	case TAG_PREVIOUSLY:
		vf->read(vf, &rr->previously, sizeof(rr->previously));
		break;
	case TAG_NEXT_TIME:
		vf->read(vf, &rr->nextTime, sizeof(rr->nextTime));
		break;
	case TAG_MAX_STREAM:
		vf->read(vf, &rr->maxStreamId, sizeof(rr->maxStreamId));
		break;
	case TAG_FRAME_COUNT:
		vf->read(vf, &rr->frames, sizeof(rr->frames));
		break;
	case TAG_LAG_COUNT:
		vf->read(vf, &rr->lagFrames, sizeof(rr->lagFrames));
		break;
	case TAG_RR_COUNT:
		vf->read(vf, &rr->rrCount, sizeof(rr->rrCount));
		break;

	case TAG_INIT_EX_NIHILO:
		rr->initFrom = INIT_EX_NIHILO;
		break;
	case TAG_INIT_FROM_SAVEGAME:
		rr->initFrom = INIT_FROM_SAVEGAME;
		break;
	case TAG_INIT_FROM_SAVESTATE:
		rr->initFrom = INIT_FROM_SAVESTATE;
	case TAG_INIT_FROM_BOTH:
		rr->initFrom = INIT_FROM_BOTH;
		break;

	// To be spec'd
	case TAG_AUTHOR:
	case TAG_COMMENT:
		break;

	// Empty markers
	case TAG_FRAME:
	case TAG_LAG:
	case TAG_BEGIN:
	case TAG_END:
	case TAG_INVALID:
	case TAG_EOF:
		break;
	}

	uint8_t tagBuffer;
	if (vf->read(vf, &tagBuffer, 1) != 1) {
		rr->peekedTag = TAG_EOF;
	} else {
		rr->peekedTag = tagBuffer;
	}

	if (rr->peekedTag == TAG_END) {
		GBARRSkipSegment(rr);
	}
	return tag;
}

bool _seekTag(struct GBARRContext* rr, struct VFile* vf, enum GBARRTag tag) {
	enum GBARRTag readTag;
	while ((readTag = _readTag(rr, vf)) != tag) {
		if (readTag == TAG_EOF) {
			return false;
		}
	}
	return true;
}

bool _emitTag(struct GBARRContext* rr, struct VFile* vf, uint8_t tag) {
	UNUSED(rr);
	return vf->write(vf, &tag, sizeof(tag)) == sizeof(tag);
}

bool _parseMetadata(struct GBARRContext* rr, struct VFile* vf) {
	if (!_verifyMagic(rr, vf)) {
		return false;
	}
	while (_readTag(rr, vf) != TAG_EOF) {
		switch (rr->peekedTag) {
		case TAG_MAX_STREAM:
			rr->maxStreamIdOffset = vf->seek(vf, 0, SEEK_CUR);
			break;
		case TAG_INIT_EX_NIHILO:
		case TAG_INIT_FROM_SAVEGAME:
		case TAG_INIT_FROM_SAVESTATE:
		case TAG_INIT_FROM_BOTH:
			rr->initFromOffset = vf->seek(vf, 0, SEEK_CUR);
			break;
		case TAG_RR_COUNT:
			rr->rrCountOffset = vf->seek(vf, 0, SEEK_CUR);
			break;
		default:
			break;
		}
	}
	return true;
}

bool _emitEnd(struct GBARRContext* rr, struct VFile* vf) {
	// TODO: Error check
	_emitTag(rr, vf, TAG_END);
	_emitTag(rr, vf, TAG_FRAME_COUNT);
	vf->write(vf, &rr->frames, sizeof(rr->frames));
	_emitTag(rr, vf, TAG_LAG_COUNT);
	vf->write(vf, &rr->lagFrames, sizeof(rr->lagFrames));
	_emitTag(rr, vf, TAG_NEXT_TIME);

	uint32_t newStreamId = 0;
	vf->write(vf, &newStreamId, sizeof(newStreamId));
	return true;
}

bool _markStreamNext(struct GBARRContext* rr, uint32_t newStreamId, bool recursive) {
	if (rr->movieStream->seek(rr->movieStream, -sizeof(newStreamId) - 1, SEEK_END) < 0) {
		return false;
	}

	uint8_t tagBuffer;
	if (rr->movieStream->read(rr->movieStream, &tagBuffer, 1) != 1) {
		return false;
	}
	if (tagBuffer != TAG_NEXT_TIME) {
		return false;
	}
	if (rr->movieStream->write(rr->movieStream, &newStreamId, sizeof(newStreamId)) != sizeof(newStreamId)) {
		return false;
	}
	if (recursive) {
		if (rr->movieStream->seek(rr->movieStream, 0, SEEK_SET) < 0) {
			return false;
		}
		if (!_verifyMagic(rr, rr->movieStream)) {
			return false;
		}
		_readTag(rr, rr->movieStream);
		if (_readTag(rr, rr->movieStream) != TAG_PREVIOUSLY) {
			return false;
		}
		if (rr->previously == 0) {
			return true;
		}
		uint32_t currentStreamId = rr->streamId;
		if (!GBARRLoadStream(rr, rr->previously)) {
			return false;
		}
		return _markStreamNext(rr, currentStreamId, rr->previously);
	}
	return true;
}

void _streamEndReached(struct GBARRContext* rr) {
	if (!GBARRIsPlaying(rr)) {
		return;
	}

	uint32_t endStreamId = rr->streamId;
	GBARRStopPlaying(rr);
	if (rr->autorecord) {
		rr->isRecording = true;
		GBARRLoadStream(rr, endStreamId);
		GBARRIncrementStream(rr, false);
	}
}

struct VFile* _openSavedata(struct GBARRContext* rr, int flags) {
	return rr->streamDir->openFile(rr->streamDir, "movie.sav", flags);
}

struct VFile* _openSavestate(struct GBARRContext* rr, int flags) {
	return rr->streamDir->openFile(rr->streamDir, "movie.ssm", flags);
}
