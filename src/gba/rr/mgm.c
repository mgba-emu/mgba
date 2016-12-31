/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/rr/mgm.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/serialize.h>
#include <mgba-util/vfs.h>

#define BINARY_EXT ".mgm"
#define BINARY_MAGIC "GBAb"
#define METADATA_FILENAME "metadata" BINARY_EXT

enum {
	INVALID_INPUT = 0x8000
};

static void GBAMGMContextDestroy(struct GBARRContext*);

static bool GBAMGMStartPlaying(struct GBARRContext*, bool autorecord);
static void GBAMGMStopPlaying(struct GBARRContext*);
static bool GBAMGMStartRecording(struct GBARRContext*);
static void GBAMGMStopRecording(struct GBARRContext*);

static bool GBAMGMIsPlaying(const struct GBARRContext*);
static bool GBAMGMIsRecording(const struct GBARRContext*);

static void GBAMGMNextFrame(struct GBARRContext*);
static void GBAMGMLogInput(struct GBARRContext*, uint16_t input);
static uint16_t GBAMGMQueryInput(struct GBARRContext*);
static bool GBAMGMQueryReset(struct GBARRContext*);

static void GBAMGMStateSaved(struct GBARRContext* rr, struct GBASerializedState* state);
static void GBAMGMStateLoaded(struct GBARRContext* rr, const struct GBASerializedState* state);

static bool _loadStream(struct GBAMGMContext*, uint32_t streamId);
static bool _incrementStream(struct GBAMGMContext*, bool recursive);
static bool _finishSegment(struct GBAMGMContext*);
static bool _skipSegment(struct GBAMGMContext*);
static bool _markRerecord(struct GBAMGMContext*);

static bool _emitMagic(struct GBAMGMContext*, struct VFile* vf);
static bool _verifyMagic(struct GBAMGMContext*, struct VFile* vf);
static enum GBAMGMTag _readTag(struct GBAMGMContext*, struct VFile* vf);
static bool _seekTag(struct GBAMGMContext*, struct VFile* vf, enum GBAMGMTag tag);
static bool _emitTag(struct GBAMGMContext*, struct VFile* vf, uint8_t tag);
static bool _emitEnd(struct GBAMGMContext*, struct VFile* vf);

static bool _parseMetadata(struct GBAMGMContext*, struct VFile* vf);

static bool _markStreamNext(struct GBAMGMContext*, uint32_t newStreamId, bool recursive);
static void _streamEndReached(struct GBAMGMContext*);

static struct VFile* GBAMGMOpenSavedata(struct GBARRContext*, int flags);
static struct VFile* GBAMGMOpenSavestate(struct GBARRContext*, int flags);

void GBAMGMContextCreate(struct GBAMGMContext* mgm) {
	memset(mgm, 0, sizeof(*mgm));

	mgm->d.destroy = GBAMGMContextDestroy;

	mgm->d.startPlaying = GBAMGMStartPlaying;
	mgm->d.stopPlaying = GBAMGMStopPlaying;
	mgm->d.startRecording = GBAMGMStartRecording;
	mgm->d.stopRecording = GBAMGMStopRecording;

	mgm->d.isPlaying = GBAMGMIsPlaying;
	mgm->d.isRecording = GBAMGMIsRecording;

	mgm->d.nextFrame = GBAMGMNextFrame;
	mgm->d.logInput = GBAMGMLogInput;
	mgm->d.queryInput = GBAMGMQueryInput;
	mgm->d.queryReset = GBAMGMQueryReset;

	mgm->d.stateSaved = GBAMGMStateSaved;
	mgm->d.stateLoaded = GBAMGMStateLoaded;

	mgm->d.openSavedata = GBAMGMOpenSavedata;
	mgm->d.openSavestate = GBAMGMOpenSavestate;
}

void GBAMGMContextDestroy(struct GBARRContext* rr) {
	struct GBAMGMContext* mgm = (struct GBAMGMContext*) rr;
	if (mgm->metadataFile) {
		mgm->metadataFile->close(mgm->metadataFile);
	}
}

