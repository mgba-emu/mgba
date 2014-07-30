#include "gba-rr.h"

#include "gba.h"
#include "util/vfs.h"

#define BINEXT ".dat"
#define METADATA_FILENAME "metadata" BINEXT

static enum GBARRTag _readTag(struct GBARRContext* rr, struct VFile* vf);
static bool _seekTag(struct GBARRContext* rr, struct VFile* vf, enum GBARRTag tag);
static bool _emitTag(struct GBARRContext* rr, struct VFile* vf, uint8_t tag);
static bool _parseMetadata(struct GBARRContext* rr, struct VFile* vf);

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

	free(gba->rr);
	gba->rr = 0;
}

bool GBARRSetStream(struct GBARRContext* rr, struct VDir* stream) {
	if (rr->movieStream && !rr->movieStream->close(rr->movieStream)) {
		return false;
	}

	if (rr->metadataFile && !rr->metadataFile->close(rr->metadataFile)) {
		return false;
	}

	rr->streamDir = stream;
	rr->metadataFile = rr->streamDir->openFile(rr->streamDir, METADATA_FILENAME, O_CREAT | O_RDWR);
	if (!_parseMetadata(rr, rr->metadataFile)) {
		rr->metadataFile->close(rr->metadataFile);
		rr->streamDir = 0;
		rr->metadataFile = 0;
		return false;
	}
	rr->movieStream = 0;
	rr->streamId = 1;
	rr->maxStreamId = 1;
	return true;
}

bool GBARRLoadStream(struct GBARRContext* rr, uint32_t streamId) {
	if (rr->movieStream && !rr->movieStream->close(rr->movieStream)) {
		return false;
	}
	rr->movieStream = 0;
	rr->streamId = streamId;
	char buffer[14];
	snprintf(buffer, sizeof(buffer), "%u" BINEXT, streamId);
	if (GBARRIsRecording(rr)) {
		int flags = O_CREAT | O_WRONLY;
		if (streamId > rr->maxStreamId) {
			flags |= O_TRUNC;
		} else {
			flags |= O_APPEND;
		}
		rr->movieStream = rr->streamDir->openFile(rr->streamDir, buffer, flags);
	} else if (GBARRIsPlaying(rr)) {
		rr->movieStream = rr->streamDir->openFile(rr->streamDir, buffer, O_RDONLY);
		rr->peekedTag = TAG_INVALID;
		if (!rr->movieStream || !_seekTag(rr, rr->movieStream, TAG_BEGIN)) {
			GBARRStopPlaying(rr);
		}
	}
	rr->frames = 0;
	rr->lagFrames = 0;
	return true;
}

bool GBARRIncrementStream(struct GBARRContext* rr) {
	uint32_t newStreamId = rr->maxStreamId + 1;
	uint32_t oldStreamId = rr->streamId;
	if (GBARRIsRecording(rr) && rr->movieStream) {
		_emitTag(rr, rr->movieStream, TAG_END);
		_emitTag(rr, rr->movieStream, TAG_FRAME_COUNT);
		rr->movieStream->write(rr->movieStream, &rr->frames, sizeof(rr->frames));
		_emitTag(rr, rr->movieStream, TAG_LAG_COUNT);
		rr->movieStream->write(rr->movieStream, &rr->lagFrames, sizeof(rr->lagFrames));
		_emitTag(rr, rr->movieStream, TAG_NEXT_TIME);
		rr->movieStream->write(rr->movieStream, &newStreamId, sizeof(newStreamId));
	}
	if (!GBARRLoadStream(rr, newStreamId)) {
		return false;
	}
	rr->maxStreamId = newStreamId;
	_emitTag(rr, rr->movieStream, TAG_PREVIOUSLY);
	rr->movieStream->write(rr->movieStream, &oldStreamId, sizeof(oldStreamId));
	_emitTag(rr, rr->movieStream, TAG_BEGIN);

	rr->metadataFile->seek(rr->metadataFile, rr->maxStreamIdOffset, SEEK_SET);
	rr->metadataFile->write(rr->movieStream, &rr->maxStreamId, sizeof(rr->maxStreamId));
	return true;
}

