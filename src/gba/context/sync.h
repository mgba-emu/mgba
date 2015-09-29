/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_SYNC_H
#define GBA_SYNC_H

#include "util/common.h"

#include "util/threading.h"

struct GBASync {
	int videoFramePending;
	bool videoFrameWait;
	bool videoFrameOn;
	Mutex videoFrameMutex;
	Condition videoFrameAvailableCond;
	Condition videoFrameRequiredCond;

	bool audioWait;
	Condition audioRequiredCond;
	Mutex audioBufferMutex;
};

void GBASyncPostFrame(struct GBASync* sync);
void GBASyncForceFrame(struct GBASync* sync);
bool GBASyncWaitFrameStart(struct GBASync* sync);
void GBASyncWaitFrameEnd(struct GBASync* sync);
void GBASyncSetVideoSync(struct GBASync* sync, bool wait);

void GBASyncProduceAudio(struct GBASync* sync, bool wait);
void GBASyncLockAudio(struct GBASync* sync);
void GBASyncUnlockAudio(struct GBASync* sync);
void GBASyncConsumeAudio(struct GBASync* sync);

#endif
