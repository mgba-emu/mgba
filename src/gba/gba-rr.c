#include "gba-rr.h"

#include "gba.h"
#include "util/vfs.h"

#define FILE_INPUTS "input.log"

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

	free(gba->rr);
	gba->rr = 0;
}

bool GBARRSetStream(struct GBARRContext* rr, struct VDir* stream) {
	if (rr->inputsStream && !rr->inputsStream->close(rr->inputsStream)) {
		return false;
	}
	rr->streamDir = stream;
	rr->inputsStream = stream->openFile(stream, FILE_INPUTS, O_CREAT | O_RDWR);
	return !!rr->inputsStream;
}

bool GBARRStartPlaying(struct GBARRContext* rr) {
	if (GBARRIsRecording(rr) || GBARRIsPlaying(rr)) {
		return false;
	}

	rr->isPlaying = true;
	rr->inputId = 0;
	return rr->inputsStream->seek(rr->inputsStream, 0, SEEK_SET) == 0;
}

void GBARRStopPlaying(struct GBARRContext* rr) {
	rr->isPlaying = 0;
}

bool GBARRStartRecording(struct GBARRContext* rr) {
	if (GBARRIsRecording(rr) || GBARRIsPlaying(rr)) {
		return false;
	}

	rr->isRecording = true;
	return true;
}

void GBARRStopRecording(struct GBARRContext* rr) {
	rr->isRecording = false;
}

bool GBARRIsPlaying(struct GBARRContext* rr) {
	return rr && rr->isPlaying;
}

bool GBARRIsRecording(struct GBARRContext* rr) {
	return rr && rr->isRecording;
}

void GBARRNextFrame(struct GBARRContext* rr) {
	if (!GBARRIsRecording(rr)) {
		return;
	}

	++rr->frames;
	if (!rr->inputThisFrame) {
		++rr->lagFrames;
	}

	rr->inputThisFrame = false;
}

void GBARRLogInput(struct GBARRContext* rr, uint16_t keys) {
	if (!GBARRIsRecording(rr)) {
		return;
	}

	rr->inputsStream->write(rr->inputsStream, &keys, sizeof(keys));
	rr->inputThisFrame = true;
}

uint16_t GBARRQueryInput(struct GBARRContext* rr) {
	if (!GBARRIsPlaying(rr)) {
		return 0;
	}

	uint16_t keys;
	rr->inputsStream->read(rr->inputsStream, &keys, sizeof(keys));
	return keys;
}
