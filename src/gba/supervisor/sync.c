/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "thread.h"

static void _changeVideoSync(struct GBASync* sync, bool frameOn) {
	// Make sure the video thread can process events while the GBA thread is paused
	MutexLock(&sync->videoFrameMutex);
	if (frameOn != sync->videoFrameOn) {
		sync->videoFrameOn = frameOn;
		ConditionWake(&sync->videoFrameAvailableCond);
	}
	MutexUnlock(&sync->videoFrameMutex);
}

void GBASyncPostFrame(struct GBASync* sync) {
	if (!sync) {
		return;
	}

	MutexLock(&sync->videoFrameMutex);
	++sync->videoFramePending;
	--sync->videoFrameSkip;
	if (sync->videoFrameSkip < 0) {
		do {
			ConditionWake(&sync->videoFrameAvailableCond);
			if (sync->videoFrameWait) {
				ConditionWait(&sync->videoFrameRequiredCond, &sync->videoFrameMutex);
			}
		} while (sync->videoFrameWait && sync->videoFramePending);
	}
	MutexUnlock(&sync->videoFrameMutex);
}

void GBASyncForceFrame(struct GBASync* sync) {
	if (!sync) {
		return;
	}

	MutexLock(&sync->videoFrameMutex);
	ConditionWake(&sync->videoFrameAvailableCond);
	MutexUnlock(&sync->videoFrameMutex);
}

bool GBASyncWaitFrameStart(struct GBASync* sync, int frameskip) {
	if (!sync) {
		return true;
	}

	MutexLock(&sync->videoFrameMutex);
	ConditionWake(&sync->videoFrameRequiredCond);
	if (!sync->videoFrameOn && !sync->videoFramePending) {
		return false;
	}
	if (sync->videoFrameOn) {
		if (ConditionWaitTimed(&sync->videoFrameAvailableCond, &sync->videoFrameMutex, 50)) {
			return false;
		}
	}
	sync->videoFramePending = 0;
	sync->videoFrameSkip = frameskip;
	return true;
}

void GBASyncWaitFrameEnd(struct GBASync* sync) {
	if (!sync) {
		return;
	}

	MutexUnlock(&sync->videoFrameMutex);
}

bool GBASyncDrawingFrame(struct GBASync* sync) {
	if (!sync) {
		return true;
	}

	return sync->videoFrameSkip <= 0;
}

void GBASyncSetVideoSync(struct GBASync* sync, bool wait) {
	if (!sync) {
		return;
	}

	_changeVideoSync(sync, wait);
}

void GBASyncProduceAudio(struct GBASync* sync, bool wait) {
	if (!sync) {
		return;
	}

	if (sync->audioWait && wait) {
		// TODO loop properly in event of spurious wakeups
		ConditionWait(&sync->audioRequiredCond, &sync->audioBufferMutex);
	}
	MutexUnlock(&sync->audioBufferMutex);
}

void GBASyncLockAudio(struct GBASync* sync) {
	if (!sync) {
		return;
	}

	MutexLock(&sync->audioBufferMutex);
}

void GBASyncUnlockAudio(struct GBASync* sync) {
	if (!sync) {
		return;
	}

	MutexUnlock(&sync->audioBufferMutex);
}

void GBASyncConsumeAudio(struct GBASync* sync) {
	if (!sync) {
		return;
	}

	ConditionWake(&sync->audioRequiredCond);
	MutexUnlock(&sync->audioBufferMutex);
}
