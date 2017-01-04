/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/sync.h>

static void _changeVideoSync(struct mCoreSync* sync, bool frameOn) {
	// Make sure the video thread can process events while the GBA thread is paused
	MutexLock(&sync->videoFrameMutex);
	if (frameOn != sync->videoFrameOn) {
		sync->videoFrameOn = frameOn;
		ConditionWake(&sync->videoFrameAvailableCond);
	}
	MutexUnlock(&sync->videoFrameMutex);
}

void mCoreSyncPostFrame(struct mCoreSync* sync) {
	if (!sync) {
		return;
	}

	MutexLock(&sync->videoFrameMutex);
	++sync->videoFramePending;
	do {
		ConditionWake(&sync->videoFrameAvailableCond);
		if (sync->videoFrameWait) {
			ConditionWait(&sync->videoFrameRequiredCond, &sync->videoFrameMutex);
		}
	} while (sync->videoFrameWait && sync->videoFramePending);
	MutexUnlock(&sync->videoFrameMutex);
}

void mCoreSyncForceFrame(struct mCoreSync* sync) {
	if (!sync) {
		return;
	}

	MutexLock(&sync->videoFrameMutex);
	ConditionWake(&sync->videoFrameAvailableCond);
	MutexUnlock(&sync->videoFrameMutex);
}

bool mCoreSyncWaitFrameStart(struct mCoreSync* sync) {
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
	return true;
}

void mCoreSyncWaitFrameEnd(struct mCoreSync* sync) {
	if (!sync) {
		return;
	}

	MutexUnlock(&sync->videoFrameMutex);
}

void mCoreSyncSetVideoSync(struct mCoreSync* sync, bool wait) {
	if (!sync) {
		return;
	}

	_changeVideoSync(sync, wait);
}

void mCoreSyncProduceAudio(struct mCoreSync* sync, bool wait) {
	if (!sync) {
		return;
	}

	if (sync->audioWait && wait) {
		// TODO loop properly in event of spurious wakeups
		ConditionWait(&sync->audioRequiredCond, &sync->audioBufferMutex);
	}
	MutexUnlock(&sync->audioBufferMutex);
}

void mCoreSyncLockAudio(struct mCoreSync* sync) {
	if (!sync) {
		return;
	}

	MutexLock(&sync->audioBufferMutex);
}

void mCoreSyncUnlockAudio(struct mCoreSync* sync) {
	if (!sync) {
		return;
	}

	MutexUnlock(&sync->audioBufferMutex);
}

void mCoreSyncConsumeAudio(struct mCoreSync* sync) {
	if (!sync) {
		return;
	}

	ConditionWake(&sync->audioRequiredCond);
	MutexUnlock(&sync->audioBufferMutex);
}
