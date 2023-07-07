/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/sio/lockstep.h>

#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/io.h>

#define LOCKSTEP_INCREMENT 512

static bool GBSIOLockstepNodeInit(struct GBSIODriver* driver);
static void GBSIOLockstepNodeDeinit(struct GBSIODriver* driver);
static void GBSIOLockstepNodeWriteSB(struct GBSIODriver* driver, uint8_t value);
static uint8_t GBSIOLockstepNodeWriteSC(struct GBSIODriver* driver, uint8_t value);
static void _GBSIOLockstepNodeProcessEvents(struct mTiming* timing, void* driver, uint32_t cyclesLate);

void GBSIOLockstepInit(struct GBSIOLockstep* lockstep) {
	lockstep->players[0] = NULL;
	lockstep->players[1] = NULL;
	lockstep->pendingSB[0] = 0xFF;
	lockstep->pendingSB[1] = 0xFF;
	lockstep->masterClaimed = false;
}

void GBSIOLockstepNodeCreate(struct GBSIOLockstepNode* node) {
	node->d.init = GBSIOLockstepNodeInit;
	node->d.deinit = GBSIOLockstepNodeDeinit;
	node->d.writeSB = GBSIOLockstepNodeWriteSB;
	node->d.writeSC = GBSIOLockstepNodeWriteSC;
}

bool GBSIOLockstepAttachNode(struct GBSIOLockstep* lockstep, struct GBSIOLockstepNode* node) {
	if (lockstep->d.attached == MAX_GBS) {
		return false;
	}
	lockstep->players[lockstep->d.attached] = node;
	node->p = lockstep;
	node->id = lockstep->d.attached;
	++lockstep->d.attached;
	return true;
}

void GBSIOLockstepDetachNode(struct GBSIOLockstep* lockstep, struct GBSIOLockstepNode* node) {
	if (lockstep->d.attached == 0) {
		return;
	}
	int i;
	for (i = 0; i < lockstep->d.attached; ++i) {
		if (lockstep->players[i] != node) {
			continue;
		}
		for (++i; i < lockstep->d.attached; ++i) {
			lockstep->players[i - 1] = lockstep->players[i];
			lockstep->players[i - 1]->id = i - 1;
		}
		--lockstep->d.attached;
		break;
	}
}

bool GBSIOLockstepNodeInit(struct GBSIODriver* driver) {
	struct GBSIOLockstepNode* node = (struct GBSIOLockstepNode*) driver;
	mLOG(GB_SIO, DEBUG, "Lockstep %i: Node init", node->id);
	node->event.context = node;
	node->event.name = "GB SIO Lockstep";
	node->event.callback = _GBSIOLockstepNodeProcessEvents;
	node->event.priority = 0x80;

	node->nextEvent = 0;
	node->eventDiff = 0;
	mTimingSchedule(&driver->p->p->timing, &node->event, 0);
#ifndef NDEBUG
	node->phase = node->p->d.transferActive;
	node->transferId = node->p->d.transferId;
#endif
	return true;
}

void GBSIOLockstepNodeDeinit(struct GBSIODriver* driver) {
	struct GBSIOLockstepNode* node = (struct GBSIOLockstepNode*) driver;
	node->p->d.unload(&node->p->d, node->id);
	mTimingDeschedule(&driver->p->p->timing, &node->event);
}

static void _finishTransfer(struct GBSIOLockstepNode* node) {
	if (node->transferFinished) {
		return;
	}
	struct GBSIO* sio = node->d.p;
	sio->pendingSB = node->p->pendingSB[!node->id];
	if (GBRegisterSCIsEnable(sio->p->memory.io[GB_REG_SC])) {
		sio->remainingBits = 8;
		mTimingDeschedule(&sio->p->timing, &sio->event);
		mTimingSchedule(&sio->p->timing, &sio->event, 0);
	}
	node->transferFinished = true;
#ifndef NDEBUG
	++node->transferId;
#endif
}


static int32_t _masterUpdate(struct GBSIOLockstepNode* node) {
	enum mLockstepPhase transferActive;
	ATOMIC_LOAD(transferActive, node->p->d.transferActive);

	bool needsToWait = false;
	int i;
	switch (transferActive) {
	case TRANSFER_IDLE:
		// If the master hasn't initiated a transfer, it can keep going.
		node->nextEvent += LOCKSTEP_INCREMENT;
		break;
	case TRANSFER_STARTING:
		// Start the transfer, but wait for the other GBs to catch up
		node->transferFinished = false;
		needsToWait = true;
		ATOMIC_STORE(node->p->d.transferActive, TRANSFER_STARTED);
		node->nextEvent += 4;
		break;
	case TRANSFER_STARTED:
		// All the other GBs have caught up and are sleeping, we can all continue now
		node->nextEvent += 4;
		ATOMIC_STORE(node->p->d.transferActive, TRANSFER_FINISHING);
		break;
	case TRANSFER_FINISHING:
		// Finish the transfer
		// We need to make sure the other GBs catch up so they don't get behind
		node->nextEvent += node->d.p->period * (2 - node->d.p->p->doubleSpeed) - 8; // Split the cycles to avoid waiting too long
#ifndef NDEBUG
		ATOMIC_ADD(node->p->d.transferId, 1);
#endif
		needsToWait = true;
		ATOMIC_STORE(node->p->d.transferActive, TRANSFER_FINISHED);
		break;
	case TRANSFER_FINISHED:
		// Everything's settled. We're done.
		_finishTransfer(node);
		ATOMIC_STORE(node->p->masterClaimed, false);
		node->nextEvent += LOCKSTEP_INCREMENT;
		ATOMIC_STORE(node->p->d.transferActive, TRANSFER_IDLE);
		break;
	}
	int mask = 0;
	for (i = 1; i < node->p->d.attached; ++i) {
		mask |= 1 << i;
	}
	if (mask) {
		if (needsToWait) {
			if (!node->p->d.wait(&node->p->d, mask)) {
				abort();
			}
		} else {
			node->p->d.signal(&node->p->d, mask);
		}
	}
	// Tell the other GBs they can continue up to where we were
	node->p->d.addCycles(&node->p->d, 0, node->eventDiff);
#ifndef NDEBUG
	node->phase = node->p->d.transferActive;
#endif
	if (needsToWait) {
		return 0;
	}
	return node->nextEvent;
}