bool GBARRStartPlaying(struct GBARRContext* rr, bool autorecord) {
	if (GBARRIsRecording(rr) || GBARRIsPlaying(rr)) {
		return false;
	}

	char buffer[14];
	snprintf(buffer, sizeof(buffer), "%u" BINEXT, rr->streamId);
	rr->movieStream = rr->streamDir->openFile(rr->streamDir, buffer, O_RDONLY);
	if (!rr->movieStream) {
		return false;
	}

	rr->autorecord = autorecord;
	rr->peekedTag = TAG_INVALID;
	_readTag(rr, rr->movieStream); // Discard the buffer
	enum GBARRTag tag = _readTag(rr, rr->movieStream);
	if (tag != TAG_BEGIN) {
		rr->movieStream->close(rr->movieStream);
		rr->movieStream = 0;
		return false;
	}

	rr->isPlaying = true;
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

	char buffer[14];
	snprintf(buffer, sizeof(buffer), "%u" BINEXT, rr->streamId);
	rr->movieStream = rr->streamDir->openFile(rr->streamDir, buffer, O_TRUNC | O_CREAT | O_WRONLY);
	if (!_emitTag(rr, rr->movieStream, TAG_BEGIN)) {
		rr->movieStream->close(rr->movieStream);
		rr->movieStream = 0;
		return false;
	}

	if (!rr->maxStreamIdOffset) {
		_emitTag(rr, rr->metadataFile, TAG_MAX_STREAM);
		rr->maxStreamIdOffset = rr->metadataFile->seek(rr->metadataFile, 0, SEEK_CUR);
		rr->metadataFile->write(rr->metadataFile, &rr->maxStreamId, sizeof(rr->maxStreamId));
	}

	rr->isRecording = true;
	return true;
}

void GBARRStopRecording(struct GBARRContext* rr) {
	if (!GBARRIsRecording(rr)) {
		return;
	}
	rr->isRecording = false;
	if (rr->movieStream) {
		_emitTag(rr, rr->movieStream, TAG_END);
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

	++rr->frames;
	if (!rr->inputThisFrame) {
		++rr->lagFrames;
	}

	if (GBARRIsRecording(rr)) {
		if (!rr->inputThisFrame) {
			_emitTag(rr, rr->movieStream, TAG_LAG);
		}
		_emitTag(rr, rr->movieStream, TAG_FRAME);

		rr->inputThisFrame = false;
	} else {
		if (!_seekTag(rr, rr->movieStream, TAG_FRAME)) {
			uint32_t endStreamId = rr->streamId;
			GBARRStopPlaying(rr);
			if (rr->autorecord) {
				rr->isRecording = true;
				GBARRLoadStream(rr, endStreamId);
				GBARRIncrementStream(rr);
			}
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
	rr->inputThisFrame = true;
}

uint16_t GBARRQueryInput(struct GBARRContext* rr) {
	if (!GBARRIsPlaying(rr)) {
		return 0;
	}

	if (rr->peekedTag == TAG_INPUT) {
		_readTag(rr, rr->movieStream);
	}
	return rr->currentInput;
}

bool GBARRSkipSegment(struct GBARRContext* rr) {
	rr->nextTime = 0;
	while (_readTag(rr, rr->movieStream) != TAG_EOF);
	if (!rr->nextTime || !GBARRLoadStream(rr, rr->nextTime)) {
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

	// To be spec'd
	case TAG_RR_COUNT:
	case TAG_INIT_TYPE:
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
	return tag;
}

bool _seekTag(struct GBARRContext* rr, struct VFile* vf, enum GBARRTag tag) {
	enum GBARRTag readTag;
	while ((readTag = _readTag(rr, vf)) != tag) {
		if (vf == rr->movieStream && readTag == TAG_END) {
			if (!GBARRSkipSegment(rr)) {
				return false;
			}
			vf = rr->movieStream;
		} else if (readTag == TAG_EOF) {
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
	while (_readTag(rr, vf) != TAG_EOF) {
		if (rr->peekedTag == TAG_MAX_STREAM) {
			rr->maxStreamIdOffset = vf->seek(vf, 0, SEEK_CUR);
		}
	}
	rr->maxStreamIdOffset = vf->seek(vf, 0, SEEK_SET);
	return true;
}