bool GBAMGMSetStream(struct GBAMGMContext* mgm, struct VDir* stream) {
	if (mgm->movieStream && !mgm->movieStream->close(mgm->movieStream)) {
		return false;
	}

	if (mgm->metadataFile && !mgm->metadataFile->close(mgm->metadataFile)) {
		return false;
	}

	mgm->streamDir = stream;
	mgm->metadataFile = mgm->streamDir->openFile(mgm->streamDir, METADATA_FILENAME, O_CREAT | O_RDWR);
	mgm->currentInput = INVALID_INPUT;
	if (!_parseMetadata(mgm, mgm->metadataFile)) {
		mgm->metadataFile->close(mgm->metadataFile);
		mgm->metadataFile = 0;
		mgm->maxStreamId = 0;
	}
	mgm->streamId = 1;
	mgm->movieStream = 0;
	return true;
}

bool GBAMGMCreateStream(struct GBAMGMContext* mgm, enum GBARRInitFrom initFrom) {
	if (mgm->metadataFile) {
		mgm->metadataFile->truncate(mgm->metadataFile, 0);
	} else {
		mgm->metadataFile = mgm->streamDir->openFile(mgm->streamDir, METADATA_FILENAME, O_CREAT | O_TRUNC | O_RDWR);
	}
	_emitMagic(mgm, mgm->metadataFile);

	mgm->d.initFrom = initFrom;
	mgm->initFromOffset = mgm->metadataFile->seek(mgm->metadataFile, 0, SEEK_CUR);
	_emitTag(mgm, mgm->metadataFile, TAG_INIT | initFrom);

	mgm->streamId = 0;
	mgm->maxStreamId = 0;
	_emitTag(mgm, mgm->metadataFile, TAG_MAX_STREAM);
	mgm->maxStreamIdOffset = mgm->metadataFile->seek(mgm->metadataFile, 0, SEEK_CUR);
	mgm->metadataFile->write(mgm->metadataFile, &mgm->maxStreamId, sizeof(mgm->maxStreamId));

	mgm->d.rrCount = 0;
	_emitTag(mgm, mgm->metadataFile, TAG_RR_COUNT);
	mgm->rrCountOffset = mgm->metadataFile->seek(mgm->metadataFile, 0, SEEK_CUR);
	mgm->metadataFile->write(mgm->metadataFile, &mgm->d.rrCount, sizeof(mgm->d.rrCount));
	return true;
}

bool _loadStream(struct GBAMGMContext* mgm, uint32_t streamId) {
	if (mgm->movieStream && !mgm->movieStream->close(mgm->movieStream)) {
		return false;
	}
	mgm->movieStream = 0;
	mgm->streamId = streamId;
	mgm->currentInput = INVALID_INPUT;
	char buffer[14];
	snprintf(buffer, sizeof(buffer), "%u" BINARY_EXT, streamId);
	if (mgm->d.isRecording(&mgm->d)) {
		int flags = O_CREAT | O_RDWR;
		if (streamId > mgm->maxStreamId) {
			flags |= O_TRUNC;
		}
		mgm->movieStream = mgm->streamDir->openFile(mgm->streamDir, buffer, flags);
	} else if (mgm->d.isPlaying(&mgm->d)) {
		mgm->movieStream = mgm->streamDir->openFile(mgm->streamDir, buffer, O_RDONLY);
		mgm->peekedTag = TAG_INVALID;
		if (!mgm->movieStream || !_verifyMagic(mgm, mgm->movieStream) || !_seekTag(mgm, mgm->movieStream, TAG_BEGIN)) {
			mgm->d.stopPlaying(&mgm->d);
		}
	}
	mLOG(GBA_RR, DEBUG, "Loading segment: %u", streamId);
	mgm->d.frames = 0;
	mgm->d.lagFrames = 0;
	return true;
}