static uint32_t _slaveUpdate(struct GBSIOLockstepNode* node) {
	enum mLockstepPhase transferActive;
	int id;

	ATOMIC_LOAD(transferActive, node->p->d.transferActive);
	ATOMIC_LOAD(id, node->id);

	bool signal = false;
	switch (transferActive) {
	case TRANSFER_IDLE:
		node->p->d.addCycles(&node->p->d, id, LOCKSTEP_INCREMENT);
		break;
	case TRANSFER_STARTING:
	case TRANSFER_FINISHING:
		break;
	case TRANSFER_STARTED:
		if (node->p->d.unusedCycles(&node->p->d, id) > node->eventDiff) {
			break;
		}
		node->transferFinished = false;
		signal = true;
		break;
	case TRANSFER_FINISHED:
		if (node->p->d.unusedCycles(&node->p->d, id) > node->eventDiff) {
			break;
		}
		_finishTransfer(node);
		signal = true;
		break;
	}
#ifndef NDEBUG
	node->phase = node->p->d.transferActive;
#endif
	if (signal) {
		node->p->d.signal(&node->p->d, 1 << id);
	}
	return 0;
}

static void _GBSIOLockstepNodeProcessEvents(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	struct GBSIOLockstepNode* node = user;
	mLockstepLock(&node->p->d);
	if (node->p->d.attached < 2) {
		mTimingSchedule(timing, &node->event, (GBSIOCyclesPerTransfer[0] >> 1) * (2 - node->d.p->p->doubleSpeed) - cyclesLate);
		mLockstepUnlock(&node->p->d);
		return;
	}
	int32_t cycles = 0;
	node->nextEvent -= cyclesLate;
	if (node->nextEvent <= 0) {
		int id;
		ATOMIC_LOAD(id, node->id);
		if (!id) {
			cycles = _masterUpdate(node);
		} else {
			cycles = _slaveUpdate(node);
			cycles += node->p->d.useCycles(&node->p->d, id, node->eventDiff);
		}
		node->eventDiff = 0;
	} else {
		cycles = node->nextEvent;
	}
	mLockstepUnlock(&node->p->d);

	if (cycles > 0) {
		node->nextEvent = 0;
		node->eventDiff += cycles;
		mTimingDeschedule(timing, &node->event);
		mTimingSchedule(timing, &node->event, cycles);
	} else {
		node->d.p->p->earlyExit = true;
		mTimingSchedule(timing, &node->event, cyclesLate + 1);
	}
}

static void GBSIOLockstepNodeWriteSB(struct GBSIODriver* driver, uint8_t value) {
	struct GBSIOLockstepNode* node = (struct GBSIOLockstepNode*) driver;
	int id;
	ATOMIC_LOAD(id, node->id);
	node->p->pendingSB[id] = value;
}

static uint8_t GBSIOLockstepNodeWriteSC(struct GBSIODriver* driver, uint8_t value) {
	struct GBSIOLockstepNode* node = (struct GBSIOLockstepNode*) driver;
	int attached;
	ATOMIC_LOAD(attached, node->p->d.attached);

	if ((value & 0x81) == 0x81 && attached > 1) {
		mLockstepLock(&node->p->d);
		bool claimed = false;
		if (ATOMIC_CMPXCHG(node->p->masterClaimed, claimed, true)) {
			int id;
			ATOMIC_LOAD(id, node->id);
			if (id != 0) {
				unsigned sb;
				node->p->players[0]->id = 1;
				node->id = 0;
				node->p->players[1] = node->p->players[0];
				node->p->players[0] = node;
				sb = node->p->pendingSB[0];
				node->p->pendingSB[0] = node->p->pendingSB[1];
				node->p->pendingSB[1] = sb;
			}
			ATOMIC_STORE(node->p->d.transferActive, TRANSFER_STARTING);
			ATOMIC_STORE(node->p->d.transferCycles, GBSIOCyclesPerTransfer[(value >> 1) & 1]);
			mTimingDeschedule(&driver->p->p->timing, &driver->p->event);
			mTimingDeschedule(&driver->p->p->timing, &node->event);
			mTimingSchedule(&driver->p->p->timing, &node->event, 0);
		} else {
			mLOG(GB_SIO, DEBUG, "GBSIOLockstepNodeWriteSC() failed to write to masterClaimed\n");
		}
		mLockstepUnlock(&node->p->d);
	}
	return value;
}
