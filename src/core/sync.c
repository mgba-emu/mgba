/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/sync.h>

#include <mgba/core/blip_buf.h>

static void _changeVideoSync(struct mCoreSync* sync, bool wait) {
	// Make sure the video thread can process events while the GBA thread is paused
	MutexLock(&sync->videoFrameMutex);
	if (wait != sync->videoFrameWait) {
		sync->videoFrameWait = wait;
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
	if (!sync->videoFrameWait && !sync->videoFramePending) {
		return false;
	}
	if (sync->videoFrameWait) {
		ConditionWake(&sync->videoFrameRequiredCond);
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

	ConditionWake(&sync->videoFrameRequiredCond);
	MutexUnlock(&sync->videoFrameMutex);
}

void mCoreSyncSetVideoSync(struct mCoreSync* sync, bool wait) {
	if (!sync) {
		return;
	}

	_changeVideoSync(sync, wait);
}

bool mCoreSyncProduceAudio(struct mCoreSync* sync, const struct blip_t* buf, size_t samples) {
	if (!sync) {
		return true;
	}

	size_t produced = blip_samples_avail(buf);
	size_t producedNew = produced;
	while (sync->audioWait && producedNew >= samples) {
		ConditionWait(&sync->audioRequiredCond, &sync->audioBufferMutex);
		produced = producedNew;
		producedNew = blip_samples_avail(buf);
	}
	MutexUnlock(&sync->audioBufferMutex);
	return producedNew != produced;
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