bool _incrementStream(struct GBAMGMContext* mgm, bool recursive) {
	uint32_t newStreamId = mgm->maxStreamId + 1;
	uint32_t oldStreamId = mgm->streamId;
	if (mgm->d.isRecording(&mgm->d) && mgm->movieStream) {
		if (!_markStreamNext(mgm, newStreamId, recursive)) {
			return false;
		}
	}
	if (!_loadStream(mgm, newStreamId)) {
		return false;
	}
	mLOG(GBA_RR, DEBUG, "New segment: %u", newStreamId);
	_emitMagic(mgm, mgm->movieStream);
	mgm->maxStreamId = newStreamId;
	_emitTag(mgm, mgm->movieStream, TAG_PREVIOUSLY);
	mgm->movieStream->write(mgm->movieStream, &oldStreamId, sizeof(oldStreamId));
	_emitTag(mgm, mgm->movieStream, TAG_BEGIN);

	mgm->metadataFile->seek(mgm->metadataFile, mgm->maxStreamIdOffset, SEEK_SET);
	mgm->metadataFile->write(mgm->metadataFile, &mgm->maxStreamId, sizeof(mgm->maxStreamId));
	mgm->previously = oldStreamId;
	return true;
}

bool GBAMGMStartPlaying(struct GBARRContext* rr, bool autorecord) {
	if (rr->isRecording(rr) || rr->isPlaying(rr)) {
		return false;
	}

	struct GBAMGMContext* mgm = (struct GBAMGMContext*) rr;
	mgm->isPlaying = true;
	if (!_loadStream(mgm, 1)) {
		mgm->isPlaying = false;
		return false;
	}
	mgm->autorecord = autorecord;
	return true;
}

void GBAMGMStopPlaying(struct GBARRContext* rr) {
	if (!rr->isPlaying(rr)) {
		return;
	}

	struct GBAMGMContext* mgm = (struct GBAMGMContext*) rr;
	mgm->isPlaying = false;
	if (mgm->movieStream) {
		mgm->movieStream->close(mgm->movieStream);
		mgm->movieStream = 0;
	}
}

bool GBAMGMStartRecording(struct GBARRContext* rr) {
	if (rr->isRecording(rr) || rr->isPlaying(rr)) {
		return false;
	}

	struct GBAMGMContext* mgm = (struct GBAMGMContext*) rr;
	if (!mgm->maxStreamIdOffset) {
		_emitTag(mgm, mgm->metadataFile, TAG_MAX_STREAM);
		mgm->maxStreamIdOffset = mgm->metadataFile->seek(mgm->metadataFile, 0, SEEK_CUR);
		mgm->metadataFile->write(mgm->metadataFile, &mgm->maxStreamId, sizeof(mgm->maxStreamId));
	}

	mgm->isRecording = true;
	return _incrementStream(mgm, false);
}

void GBAMGMStopRecording(struct GBARRContext* rr) {
	if (!rr->isRecording(rr)) {
		return;
	}

	struct GBAMGMContext* mgm = (struct GBAMGMContext*) rr;
	mgm->isRecording = false;
	if (mgm->movieStream) {
		_emitEnd(mgm, mgm->movieStream);
		mgm->movieStream->close(mgm->movieStream);
		mgm->movieStream = 0;
	}
}

bool GBAMGMIsPlaying(const struct GBARRContext* rr) {
	const struct GBAMGMContext* mgm = (const struct GBAMGMContext*) rr;
	return mgm->isPlaying;
}

bool GBAMGMIsRecording(const struct GBARRContext* rr) {
	const struct GBAMGMContext* mgm = (const struct GBAMGMContext*) rr;
	return mgm->isRecording;
}

