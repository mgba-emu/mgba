/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_RR_H
#define GBA_RR_H

#include "util/common.h"

struct GBA;
struct VDir;
struct VFile;

enum GBARRInitFrom {
	INIT_EX_NIHILO = 0,
	INIT_FROM_SAVEGAME = 1,
	INIT_FROM_SAVESTATE = 2,
	INIT_FROM_BOTH = 3,
};

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
	TAG_INIT = 0x24,
	TAG_INIT_EX_NIHILO = 0x24 | INIT_EX_NIHILO,
	TAG_INIT_FROM_SAVEGAME = 0x24 | INIT_FROM_SAVEGAME,
	TAG_INIT_FROM_SAVESTATE = 0x24 | INIT_FROM_SAVESTATE,
	TAG_INIT_FROM_BOTH = 0x24 | INIT_FROM_BOTH,

	// User metadata tags
	TAG_AUTHOR = 0x30,
	TAG_COMMENT = 0x31,

	TAG_EOF = INT_MAX
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
	off_t maxStreamIdOffset;

	enum GBARRInitFrom initFrom;
	off_t initFromOffset;

	uint32_t rrCount;
	off_t rrCountOffset;

	struct VFile* savedata;

	// Streaming state
	struct VDir* streamDir;
	struct VFile* metadataFile;
	struct VFile* movieStream;
	uint16_t currentInput;
	enum GBARRTag peekedTag;
	uint32_t nextTime;
	uint32_t previously;
};

void GBARRContextCreate(struct GBA*);
void GBARRContextDestroy(struct GBA*);
void GBARRSaveState(struct GBA*);
void GBARRLoadState(struct GBA*);

bool GBARRInitStream(struct GBARRContext*, struct VDir*);
bool GBARRReinitStream(struct GBARRContext*, enum GBARRInitFrom);
bool GBARRLoadStream(struct GBARRContext*, uint32_t streamId);
bool GBARRIncrementStream(struct GBARRContext*, bool recursive);
bool GBARRFinishSegment(struct GBARRContext*);
bool GBARRSkipSegment(struct GBARRContext*);
bool GBARRMarkRerecord(struct GBARRContext*);

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
