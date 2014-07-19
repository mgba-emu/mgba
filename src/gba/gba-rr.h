#ifndef GBA_RR_H
#define GBA_RR_H

#include "common.h"

struct GBA;
struct VFile;

struct GBARRContext {
	struct GBARRBlock* rootBlock;
	struct GBARRBlock* currentBlock;

	// Playback state
	struct GBARRBlock* playbackBlock;
	size_t inputId;

	// Recording state
	bool isRecording;
	bool inputThisFrame;
	uint32_t frames;
	uint32_t lagFrames;
};

void GBARRContextCreate(struct GBA*);
void GBARRContextDestroy(struct GBA*);

bool GBARRStartPlaying(struct GBA*);
void GBARRStopPlaying(struct GBA*);
bool GBARRStartRecording(struct GBA*);
void GBARRStopRecording(struct GBA*);

bool GBARRIsPlaying(struct GBA*);
bool GBARRIsRecording(struct GBA*);

void GBARRNextFrame(struct GBA*);
void GBARRLogInput(struct GBA*, uint16_t input);
uint16_t GBARRQueryInput(struct GBA*);

#endif