void GBAMGMNextFrame(struct GBARRContext* rr) {
	if (!rr->isRecording(rr) && !rr->isPlaying(rr)) {
		return;
	}

	struct GBAMGMContext* mgm = (struct GBAMGMContext*) rr;
	if (rr->isPlaying(rr)) {
		while (mgm->peekedTag == TAG_INPUT) {
			_readTag(mgm, mgm->movieStream);
			mLOG(GBA_RR, WARN, "Desync detected!");
		}
		if (mgm->peekedTag == TAG_LAG) {
			mLOG(GBA_RR, DEBUG, "Lag frame marked in stream");
			if (mgm->inputThisFrame) {
				mLOG(GBA_RR, WARN, "Lag frame in stream does not match movie");
			}
		}
	}

	++mgm->d.frames;
	mLOG(GBA_RR, DEBUG, "Frame: %u", mgm->d.frames);
	if (!mgm->inputThisFrame) {
		++mgm->d.lagFrames;
		mLOG(GBA_RR, DEBUG, "Lag frame: %u", mgm->d.lagFrames);
	}

	if (rr->isRecording(rr)) {
		if (!mgm->inputThisFrame) {
			_emitTag(mgm, mgm->movieStream, TAG_LAG);
		}
		_emitTag(mgm, mgm->movieStream, TAG_FRAME);
		mgm->inputThisFrame = false;
	} else {
		if (!_seekTag(mgm, mgm->movieStream, TAG_FRAME)) {
			_streamEndReached(mgm);
		}
	}
}

void GBAMGMLogInput(struct GBARRContext* rr, uint16_t keys) {
	if (!rr->isRecording(rr)) {
		return;
	}

	struct GBAMGMContext* mgm = (struct GBAMGMContext*) rr;
	if (keys != mgm->currentInput) {
		_emitTag(mgm, mgm->movieStream, TAG_INPUT);
		mgm->movieStream->write(mgm->movieStream, &keys, sizeof(keys));
		mgm->currentInput = keys;
	}
	mLOG(GBA_RR, DEBUG, "Input log: %03X", mgm->currentInput);
	mgm->inputThisFrame = true;
}

uint16_t GBAMGMQueryInput(struct GBARRContext* rr) {
	if (!rr->isPlaying(rr)) {
		return 0;
	}

	struct GBAMGMContext* mgm = (struct GBAMGMContext*) rr;
	if (mgm->peekedTag == TAG_INPUT) {
		_readTag(mgm, mgm->movieStream);
	}
	mgm->inputThisFrame = true;
	if (mgm->currentInput == INVALID_INPUT) {
		mLOG(GBA_RR, WARN, "Stream did not specify input");
	}
	mLOG(GBA_RR, DEBUG, "Input replay: %03X", mgm->currentInput);
	return mgm->currentInput;
}

bool GBAMGMQueryReset(struct GBARRContext* rr) {
	if (!rr->isPlaying(rr)) {
		return 0;
	}

	struct GBAMGMContext* mgm = (struct GBAMGMContext*) rr;
	return mgm->peekedTag == TAG_RESET;
}

void GBAMGMStateSaved(struct GBARRContext* rr, struct GBASerializedState* state) {
	struct GBAMGMContext* mgm = (struct GBAMGMContext*) rr;
	if (rr->isRecording(rr)) {
		state->associatedStreamId = mgm->streamId;
		_finishSegment(mgm);
	}
}

void GBAMGMStateLoaded(struct GBARRContext* rr, const struct GBASerializedState* state) {
	struct GBAMGMContext* mgm = (struct GBAMGMContext*) rr;
	if (rr->isRecording(rr)) {
		if (state->associatedStreamId != mgm->streamId) {
			_loadStream(mgm, state->associatedStreamId);
			_incrementStream(mgm, true);
		} else {
			_finishSegment(mgm);
		}
		_markRerecord(mgm);
	} else if (rr->isPlaying(rr)) {
		_loadStream(mgm, state->associatedStreamId);
		_skipSegment(mgm);
	}
}

bool _finishSegment(struct GBAMGMContext* mgm) {
	if (mgm->movieStream) {
		if (!_emitEnd(mgm, mgm->movieStream)) {
			return false;
		}
	}
	return _incrementStream(mgm, false);
}

bool _skipSegment(struct GBAMGMContext* mgm) {
	mgm->nextTime = 0;
	while (_readTag(mgm, mgm->movieStream) != TAG_EOF);
	if (!mgm->nextTime || !_loadStream(mgm, mgm->nextTime)) {
		_streamEndReached(mgm);
		return false;
	}
	return true;
}

