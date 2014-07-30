#include "gba-rr.h"

#include "gba.h"
#include "util/vfs.h"

static enum GBARRTag _readTag(struct GBARRContext* rr, struct VFile* vf);
static bool _seekTag(struct GBARRContext* rr, struct VFile* vf, enum GBARRTag tag);
static bool _emitTag(struct GBARRContext* rr, struct VFile* vf, uint8_t tag);

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

	free(gba->rr);
	gba->rr = 0;
}

bool GBARRSetStream(struct GBARRContext* rr, struct VDir* stream) {
	if (rr->movieStream && !rr->movieStream->close(rr->movieStream)) {
		return false;
	}
	rr->streamDir = stream;
	rr->movieStream = 0;
	rr->streamId = 1;
	return true;
}

bool GBARRLoadStream(struct GBARRContext* rr, uint32_t streamId) {
	if (rr->movieStream && !rr->movieStream->close(rr->movieStream)) {
		return false;
	}
	rr->movieStream = 0;
	rr->streamId = streamId;
	char buffer[14];
	snprintf(buffer, sizeof(buffer), "%u.log", streamId);
	if (GBARRIsRecording(rr)) {
		rr->movieStream = rr->streamDir->openFile(rr->streamDir, buffer, O_TRUNC | O_CREAT | O_WRONLY);
	} else if (GBARRIsPlaying(rr)) {
		rr->movieStream = rr->streamDir->openFile(rr->streamDir, buffer, O_RDONLY);
		rr->peekedTag = TAG_INVALID;
		if (!rr->movieStream || !_seekTag(rr, rr->movieStream, TAG_BEGIN)) {
			GBARRStopPlaying(rr);
		}
	}
	return true;
}

uint32_t GBARRIncrementStream(struct GBARRContext* rr) {
	uint32_t newStreamId = rr->streamId + 1;
	uint32_t oldStreamId = rr->streamId;
	if (GBARRIsRecording(rr) && rr->movieStream) {
		_emitTag(rr, rr->movieStream, TAG_END);
		_emitTag(rr, rr->movieStream, TAG_NEXT_TIME);
		rr->movieStream->write(rr->movieStream, &newStreamId, sizeof(newStreamId));
	}
	if (!GBARRLoadStream(rr, newStreamId)) {
		return 0;
	}
	_emitTag(rr, rr->movieStream, TAG_PREVIOUSLY);
	rr->movieStream->write(rr->movieStream, &oldStreamId, sizeof(oldStreamId));
	_emitTag(rr, rr->movieStream, TAG_BEGIN);
	return rr->streamId;
}

bool GBARRStartPlaying(struct GBARRContext* rr, bool autorecord) {
	if (GBARRIsRecording(rr) || GBARRIsPlaying(rr)) {
		return false;
	}

	char buffer[14];
	snprintf(buffer, sizeof(buffer), "%u.log", rr->streamId);
	rr->movieStream = rr->streamDir->openFile(rr->streamDir, buffer, O_RDONLY);
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
	snprintf(buffer, sizeof(buffer), "%u.log", rr->streamId);
	rr->movieStream = rr->streamDir->openFile(rr->streamDir, buffer, O_TRUNC | O_CREAT | O_WRONLY);
	if (!_emitTag(rr, rr->movieStream, TAG_BEGIN)) {
		rr->movieStream->close(rr->movieStream);
		rr->movieStream = 0;
		return false;
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
			GBARRStopPlaying(rr);
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

enum GBARRTag _readTag(struct GBARRContext* rr, struct VFile* vf) {
	enum GBARRTag tag = rr->peekedTag;
	switch (tag) {
	case TAG_INPUT:
		vf->read(vf, &rr->currentInput, sizeof(uint16_t));
		break;
	case TAG_NEXT_TIME:
		vf->read(vf, &rr->nextTime, sizeof(rr->nextTime));
		break;
	case TAG_FRAME:
	case TAG_LAG:
	case TAG_BEGIN:
	case TAG_END:
	case TAG_PREVIOUSLY:
	case TAG_INVALID:
		break;
	}

	uint8_t tagBuffer;
	if (vf->read(vf, &tagBuffer, 1) != 1) {
		tagBuffer = TAG_END;
	}
	rr->peekedTag = tagBuffer;
	return tag;
}

bool _seekTag(struct GBARRContext* rr, struct VFile* vf, enum GBARRTag tag) {
	enum GBARRTag readTag;
	while ((readTag = _readTag(rr, vf)) != tag) {
		if (readTag == TAG_END) {
			if (rr->peekedTag == TAG_NEXT_TIME) {
				while (_readTag(rr, vf) != TAG_END) {
					if (!rr->nextTime) {
						return false;
					}
				}
				if (!rr->nextTime || !GBARRLoadStream(rr, rr->nextTime)) {
					return false;
				}
				vf = rr->movieStream;
			} else {
				return false;
			}
		}
	}
	return true;
}

bool _emitTag(struct GBARRContext* rr, struct VFile* vf, uint8_t tag) {
	UNUSED(rr);
	return vf->write(vf, &tag, 1) == 1;
}
