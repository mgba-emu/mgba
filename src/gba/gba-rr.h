#ifndef GBA_RR_H
#define GBA_RR_H

#include "common.h"

struct GBA;
struct VDir;
struct VFile;

struct GBARRContext {
	// Playback state
	bool isPlaying;
	size_t inputId;

	// Recording state
	bool isRecording;
	bool inputThisFrame;

	// Metadata
	uint32_t frames;
	uint32_t lagFrames;

	// Streaming state
	struct VDir* streamDir;
	struct VFile* inputsStream;
};

void GBARRContextCreate(struct GBA*);
void GBARRContextDestroy(struct GBA*);

bool GBARRSetStream(struct GBARRContext*, struct VDir*);

bool GBARRStartPlaying(struct GBARRContext*);
void GBARRStopPlaying(struct GBARRContext*);
bool GBARRStartRecording(struct GBARRContext*);
void GBARRStopRecording(struct GBARRContext*);

bool GBARRIsPlaying(struct GBARRContext*);
bool GBARRIsRecording(struct GBARRContext*);

void GBARRNextFrame(struct GBARRContext*);
void GBARRLogInput(struct GBARRContext*, uint16_t input);
uint16_t GBARRQueryInput(struct GBARRContext*);

#endif