bool _markRerecord(struct GBAMGMContext* mgm) {
	++mgm->d.rrCount;
	mgm->metadataFile->seek(mgm->metadataFile, mgm->rrCountOffset, SEEK_SET);
	mgm->metadataFile->write(mgm->metadataFile, &mgm->d.rrCount, sizeof(mgm->d.rrCount));
	return true;
}

bool _emitMagic(struct GBAMGMContext* mgm, struct VFile* vf) {
	UNUSED(mgm);
	return vf->write(vf, BINARY_MAGIC, 4) == 4;
}

bool _verifyMagic(struct GBAMGMContext* mgm, struct VFile* vf) {
	UNUSED(mgm);
	char buffer[4];
	if (vf->read(vf, buffer, sizeof(buffer)) != sizeof(buffer)) {
		return false;
	}
	if (memcmp(buffer, BINARY_MAGIC, sizeof(buffer)) != 0) {
		return false;
	}
	return true;
}

enum GBAMGMTag _readTag(struct GBAMGMContext* mgm, struct VFile* vf) {
	if (!mgm || !vf) {
		return TAG_EOF;
	}

	enum GBAMGMTag tag = mgm->peekedTag;
	switch (tag) {
	case TAG_INPUT:
		vf->read(vf, &mgm->currentInput, sizeof(uint16_t));
		break;
	case TAG_PREVIOUSLY:
		vf->read(vf, &mgm->previously, sizeof(mgm->previously));
		break;
	case TAG_NEXT_TIME:
		vf->read(vf, &mgm->nextTime, sizeof(mgm->nextTime));
		break;
	case TAG_MAX_STREAM:
		vf->read(vf, &mgm->maxStreamId, sizeof(mgm->maxStreamId));
		break;
	case TAG_FRAME_COUNT:
		vf->read(vf, &mgm->d.frames, sizeof(mgm->d.frames));
		break;
	case TAG_LAG_COUNT:
		vf->read(vf, &mgm->d.lagFrames, sizeof(mgm->d.lagFrames));
		break;
	case TAG_RR_COUNT:
		vf->read(vf, &mgm->d.rrCount, sizeof(mgm->d.rrCount));
		break;

	case TAG_INIT_EX_NIHILO:
		mgm->d.initFrom = INIT_EX_NIHILO;
		break;
	case TAG_INIT_FROM_SAVEGAME:
		mgm->d.initFrom = INIT_FROM_SAVEGAME;
		break;
	case TAG_INIT_FROM_SAVESTATE:
		mgm->d.initFrom = INIT_FROM_SAVESTATE;
		break;
	case TAG_INIT_FROM_BOTH:
		mgm->d.initFrom = INIT_FROM_BOTH;
		break;

	// To be spec'd
	case TAG_AUTHOR:
	case TAG_COMMENT:
		break;

	// Empty markers
	case TAG_FRAME:
	case TAG_LAG:
	case TAG_RESET:
	case TAG_BEGIN:
	case TAG_END:
	case TAG_INVALID:
	case TAG_EOF:
		break;
	}

	uint8_t tagBuffer;
	if (vf->read(vf, &tagBuffer, 1) != 1) {
		mgm->peekedTag = TAG_EOF;
	} else {
		mgm->peekedTag = tagBuffer;
	}

	if (mgm->peekedTag == TAG_END) {
		_skipSegment(mgm);
	}
	return tag;
}

bool _seekTag(struct GBAMGMContext* mgm, struct VFile* vf, enum GBAMGMTag tag) {
	enum GBAMGMTag readTag;
	while ((readTag = _readTag(mgm, vf)) != tag) {
		if (readTag == TAG_EOF) {
			return false;
		}
	}
	return true;
}

bool _emitTag(struct GBAMGMContext* mgm, struct VFile* vf, uint8_t tag) {
	UNUSED(mgm);
	return vf->write(vf, &tag, sizeof(tag)) == sizeof(tag);
}

