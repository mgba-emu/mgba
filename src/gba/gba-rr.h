#ifndef GBA_RR_H
#define GBA_RR_H

#include "common.h"

struct GBA;
struct VDir;
struct VFile;

enum GBARRTag {
	// Playback tags
	TAG_INVALID = 0x00,
	TAG_INPUT = 0x01,
	TAG_FRAME = 0x02,
	TAG_LAG = 0x03,

	// Stream chunking tags
	TAG_BEGIN = 0x10,
	TAG_END = 0x11,
	TAG_PREVIOUSLY = 0x12,
	TAG_NEXT_TIME = 0x13,
	TAG_MAX_STREAM = 0x14,

	// Recording information tags
	TAG_FRAME_COUNT = 0x20,
	TAG_LAG_COUNT = 0x21,
	TAG_RR_COUNT = 0x22,
	TAG_INIT_TYPE = 0x23,

	// User metadata tags
	TAG_AUTHOR = 0x30,
	TAG_COMMENT = 0x31
};

struct GBARRContext {
	// Playback state
	bool isPlaying;
	bool autorecord;

	// Recording state
	bool isRecording;
	bool inputThisFrame;

	// Metadata
	uint32_t frames;
	uint32_t lagFrames;
	uint32_t streamId;
	uint32_t maxStreamId;

	// Streaming state
	struct VDir* streamDir;
	struct VFile* movieStream;
	uint16_t currentInput;
	enum GBARRTag peekedTag;
	uint32_t nextTime;
	uint32_t previously;
};

void GBARRContextCreate(struct GBA*);
void GBARRContextDestroy(struct GBA*);

bool GBARRSetStream(struct GBARRContext*, struct VDir*);
bool GBARRLoadStream(struct GBARRContext*, uint32_t streamId);
bool GBARRIncrementStream(struct GBARRContext*);

bool GBARRStartPlaying(struct GBARRContext*, bool autorecord);
void GBARRStopPlaying(struct GBARRContext*);
bool GBARRStartRecording(struct GBARRContext*);
void GBARRStopRecording(struct GBARRContext*);

bool GBARRIsPlaying(struct GBARRContext*);
bool GBARRIsRecording(struct GBARRContext*);

void GBARRNextFrame(struct GBARRContext*);
void GBARRLogInput(struct GBARRContext*, uint16_t input);
uint16_t GBARRQueryInput(struct GBARRContext*);

#endif
