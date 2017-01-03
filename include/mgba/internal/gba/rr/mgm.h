/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef RR_MGM_H
#define RR_MGM_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/internal/gba/rr/rr.h>

struct GBA;
struct VDir;
struct VFile;

enum GBAMGMTag {
	// Playback tags
	TAG_INVALID = 0x00,
	TAG_INPUT = 0x01,
	TAG_FRAME = 0x02,
	TAG_LAG = 0x03,
	TAG_RESET = 0x04,

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

struct GBAMGMContext {
	struct GBARRContext d;

	// Playback state
	bool isPlaying;
	bool autorecord;

	// Recording state
	bool isRecording;
	bool inputThisFrame;

	// Metadata
	uint32_t streamId;

	uint32_t maxStreamId;
	off_t maxStreamIdOffset;
	off_t initFromOffset;
	off_t rrCountOffset;

	// Streaming state
	struct VDir* streamDir;
	struct VFile* metadataFile;
	struct VFile* movieStream;
	uint16_t currentInput;
	enum GBAMGMTag peekedTag;
	uint32_t nextTime;
	uint32_t previously;
};

void GBAMGMContextCreate(struct GBAMGMContext*);

bool GBAMGMSetStream(struct GBAMGMContext* mgm, struct VDir* stream);
bool GBAMGMCreateStream(struct GBAMGMContext* mgm, enum GBARRInitFrom initFrom);

CXX_GUARD_END

#endif