bool _parseMetadata(struct GBAMGMContext* mgm, struct VFile* vf) {
	if (!_verifyMagic(mgm, vf)) {
		return false;
	}
	while (_readTag(mgm, vf) != TAG_EOF) {
		switch (mgm->peekedTag) {
		case TAG_MAX_STREAM:
			mgm->maxStreamIdOffset = vf->seek(vf, 0, SEEK_CUR);
			break;
		case TAG_INIT_EX_NIHILO:
		case TAG_INIT_FROM_SAVEGAME:
		case TAG_INIT_FROM_SAVESTATE:
		case TAG_INIT_FROM_BOTH:
			mgm->initFromOffset = vf->seek(vf, 0, SEEK_CUR);
			break;
		case TAG_RR_COUNT:
			mgm->rrCountOffset = vf->seek(vf, 0, SEEK_CUR);
			break;
		default:
			break;
		}
	}
	return true;
}

bool _emitEnd(struct GBAMGMContext* mgm, struct VFile* vf) {
	// TODO: Error check
	_emitTag(mgm, vf, TAG_END);
	_emitTag(mgm, vf, TAG_FRAME_COUNT);
	vf->write(vf, &mgm->d.frames, sizeof(mgm->d.frames));
	_emitTag(mgm, vf, TAG_LAG_COUNT);
	vf->write(vf, &mgm->d.lagFrames, sizeof(mgm->d.lagFrames));
	_emitTag(mgm, vf, TAG_NEXT_TIME);

	uint32_t newStreamId = 0;
	vf->write(vf, &newStreamId, sizeof(newStreamId));
	return true;
}

bool _markStreamNext(struct GBAMGMContext* mgm, uint32_t newStreamId, bool recursive) {
	if (mgm->movieStream->seek(mgm->movieStream, -sizeof(newStreamId) - 1, SEEK_END) < 0) {
		return false;
	}

	uint8_t tagBuffer;
	if (mgm->movieStream->read(mgm->movieStream, &tagBuffer, 1) != 1) {
		return false;
	}
	if (tagBuffer != TAG_NEXT_TIME) {
		return false;
	}
	if (mgm->movieStream->write(mgm->movieStream, &newStreamId, sizeof(newStreamId)) != sizeof(newStreamId)) {
		return false;
	}
	if (recursive) {
		if (mgm->movieStream->seek(mgm->movieStream, 0, SEEK_SET) < 0) {
			return false;
		}
		if (!_verifyMagic(mgm, mgm->movieStream)) {
			return false;
		}
		_readTag(mgm, mgm->movieStream);
		if (_readTag(mgm, mgm->movieStream) != TAG_PREVIOUSLY) {
			return false;
		}
		if (mgm->previously == 0) {
			return true;
		}
		uint32_t currentStreamId = mgm->streamId;
		if (!_loadStream(mgm, mgm->previously)) {
			return false;
		}
		return _markStreamNext(mgm, currentStreamId, mgm->previously);
	}
	return true;
}

void _streamEndReached(struct GBAMGMContext* mgm) {
	if (!mgm->d.isPlaying(&mgm->d)) {
		return;
	}

	uint32_t endStreamId = mgm->streamId;
	mgm->d.stopPlaying(&mgm->d);
	if (mgm->autorecord) {
		mgm->isRecording = true;
		_loadStream(mgm, endStreamId);
		_incrementStream(mgm, false);
	}
}

struct VFile* GBAMGMOpenSavedata(struct GBARRContext* rr, int flags) {
	struct GBAMGMContext* mgm = (struct GBAMGMContext*) rr;
	return mgm->streamDir->openFile(mgm->streamDir, "movie.sav", flags);
}

struct VFile* GBAMGMOpenSavestate(struct GBARRContext* rr, int flags) {
	struct GBAMGMContext* mgm = (struct GBAMGMContext*) rr;
	return mgm->streamDir->openFile(mgm->streamDir, "movie.ssm", flags);
}
