/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_RR_H
#define GBA_RR_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>
#include <mgba/internal/gba/serialize.h>

struct VFile;

mLOG_DECLARE_CATEGORY(GBA_RR);

enum GBARRInitFrom {
	INIT_EX_NIHILO = 0,
	INIT_FROM_SAVEGAME = 1,
	INIT_FROM_SAVESTATE = 2,
	INIT_FROM_BOTH = 3,
};

struct GBARRContext {
	void (*destroy)(struct GBARRContext*);

	bool (*startPlaying)(struct GBARRContext*, bool autorecord);
	void (*stopPlaying)(struct GBARRContext*);
	bool (*startRecording)(struct GBARRContext*);
	void (*stopRecording)(struct GBARRContext*);

	bool (*isPlaying)(const struct GBARRContext*);
	bool (*isRecording)(const struct GBARRContext*);

	void (*nextFrame)(struct GBARRContext*);
	void (*logInput)(struct GBARRContext*, uint16_t input);
	uint16_t (*queryInput)(struct GBARRContext*);
	bool (*queryReset)(struct GBARRContext*);

	void (*stateSaved)(struct GBARRContext*, struct GBASerializedState*);
	void (*stateLoaded)(struct GBARRContext*, const struct GBASerializedState*);

	struct VFile* (*openSavedata)(struct GBARRContext* mgm, int flags);
	struct VFile* (*openSavestate)(struct GBARRContext* mgm, int flags);

	uint32_t frames;
	uint32_t lagFrames;
	enum GBARRInitFrom initFrom;

	uint32_t rrCount;

	struct VFile* savedata;
};

void GBARRDestroy(struct GBARRContext*);

void GBARRInitRecord(struct GBA*);
void GBARRInitPlay(struct GBA*);

CXX_GUARD_END

#endif
