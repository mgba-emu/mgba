/* Copyright (c) 2013-2024 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/sio/lockstep.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>

#define LOCKSTEP_INCREMENT 2000
#define LOCKSTEP_TRANSFER 512
#define QUEUE_SIZE 16

static bool GBASIOLockstepNodeInit(struct GBASIODriver* driver);
static void GBASIOLockstepNodeDeinit(struct GBASIODriver* driver);
static bool GBASIOLockstepNodeLoad(struct GBASIODriver* driver);
static bool GBASIOLockstepNodeUnload(struct GBASIODriver* driver);
static bool GBASIOLockstepNodeHandlesMode(struct GBASIODriver* driver, enum GBASIOMode mode);
static int GBASIOLockstepNodeConnectedDevices(struct GBASIODriver* driver);
static int GBASIOLockstepNodeDeviceId(struct GBASIODriver* driver);
static bool GBASIOLockstepNodeStart(struct GBASIODriver* driver);
static uint16_t GBASIOLockstepNodeMultiWriteSIOCNT(struct GBASIODriver* driver, uint16_t value);
static uint16_t GBASIOLockstepNodeNormalWriteSIOCNT(struct GBASIODriver* driver, uint16_t value);
static void _GBASIOLockstepNodeProcessEvents(struct mTiming* timing, void* driver, uint32_t cyclesLate);
static void _finishTransfer(struct GBASIOLockstepNode* node);

void GBASIOLockstepInit(struct GBASIOLockstep* lockstep) {
	lockstep->players[0] = 0;
	lockstep->players[1] = 0;
	lockstep->players[2] = 0;
	lockstep->players[3] = 0;
	lockstep->multiRecv[0] = 0xFFFF;
	lockstep->multiRecv[1] = 0xFFFF;
	lockstep->multiRecv[2] = 0xFFFF;
	lockstep->multiRecv[3] = 0xFFFF;
	lockstep->attachedMulti = 0;
	lockstep->attachedNormal = 0;
}

void GBASIOLockstepNodeCreate(struct GBASIOLockstepNode* node) {
	memset(&node->d, 0, sizeof(node->d));
	node->d.init = GBASIOLockstepNodeInit;
	node->d.deinit = GBASIOLockstepNodeDeinit;
	node->d.load = GBASIOLockstepNodeLoad;
	node->d.unload = GBASIOLockstepNodeUnload;
	node->d.handlesMode = GBASIOLockstepNodeHandlesMode;
	node->d.connectedDevices = GBASIOLockstepNodeConnectedDevices;
	node->d.deviceId = GBASIOLockstepNodeDeviceId;
	node->d.start = GBASIOLockstepNodeStart;
	node->d.writeSIOCNT = NULL;
}

bool GBASIOLockstepAttachNode(struct GBASIOLockstep* lockstep, struct GBASIOLockstepNode* node) {
	if (lockstep->d.attached == MAX_GBAS) {
		return false;
	}
	mLockstepLock(&lockstep->d);
	lockstep->players[lockstep->d.attached] = node;
	node->p = lockstep;
	node->id = lockstep->d.attached;
	node->normalSO = true;
	node->transferFinished = true;
	++lockstep->d.attached;
	mLockstepUnlock(&lockstep->d);
	return true;
}

void GBASIOLockstepDetachNode(struct GBASIOLockstep* lockstep, struct GBASIOLockstepNode* node) {
	if (lockstep->d.attached == 0) {
		return;
	}
	mLockstepLock(&lockstep->d);
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
		lockstep->players[lockstep->d.attached] = NULL;
		break;
	}
	mLockstepUnlock(&lockstep->d);
}

bool GBASIOLockstepNodeInit(struct GBASIODriver* driver) {
	struct GBASIOLockstepNode* node = (struct GBASIOLockstepNode*) driver;
	node->d.p->siocnt = GBASIOMultiplayerSetSlave(node->d.p->siocnt, node->id > 0);
	mLOG(GBA_SIO, DEBUG, "Lockstep %i: Node init", node->id);
	node->event.context = node;
	node->event.name = "GBA SIO Lockstep";
	node->event.callback = _GBASIOLockstepNodeProcessEvents;
	node->event.priority = 0x80;
	return true;
}

void GBASIOLockstepNodeDeinit(struct GBASIODriver* driver) {
	UNUSED(driver);
}

bool GBASIOLockstepNodeLoad(struct GBASIODriver* driver) {
	struct GBASIOLockstepNode* node = (struct GBASIOLockstepNode*) driver;
	node->nextEvent = 0;
	node->eventDiff = 0;
	mTimingSchedule(&driver->p->p->timing, &node->event, 0);

	mLockstepLock(&node->p->d);

	node->mode = driver->p->mode;

	switch (node->mode) {
	case GBA_SIO_MULTI:
		node->d.writeSIOCNT = GBASIOLockstepNodeMultiWriteSIOCNT;
		ATOMIC_ADD(node->p->attachedMulti, 1);
		node->d.p->siocnt = GBASIOMultiplayerSetReady(node->d.p->siocnt, node->p->attachedMulti == node->p->d.attached);
		if (node->id) {
			node->d.p->rcnt |= 4;
			int try;
			for (try = 0; try < 3; ++try) {
				uint16_t masterSiocnt;
				ATOMIC_LOAD(masterSiocnt, node->p->players[0]->d.p->siocnt);
				if (ATOMIC_CMPXCHG(node->p->players[0]->d.p->siocnt, masterSiocnt, GBASIOMultiplayerClearSlave(masterSiocnt))) {
					break;
				}
			}
		}
		break;
	case GBA_SIO_NORMAL_8:
	case GBA_SIO_NORMAL_32:
		if (ATOMIC_ADD(node->p->attachedNormal, 1) > node->id + 1 && node->id > 0) {
			node->d.p->siocnt = GBASIONormalSetSi(node->d.p->siocnt, GBASIONormalGetIdleSo(node->p->players[node->id - 1]->d.p->siocnt));
		} else {
			node->d.p->siocnt = GBASIONormalClearSi(node->d.p->siocnt);
		}
		node->d.writeSIOCNT = GBASIOLockstepNodeNormalWriteSIOCNT;
		break;
	default:
		break;
	}
#ifndef NDEBUG
	node->phase = node->p->d.transferActive;
	node->transferId = node->p->d.transferId;
#endif

	mLockstepUnlock(&node->p->d);

	return true;
}

bool GBASIOLockstepNodeUnload(struct GBASIODriver* driver) {
	struct GBASIOLockstepNode* node = (struct GBASIOLockstepNode*) driver;

	mLockstepLock(&node->p->d);

	node->mode = driver->p->mode;
	switch (node->mode) {
	case GBA_SIO_MULTI:
		ATOMIC_SUB(node->p->attachedMulti, 1);
		break;
	case GBA_SIO_NORMAL_8:
	case GBA_SIO_NORMAL_32:
		ATOMIC_SUB(node->p->attachedNormal, 1);
		break;
	default:
		break;
	}

	// Flush ongoing transfer
	if (mTimingIsScheduled(&driver->p->p->timing, &node->event)) {
		node->eventDiff -= node->event.when - mTimingCurrentTime(&driver->p->p->timing);
		mTimingDeschedule(&driver->p->p->timing, &node->event);
	}

	node->p->d.unload(&node->p->d, node->id);

	_finishTransfer(node);

	if (!node->id) {
		ATOMIC_STORE(node->p->d.transferActive, TRANSFER_IDLE);
	}

	// Invalidate SIO mode
	node->mode = GBA_SIO_GPIO;

	mLockstepUnlock(&node->p->d);

	return true;
}

static bool GBASIOLockstepNodeHandlesMode(struct GBASIODriver* driver, enum GBASIOMode mode) {
	UNUSED(driver);
	switch (mode) {
	case GBA_SIO_NORMAL_8:
	case GBA_SIO_NORMAL_32:
	case GBA_SIO_MULTI:
		return true;
	default:
		return false;
	}
}

static int GBASIOLockstepNodeConnectedDevices(struct GBASIODriver* driver) {
	struct GBASIOLockstepNode* node = (struct GBASIOLockstepNode*) driver;
	int attached = 1;

	switch (node->mode) {
	case GBA_SIO_NORMAL_8:
	case GBA_SIO_NORMAL_32:
		ATOMIC_LOAD(attached, node->p->attachedNormal);
		break;
	case GBA_SIO_MULTI:
		ATOMIC_LOAD(attached, node->p->attachedMulti);
		break;
	default:
		break;
	}
	return attached - 1;
}

static int GBASIOLockstepNodeDeviceId(struct GBASIODriver* driver) {
	struct GBASIOLockstepNode* node = (struct GBASIOLockstepNode*) driver;
	return node->id;
}

static bool GBASIOLockstepNodeStart(struct GBASIODriver* driver) {
	UNUSED(driver);
	return false;
}

static uint16_t GBASIOLockstepNodeMultiWriteSIOCNT(struct GBASIODriver* driver, uint16_t value) {
	struct GBASIOLockstepNode* node = (struct GBASIOLockstepNode*) driver;

	mLockstepLock(&node->p->d);

	mLOG(GBA_SIO, DEBUG, "Lockstep %i: SIOCNT <- %04X", node->id, value);

	enum mLockstepPhase transferActive;
	int attached;
	ATOMIC_LOAD(transferActive, node->p->d.transferActive);
	ATOMIC_LOAD(attached, node->p->d.attached);

	driver->p->siocnt = GBASIOMultiplayerSetSlave(driver->p->siocnt, node->id || attached < 2);

	if (value & 0x0080 && transferActive == TRANSFER_IDLE) {
		if (!node->id && attached > 1 && GBASIOMultiplayerIsReady(node->d.p->siocnt)) {
			mLOG(GBA_SIO, DEBUG, "Lockstep %i: Transfer initiated", node->id);
			ATOMIC_STORE(node->p->d.transferActive, TRANSFER_STARTING);
			ATOMIC_STORE(node->p->d.transferCycles, GBASIOTransferCycles(node->d.p->mode, node->d.p->siocnt, attached));

			if (mTimingIsScheduled(&driver->p->p->timing, &node->event)) {
				node->eventDiff -= node->event.when - mTimingCurrentTime(&driver->p->p->timing);
				mTimingDeschedule(&driver->p->p->timing, &node->event);
			}
			mTimingSchedule(&driver->p->p->timing, &node->event, 0);
		}
	}
	value &= 0xFF83;
	value |= driver->p->siocnt & 0x00FC;

	mLockstepUnlock(&node->p->d);

	return value;
}

static void _finishTransfer(struct GBASIOLockstepNode* node) {
	if (node->transferFinished) {
		return;
	}

	struct GBASIO* sio = node->d.p;
	switch (node->mode) {
	case GBA_SIO_MULTI:
		GBASIOMultiplayerFinishTransfer(sio, node->p->multiRecv, 0);
		break;
	case GBA_SIO_NORMAL_8:
		if (node->id) {
			sio->siocnt = GBASIONormalSetSi(sio->siocnt, GBASIONormalGetIdleSo(node->p->players[node->id - 1]->d.p->siocnt));
			GBASIONormal8FinishTransfer(sio,  node->p->normalRecv[node->id - 1], 0);
		} else {
			GBASIONormal8FinishTransfer(sio, 0xFF, 0);
		}
		break;
	case GBA_SIO_NORMAL_32:
		if (node->id) {
			sio->siocnt = GBASIONormalSetSi(sio->siocnt, GBASIONormalGetIdleSo(node->p->players[node->id - 1]->d.p->siocnt));
			GBASIONormal32FinishTransfer(sio,  node->p->normalRecv[node->id - 1], 0);
		} else {
			GBASIONormal32FinishTransfer(sio, 0xFFFFFFFF, 0);
		}
		break;
	default:
		break;
	}
	node->transferFinished = true;
#ifndef NDEBUG
	++node->transferId;
#endif
}

static int32_t _masterUpdate(struct GBASIOLockstepNode* node) {
	bool needsToWait = false;
	int i;

	enum mLockstepPhase transferActive;
	int attachedMulti, attached;

	ATOMIC_LOAD(transferActive, node->p->d.transferActive);
	ATOMIC_LOAD(attachedMulti, node->p->attachedMulti);
	ATOMIC_LOAD(attached, node->p->d.attached);

	switch (transferActive) {
	case TRANSFER_IDLE:
		// If the master hasn't initiated a transfer, it can keep going.
		node->nextEvent += LOCKSTEP_INCREMENT;
		if (node->mode == GBA_SIO_MULTI) {
			node->d.p->siocnt = GBASIOMultiplayerSetReady(node->d.p->siocnt, attachedMulti == attached);
		}
		break;
	case TRANSFER_STARTING:
		// Start the transfer, but wait for the other GBAs to catch up
		node->transferFinished = false;
		switch (node->mode) {
		case GBA_SIO_MULTI:
			node->p->multiRecv[0] = node->d.p->p->memory.io[GBA_REG(SIOMLT_SEND)];
			node->p->multiRecv[1] = 0xFFFF;
			node->p->multiRecv[2] = 0xFFFF;
			node->p->multiRecv[3] = 0xFFFF;
			break;
		case GBA_SIO_NORMAL_8:
			node->p->multiRecv[0] = 0xFFFF;
			node->p->normalRecv[0] = node->d.p->p->memory.io[GBA_REG(SIODATA8)] & 0xFF;
			break;
		case GBA_SIO_NORMAL_32:
			node->p->multiRecv[0] = 0xFFFF;
			mLOG(GBA_SIO, DEBUG, "Lockstep %i: SIODATA32_LO <- %04X", node->id, node->d.p->p->memory.io[GBA_REG(SIODATA32_LO)]);
			mLOG(GBA_SIO, DEBUG, "Lockstep %i: SIODATA32_HI <- %04X", node->id, node->d.p->p->memory.io[GBA_REG(SIODATA32_HI)]);
			node->p->normalRecv[0] = node->d.p->p->memory.io[GBA_REG(SIODATA32_LO)];
			node->p->normalRecv[0] |= node->d.p->p->memory.io[GBA_REG(SIODATA32_HI)] << 16;
			break;
		default:
			node->p->multiRecv[0] = 0xFFFF;
			break;
		}
		needsToWait = true;
		ATOMIC_STORE(node->p->d.transferActive, TRANSFER_STARTED);
		node->nextEvent += LOCKSTEP_TRANSFER;
		break;
	case TRANSFER_STARTED:
		// All the other GBAs have caught up and are sleeping, we can all continue now
		node->nextEvent += LOCKSTEP_TRANSFER;
		ATOMIC_STORE(node->p->d.transferActive, TRANSFER_FINISHING);
		break;
	case TRANSFER_FINISHING:
		// Finish the transfer
		// We need to make sure the other GBAs catch up so they don't get behind
		node->nextEvent += node->p->d.transferCycles - 1024; // Split the cycles to avoid waiting too long
#ifndef NDEBUG
		ATOMIC_ADD(node->p->d.transferId, 1);
#endif
		needsToWait = true;
		ATOMIC_STORE(node->p->d.transferActive, TRANSFER_FINISHED);
		break;
	case TRANSFER_FINISHED:
		// Everything's settled. We're done.
		_finishTransfer(node);
		node->nextEvent += LOCKSTEP_INCREMENT;
		ATOMIC_STORE(node->p->d.transferActive, TRANSFER_IDLE);
		break;
	}
	int mask = 0;
	for (i = 1; i < node->p->d.attached; ++i) {
		if (node->p->players[i]->mode == node->mode) {
			mask |= 1 << i;
		}
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
	// Tell the other GBAs they can continue up to where we were
	node->p->d.addCycles(&node->p->d, 0, node->eventDiff);
#ifndef NDEBUG
	node->phase = node->p->d.transferActive;
#endif

	if (needsToWait) {
		return 0;
	}
	return node->nextEvent;
}

static uint32_t _slaveUpdate(struct GBASIOLockstepNode* node) {
	enum mLockstepPhase transferActive;
	int attached;
	int attachedMode;

	ATOMIC_LOAD(transferActive, node->p->d.transferActive);
	ATOMIC_LOAD(attached, node->p->d.attached);

	if (node->mode == GBA_SIO_MULTI) {
		ATOMIC_LOAD(attachedMode, node->p->attachedMulti);
		node->d.p->siocnt = GBASIOMultiplayerSetReady(node->d.p->siocnt, attachedMode == attached);
	} else {
		ATOMIC_LOAD(attachedMode, node->p->attachedNormal);
	}
	bool signal = false;
	switch (transferActive) {
	case TRANSFER_IDLE:
		if (attachedMode != attached) {
			node->p->d.addCycles(&node->p->d, node->id, LOCKSTEP_INCREMENT);
		}
		break;
	case TRANSFER_STARTING:
	case TRANSFER_FINISHING:
		break;
	case TRANSFER_STARTED:
		if (node->p->d.unusedCycles(&node->p->d, node->id) > node->eventDiff) {
			break;
		}
		node->transferFinished = false;
		switch (node->mode) {
		case GBA_SIO_MULTI:
			node->d.p->rcnt &= ~1;
			node->p->multiRecv[node->id] = node->d.p->p->memory.io[GBA_REG(SIOMLT_SEND)];
			node->d.p->p->memory.io[GBA_REG(SIOMULTI0)] = 0xFFFF;
			node->d.p->p->memory.io[GBA_REG(SIOMULTI1)] = 0xFFFF;
			node->d.p->p->memory.io[GBA_REG(SIOMULTI2)] = 0xFFFF;
			node->d.p->p->memory.io[GBA_REG(SIOMULTI3)] = 0xFFFF;
			node->d.p->siocnt = GBASIOMultiplayerFillBusy(node->d.p->siocnt);
			break;
		case GBA_SIO_NORMAL_8:
			node->p->multiRecv[node->id] = 0xFFFF;
			node->p->normalRecv[node->id] = node->d.p->p->memory.io[GBA_REG(SIODATA8)] & 0xFF;
			break;
		case GBA_SIO_NORMAL_32:
			node->p->multiRecv[node->id] = 0xFFFF;
			node->p->normalRecv[node->id] = node->d.p->p->memory.io[GBA_REG(SIODATA32_LO)];
			node->p->normalRecv[node->id] |= node->d.p->p->memory.io[GBA_REG(SIODATA32_HI)] << 16;
			break;
		default:
			node->p->multiRecv[node->id] = 0xFFFF;
			break;
		}
		signal = true;
		break;
	case TRANSFER_FINISHED:
		if (node->p->d.unusedCycles(&node->p->d, node->id) > node->eventDiff) {
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
		node->p->d.signal(&node->p->d, 1 << node->id);
	}

	return 0;
}

static void _GBASIOLockstepNodeProcessEvents(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	struct GBASIOLockstepNode* node = user;
	mLockstepLock(&node->p->d);

	int32_t cycles = node->nextEvent;
	node->nextEvent -= cyclesLate;
	node->eventDiff += cyclesLate;
	if (node->p->d.attached < 2) {
		switch (node->mode) {
		case GBA_SIO_MULTI:
			cycles = GBASIOTransferCycles(node->d.p->mode, node->d.p->siocnt, node->p->d.attached);
			break;
		case GBA_SIO_NORMAL_8:
		case GBA_SIO_NORMAL_32:
			if (node->nextEvent <= 0) {
				cycles = _masterUpdate(node);
				node->eventDiff = 0;
			}
			break;
		default:
			break;
		}
	} else if (node->nextEvent <= 0) {
		if (!node->id) {
			cycles = _masterUpdate(node);
		} else {
			cycles = _slaveUpdate(node);
			cycles += node->p->d.useCycles(&node->p->d, node->id, node->eventDiff);
		}
		node->eventDiff = 0;
	}
	if (cycles > 0) {
		node->nextEvent = 0;
		node->eventDiff += cycles;
		mTimingDeschedule(timing, &node->event);
		mTimingSchedule(timing, &node->event, cycles);
	} else {
		node->d.p->p->earlyExit = true;
		node->eventDiff += 1;
		mTimingSchedule(timing, &node->event, 1);
	}

	mLockstepUnlock(&node->p->d);
}

static uint16_t GBASIOLockstepNodeNormalWriteSIOCNT(struct GBASIODriver* driver, uint16_t value) {
	struct GBASIOLockstepNode* node = (struct GBASIOLockstepNode*) driver;

	mLockstepLock(&node->p->d);

	mLOG(GBA_SIO, DEBUG, "Lockstep %i: SIOCNT <- %04X", node->id, value);
	int attached;
	ATOMIC_LOAD(attached, node->p->attachedNormal);
	value &= 0xFF8B;
	if (node->id > 0) {
		value = GBASIONormalSetSi(value, GBASIONormalGetIdleSo(node->p->players[node->id - 1]->d.p->siocnt));
	} else {
		value = GBASIONormalClearSi(value);
	}

	enum mLockstepPhase transferActive;
	ATOMIC_LOAD(transferActive, node->p->d.transferActive);
	if (node->id < 3 && attached > node->id + 1 && transferActive == TRANSFER_IDLE) {
		int try;
		for (try = 0; try < 3; ++try) {
			GBASIONormal nextSiocnct;
			ATOMIC_LOAD(nextSiocnct, node->p->players[node->id + 1]->d.p->siocnt);
			if (ATOMIC_CMPXCHG(node->p->players[node->id + 1]->d.p->siocnt, nextSiocnct, GBASIONormalSetSi(nextSiocnct, GBASIONormalGetIdleSo(value)))) {
				break;
			}
		}
	}
	if ((value & 0x0081) == 0x0081) {
		if (!node->id) {
			// Frequency
			int32_t cycles = GBASIOTransferCycles(node->d.p->mode, node->d.p->siocnt, attached);

			if (transferActive == TRANSFER_IDLE) {
				mLOG(GBA_SIO, DEBUG, "Lockstep %i: Transfer initiated", node->id);
				ATOMIC_STORE(node->p->d.transferActive, TRANSFER_STARTING);
				ATOMIC_STORE(node->p->d.transferCycles, cycles);

				if (mTimingIsScheduled(&driver->p->p->timing, &node->event)) {
					node->eventDiff -= node->event.when - mTimingCurrentTime(&driver->p->p->timing);
					mTimingDeschedule(&driver->p->p->timing, &node->event);
				}
				mTimingSchedule(&driver->p->p->timing, &node->event, 0);
			} else {
				value &= ~0x0080;
			}
		} else {
			// TODO
		}
	}

	mLockstepUnlock(&node->p->d);

	return value;
}

#define TARGET(P) (1 << (P))
#define TARGET_ALL 0xF
#define TARGET_PRIMARY 0x1
#define TARGET_SECONDARY ((TARGET_ALL) & ~(TARGET_PRIMARY))

static bool GBASIOLockstepDriverInit(struct GBASIODriver* driver);
static void GBASIOLockstepDriverDeinit(struct GBASIODriver* driver);
static void GBASIOLockstepDriverReset(struct GBASIODriver* driver);
static bool GBASIOLockstepDriverLoad(struct GBASIODriver* driver);
static bool GBASIOLockstepDriverUnload(struct GBASIODriver* driver);
static void GBASIOLockstepDriverSetMode(struct GBASIODriver* driver, enum GBASIOMode mode);
static bool GBASIOLockstepDriverHandlesMode(struct GBASIODriver* driver, enum GBASIOMode mode);
static int GBASIOLockstepDriverConnectedDevices(struct GBASIODriver* driver);
static int GBASIOLockstepDriverDeviceId(struct GBASIODriver* driver);
static uint16_t GBASIOLockstepDriverWriteSIOCNT(struct GBASIODriver* driver, uint16_t value);
static uint16_t GBASIOLockstepDriverWriteRCNT(struct GBASIODriver* driver, uint16_t value);
static bool GBASIOLockstepDriverStart(struct GBASIODriver* driver);
static void GBASIOLockstepDriverFinishMultiplayer(struct GBASIODriver* driver, uint16_t data[4]);
static uint8_t GBASIOLockstepDriverFinishNormal8(struct GBASIODriver* driver);
static uint32_t GBASIOLockstepDriverFinishNormal32(struct GBASIODriver* driver);

static void GBASIOLockstepCoordinatorWaitOnPlayers(struct GBASIOLockstepCoordinator*, struct GBASIOLockstepPlayer*);
static void GBASIOLockstepCoordinatorAckPlayer(struct GBASIOLockstepCoordinator*, struct GBASIOLockstepPlayer*);
static void GBASIOLockstepCoordinatorWakePlayers(struct GBASIOLockstepCoordinator*);

static int32_t GBASIOLockstepTime(struct GBASIOLockstepPlayer*);
static void GBASIOLockstepPlayerWake(struct GBASIOLockstepPlayer*);
static void GBASIOLockstepPlayerSleep(struct GBASIOLockstepPlayer*);

static void _advanceCycle(struct GBASIOLockstepCoordinator*, struct GBASIOLockstepPlayer*);
static void _removePlayer(struct GBASIOLockstepCoordinator*, struct GBASIOLockstepPlayer*);
static void _reconfigPlayers(struct GBASIOLockstepCoordinator*);
static int32_t _untilNextSync(struct GBASIOLockstepCoordinator*, struct GBASIOLockstepPlayer*);
static void _enqueueEvent(struct GBASIOLockstepCoordinator*, const struct GBASIOLockstepEvent*, uint32_t target);
static void _setData(struct GBASIOLockstepCoordinator*, uint32_t id, struct GBASIO* sio);
static void _setReady(struct GBASIOLockstepCoordinator*, struct GBASIOLockstepPlayer* activePlayer, int playerId, enum GBASIOMode mode);
static void _hardSync(struct GBASIOLockstepCoordinator*, struct GBASIOLockstepPlayer*);

static void _lockstepEvent(struct mTiming*, void* context, uint32_t cyclesLate);

void GBASIOLockstepDriverCreate(struct GBASIOLockstepDriver* driver, struct mLockstepUser* user) {
	memset(driver, 0, sizeof(*driver));
	driver->d.init = GBASIOLockstepDriverInit;
	driver->d.deinit = GBASIOLockstepDriverDeinit;
	driver->d.reset = GBASIOLockstepDriverReset;
	driver->d.load = GBASIOLockstepDriverLoad;
	driver->d.unload = GBASIOLockstepDriverUnload;
	driver->d.setMode = GBASIOLockstepDriverSetMode;
	driver->d.handlesMode = GBASIOLockstepDriverHandlesMode;
	driver->d.deviceId = GBASIOLockstepDriverDeviceId;
	driver->d.connectedDevices = GBASIOLockstepDriverConnectedDevices;
	driver->d.writeSIOCNT = GBASIOLockstepDriverWriteSIOCNT;
	driver->d.writeRCNT = GBASIOLockstepDriverWriteRCNT;
	driver->d.start = GBASIOLockstepDriverStart;
	driver->d.finishMultiplayer = GBASIOLockstepDriverFinishMultiplayer;
	driver->d.finishNormal8 = GBASIOLockstepDriverFinishNormal8;
	driver->d.finishNormal32 = GBASIOLockstepDriverFinishNormal32;
	driver->event.context = driver;
	driver->event.callback = _lockstepEvent;
	driver->event.name = "GBA SIO Lockstep";
	driver->event.priority = 0x80;
	driver->user = user;
}

static bool GBASIOLockstepDriverInit(struct GBASIODriver* driver) {
	GBASIOLockstepDriverReset(driver);
	return true;
}

static void GBASIOLockstepDriverDeinit(struct GBASIODriver* driver) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	MutexLock(&coordinator->mutex);
	struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, lockstep->lockstepId);
	if (player) {
		_removePlayer(coordinator, player);
	}
	MutexUnlock(&coordinator->mutex);
	mTimingDeschedule(&lockstep->d.p->p->timing, &lockstep->event);
	lockstep->lockstepId = 0;
}

static void GBASIOLockstepDriverReset(struct GBASIODriver* driver) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	if (!lockstep->lockstepId) {
		struct GBASIOLockstepPlayer* player = calloc(1, sizeof(*player));
		unsigned id;
		player->driver = lockstep;
		player->mode = driver->p->mode;
		player->playerId = -1;

		int i;
		for (i = 0; i < MAX_LOCKSTEP_EVENTS - 1; ++i) {
			player->buffer[i].next = &player->buffer[i + 1];
		}
		player->freeList = &player->buffer[0];

		MutexLock(&coordinator->mutex);
		while (true) {
			if (coordinator->nextId == UINT_MAX) {
				coordinator->nextId = 0;
			}
			++coordinator->nextId;
			id = coordinator->nextId;
			if (!TableLookup(&coordinator->players, id)) {
				TableInsert(&coordinator->players, id, player);
				lockstep->lockstepId = id;
				break;
			}
		}
		_reconfigPlayers(coordinator);
		if (player->playerId != 0) {
			player->cycleOffset = mTimingCurrentTime(&driver->p->p->timing) - coordinator->cycle + LOCKSTEP_INCREMENT;
			struct GBASIOLockstepEvent event = {
				.type = SIO_EV_ATTACH,
				.playerId = player->playerId,
				.timestamp = GBASIOLockstepTime(player),
			};
			_enqueueEvent(coordinator, &event, TARGET_ALL & ~TARGET(player->playerId));
		}
		MutexUnlock(&coordinator->mutex);
	}

	if (mTimingIsScheduled(&lockstep->d.p->p->timing, &lockstep->event)) {
		return;
	}

	int32_t nextEvent;
	MutexLock(&coordinator->mutex);
	struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, lockstep->lockstepId);
	_setReady(coordinator, player, player->playerId, player->mode);
	if (TableSize(&coordinator->players) == 1) {
		coordinator->cycle = mTimingCurrentTime(&lockstep->d.p->p->timing);
		nextEvent = LOCKSTEP_INCREMENT;
	} else {
		_setReady(coordinator, player, 0, coordinator->transferMode);
		nextEvent = _untilNextSync(lockstep->coordinator, player);
	}
	MutexUnlock(&coordinator->mutex);
	mTimingSchedule(&lockstep->d.p->p->timing, &lockstep->event, nextEvent);
}

static bool GBASIOLockstepDriverLoad(struct GBASIODriver* driver) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	if (lockstep->lockstepId) {
		struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
		MutexLock(&coordinator->mutex);
		struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, lockstep->lockstepId);
		_setReady(coordinator, player, 0, coordinator->transferMode);
		MutexUnlock(&coordinator->mutex);
		GBASIOLockstepDriverSetMode(driver, driver->p->mode);
	}
	return true;
}

static bool GBASIOLockstepDriverUnload(struct GBASIODriver* driver) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	if (lockstep->lockstepId) {
		GBASIOLockstepDriverSetMode(driver, -1);
	}
	return true;
}

static void GBASIOLockstepDriverSetMode(struct GBASIODriver* driver, enum GBASIOMode mode) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	MutexLock(&coordinator->mutex);
	struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, lockstep->lockstepId);
	if (mode != player->mode) {
		player->mode = mode;
		struct GBASIOLockstepEvent event = {
			.type = SIO_EV_MODE_SET,
			.playerId = player->playerId,
			.timestamp = GBASIOLockstepTime(player),
			.mode = mode,
		};
		if (player->playerId == 0) {
			mASSERT(!coordinator->transferActive); // TODO
			coordinator->transferMode = mode;
			GBASIOLockstepCoordinatorWaitOnPlayers(coordinator, player);
		}
		_setReady(coordinator, player, player->playerId, mode);
		_enqueueEvent(coordinator, &event, TARGET_ALL & ~TARGET(player->playerId));
	}
	MutexUnlock(&coordinator->mutex);
}

static bool GBASIOLockstepDriverHandlesMode(struct GBASIODriver* driver, enum GBASIOMode mode) {
	UNUSED(driver);
	UNUSED(mode);
	return true;
}

static int GBASIOLockstepDriverConnectedDevices(struct GBASIODriver* driver) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	if (!lockstep->lockstepId) {
		return 0;
	}
	MutexLock(&coordinator->mutex);
	int attached = coordinator->nAttached - 1;
	MutexUnlock(&coordinator->mutex);
	return attached;
}

static int GBASIOLockstepDriverDeviceId(struct GBASIODriver* driver) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	int playerId = 0;
	MutexLock(&coordinator->mutex);
	struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, lockstep->lockstepId);
	if (player && player->playerId >= 0) {
		playerId = player->playerId;
	}
	MutexUnlock(&coordinator->mutex);
	return playerId;
}

static uint16_t GBASIOLockstepDriverWriteSIOCNT(struct GBASIODriver* driver, uint16_t value) {
	UNUSED(driver);
	mLOG(GBA_SIO, DEBUG, "Lockstep: SIOCNT <- %04X", value);
	return value;
}

static uint16_t GBASIOLockstepDriverWriteRCNT(struct GBASIODriver* driver, uint16_t value) {
	UNUSED(driver);
	mLOG(GBA_SIO, DEBUG, "Lockstep: RCNT <- %04X", value);
	return value;
}

static bool GBASIOLockstepDriverStart(struct GBASIODriver* driver) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	bool ret = false;
	MutexLock(&coordinator->mutex);
	if (coordinator->transferActive) {
		mLOG(GBA_SIO, ERROR, "Transfer restarted unexpectedly");
		goto out;
	}
	struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, lockstep->lockstepId);
	if (player->playerId != 0) {
		mLOG(GBA_SIO, DEBUG, "Secondary player attempted to start transfer");
		goto out;
	}
	mLOG(GBA_SIO, DEBUG, "Transfer starting at %08X", coordinator->cycle);
	memset(coordinator->multiData, 0xFF, sizeof(coordinator->multiData));
	_setData(coordinator, 0, player->driver->d.p);

	int32_t timestamp = GBASIOLockstepTime(player);
	struct GBASIOLockstepEvent event = {
		.type = SIO_EV_TRANSFER_START,
		.timestamp = timestamp,
		.finishCycle = timestamp + GBASIOTransferCycles(player->mode, player->driver->d.p->siocnt, coordinator->nAttached - 1),
	};
	_enqueueEvent(coordinator, &event, TARGET_SECONDARY);
	GBASIOLockstepCoordinatorWaitOnPlayers(coordinator, player);
	coordinator->transferActive = true;
	ret = true;
out:
	MutexUnlock(&coordinator->mutex);
	return ret;
}

static void GBASIOLockstepDriverFinishMultiplayer(struct GBASIODriver* driver, uint16_t data[4]) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	MutexLock(&coordinator->mutex);
	if (coordinator->transferMode == GBA_SIO_MULTI) {
		struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, lockstep->lockstepId);
		if (!player->dataReceived) {
			mLOG(GBA_SIO, WARN, "MULTI did not receive data. Are we running behind?");
			memset(data, 0xFF, sizeof(uint16_t) * 4);
		} else {
			mLOG(GBA_SIO, INFO, "MULTI transfer finished: %04X %04X %04X %04X",
			     coordinator->multiData[0],
			     coordinator->multiData[1],
			     coordinator->multiData[2],
			     coordinator->multiData[3]);
			memcpy(data, coordinator->multiData, sizeof(uint16_t) * 4);
		}
		player->dataReceived = false;
		if (player->playerId == 0) {
			_hardSync(coordinator, player);
		}
	}
	MutexUnlock(&coordinator->mutex);
}

static uint8_t GBASIOLockstepDriverFinishNormal8(struct GBASIODriver* driver) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	uint8_t data = 0xFF;
	MutexLock(&coordinator->mutex);
	if (coordinator->transferMode == GBA_SIO_NORMAL_8) {
		struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, lockstep->lockstepId);
		if (player->playerId > 0) {
			if (!player->dataReceived) {
				mLOG(GBA_SIO, WARN, "NORMAL did not receive data. Are we running behind?");
			} else {
				data = coordinator->normalData[player->playerId - 1];
				mLOG(GBA_SIO, INFO, "NORMAL8 transfer finished: %02X", data);
			}
		}
		player->dataReceived = false;
		if (player->playerId == 0) {
			_hardSync(coordinator, player);
		}
	}
	MutexUnlock(&coordinator->mutex);
	return data;
}

static uint32_t GBASIOLockstepDriverFinishNormal32(struct GBASIODriver* driver) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	uint32_t data = 0xFFFFFFFF;
	MutexLock(&coordinator->mutex);
	if (coordinator->transferMode == GBA_SIO_NORMAL_32) {
		struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, lockstep->lockstepId);
		if (player->playerId > 0) {
			if (!player->dataReceived) {
				mLOG(GBA_SIO, WARN, "Did not receive data. Are we running behind?");
			} else {
				data = coordinator->normalData[player->playerId - 1];
				mLOG(GBA_SIO, INFO, "NORMAL32 transfer finished: %08X", data);
			}
		}
		player->dataReceived = false;
		if (player->playerId == 0) {
			_hardSync(coordinator, player);
		}
	}
	MutexUnlock(&coordinator->mutex);
	return data;
}

void GBASIOLockstepCoordinatorInit(struct GBASIOLockstepCoordinator* coordinator) {
	memset(coordinator, 0, sizeof(*coordinator));
	MutexInit(&coordinator->mutex);
	TableInit(&coordinator->players, 8, free);
}

void GBASIOLockstepCoordinatorDeinit(struct GBASIOLockstepCoordinator* coordinator) {
	MutexDeinit(&coordinator->mutex);
	TableDeinit(&coordinator->players);
}

void GBASIOLockstepCoordinatorAttach(struct GBASIOLockstepCoordinator* coordinator, struct GBASIOLockstepDriver* driver) {
	if (driver->coordinator && driver->coordinator != coordinator) {
		// TODO
		abort();
	}
	driver->coordinator = coordinator;
}

void GBASIOLockstepCoordinatorDetach(struct GBASIOLockstepCoordinator* coordinator, struct GBASIOLockstepDriver* driver) {
	if (driver->coordinator != coordinator) {
		// TODO
		abort();
		return;
	}
	MutexLock(&coordinator->mutex);
	struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, driver->lockstepId);
	if (player) {
		_removePlayer(coordinator, player);
	}
	MutexUnlock(&coordinator->mutex);
	driver->coordinator = NULL;
}

int32_t _untilNextSync(struct GBASIOLockstepCoordinator* coordinator, struct GBASIOLockstepPlayer* player) {
	int32_t cycle = coordinator->cycle - GBASIOLockstepTime(player);
	if (player->playerId == 0) {
		cycle += LOCKSTEP_INCREMENT;
	}
	return cycle;
}

void _advanceCycle(struct GBASIOLockstepCoordinator* coordinator, struct GBASIOLockstepPlayer* player) {
	int32_t newCycle = GBASIOLockstepTime(player);
	mASSERT(newCycle - coordinator->cycle >= 0);
	//mLOG(GBA_SIO, DEBUG, "Advancing from cycle %08X to %08X (%i cycles)", coordinator->cycle, newCycle, newCycle - coordinator->cycle);
	coordinator->cycle = newCycle;
}

void _removePlayer(struct GBASIOLockstepCoordinator* coordinator, struct GBASIOLockstepPlayer* player) {
	struct GBASIOLockstepEvent event = {
		.type = SIO_EV_DETACH,
		.playerId = player->playerId,
		.timestamp = GBASIOLockstepTime(player),
	};
	_enqueueEvent(coordinator, &event, TARGET_ALL & ~TARGET(player->playerId));
	GBASIOLockstepCoordinatorWakePlayers(coordinator);
	if (player->playerId != 0) {
		GBASIOLockstepCoordinatorAckPlayer(coordinator, player);
	}
	TableRemove(&coordinator->players, player->driver->lockstepId);
	_reconfigPlayers(coordinator);
}

void _reconfigPlayers(struct GBASIOLockstepCoordinator* coordinator) {
	size_t players = TableSize(&coordinator->players);
	memset(coordinator->attachedPlayers, 0, sizeof(coordinator->attachedPlayers));
	if (players == 0) {
		mLOG(GBA_SIO, WARN, "Reconfiguring player IDs with no players attached somehow?");
	} else if (players == 1) {
		struct TableIterator iter;
		mASSERT(TableIteratorStart(&coordinator->players, &iter));
		unsigned p0 = TableIteratorGetKey(&coordinator->players, &iter);
		coordinator->attachedPlayers[0] = p0;

		struct GBASIOLockstepPlayer* player = TableIteratorGetValue(&coordinator->players, &iter);
		coordinator->cycle = mTimingCurrentTime(&player->driver->d.p->p->timing);

		if (player->playerId != 0) {
			player->playerId = 0;
			if (player->driver->user->playerIdChanged) {
				player->driver->user->playerIdChanged(player->driver->user, player->playerId);
			}
		}

		if (!coordinator->transferActive) {
			coordinator->transferMode = player->mode;
		}
	} else {
		struct UIntList playerPreferences[MAX_GBAS];

		int i;
		for (i = 0; i < MAX_GBAS; ++i) {
			UIntListInit(&playerPreferences[i], 4);
		}

		// Collect the first four players' requested player IDs so we can sort through them later
		int seen = 0;
		struct TableIterator iter;
		mASSERT(TableIteratorStart(&coordinator->players, &iter));
		do {
			unsigned pid = TableIteratorGetKey(&coordinator->players, &iter);
			struct GBASIOLockstepPlayer* player = TableIteratorGetValue(&coordinator->players, &iter);
			int requested = MAX_GBAS - 1;
			if (player->driver->user->requestedId) {
				requested = player->driver->user->requestedId(player->driver->user);
			}
			if (requested < 0) {
				continue;
			}
			if (requested >= MAX_GBAS) {
				requested = MAX_GBAS - 1;
			}

			*UIntListAppend(&playerPreferences[requested]) = pid;
			++seen;
		} while (TableIteratorNext(&coordinator->players, &iter) && seen < MAX_GBAS);

		// Now sort each requested player ID to figure out who gets which ID
		seen = 0;
		for (i = 0; i < MAX_GBAS; ++i) {
			int j;
			for (j = 0; j <= i; ++j) {
				while (UIntListSize(&playerPreferences[j]) && seen < MAX_GBAS) {
					unsigned pid = *UIntListGetPointer(&playerPreferences[j], 0);
					UIntListShift(&playerPreferences[j], 0, 1);
					struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, pid);
					if (!player) {
						mLOG(GBA_SIO, ERROR, "Player list appears to have changed unexpectedly. PID %u missing.", pid);
						continue;
					}
					coordinator->attachedPlayers[seen] = pid;
					if (player->playerId != seen) {
						player->playerId = seen;
						if (player->driver->user->playerIdChanged) {
							player->driver->user->playerIdChanged(player->driver->user, player->playerId);
						}
					}
					++seen;
				}
			}
		}

		for (i = 0; i < MAX_GBAS; ++i) {
			UIntListDeinit(&playerPreferences[i]);
		}
	}

	int nAttached = 0;
	size_t i;
	for (i = 0; i < MAX_GBAS; ++i) {
		unsigned pid = coordinator->attachedPlayers[i];
		if (!pid) {
			continue;
		}
		struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, pid);
		if (!player) {
			coordinator->attachedPlayers[i] = 0;
		} else {
			++nAttached;
		}
	}
	coordinator->nAttached = nAttached;
}

static void _setData(struct GBASIOLockstepCoordinator* coordinator, uint32_t id, struct GBASIO* sio) {
	switch (coordinator->transferMode) {
	case GBA_SIO_MULTI:
		coordinator->multiData[id] = sio->p->memory.io[GBA_REG(SIOMLT_SEND)];
		break;
	case GBA_SIO_NORMAL_8:
		coordinator->normalData[id] = sio->p->memory.io[GBA_REG(SIODATA8)];
		break;
	case GBA_SIO_NORMAL_32:
		coordinator->normalData[id] = sio->p->memory.io[GBA_REG(SIODATA32_LO)];
		coordinator->normalData[id] |= sio->p->memory.io[GBA_REG(SIODATA32_HI)] << 16;
		break;
	case GBA_SIO_UART:
	case GBA_SIO_GPIO:
	case GBA_SIO_JOYBUS:
		mLOG(GBA_SIO, ERROR, "Unsupported mode %i in lockstep", coordinator->transferMode);
		// TODO: Should we handle this or just abort?
		abort();
		break;
	}
}

void _setReady(struct GBASIOLockstepCoordinator* coordinator, struct GBASIOLockstepPlayer* activePlayer, int playerId, enum GBASIOMode mode) {
	activePlayer->otherModes[playerId] = mode;
	bool ready = true;
	int i;
	for (i = 0; ready && i < coordinator->nAttached; ++i) {
		ready = activePlayer->otherModes[i] == activePlayer->mode;
	}
	if (activePlayer->mode == GBA_SIO_MULTI) {
		struct GBASIO* sio = activePlayer->driver->d.p;
		sio->siocnt = GBASIOMultiplayerSetReady(sio->siocnt, ready);
		sio->rcnt = GBASIORegisterRCNTSetSd(sio->rcnt, ready);
	}
}

void _hardSync(struct GBASIOLockstepCoordinator* coordinator, struct GBASIOLockstepPlayer* player) {
	mASSERT_DEBUG(player->playerId == 0);
	struct GBASIOLockstepEvent event = {
		.type = SIO_EV_HARD_SYNC,
		.playerId = 0,
		.timestamp = GBASIOLockstepTime(player),
	};
	_enqueueEvent(coordinator, &event, TARGET_SECONDARY);
	GBASIOLockstepCoordinatorWaitOnPlayers(coordinator, player);
}

void _enqueueEvent(struct GBASIOLockstepCoordinator* coordinator, const struct GBASIOLockstepEvent* event, uint32_t target) {
	mLOG(GBA_SIO, DEBUG, "Enqueuing event of type %X from %i for target %X at timestamp %X",
	                      event->type, event->playerId, target, event->timestamp);

	int i;
	for (i = 0; i < coordinator->nAttached; ++i) {
		if (!(target & TARGET(i))) {
			continue;
		}
		struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, coordinator->attachedPlayers[i]);
		mASSERT_LOG(GBA_SIO, player->freeList, "No free events");
		struct GBASIOLockstepEvent* newEvent = player->freeList;
		player->freeList = newEvent->next;

		memcpy(newEvent, event, sizeof(*event));
		struct GBASIOLockstepEvent** previous = &player->queue;
		struct GBASIOLockstepEvent* next = player->queue;
		while (next) {
			int32_t until = newEvent->timestamp - next->timestamp;
			if (until < 0) {
				break;
			}
			previous = &next->next;
			next = next->next;
		}
		newEvent->next = next;
		*previous = newEvent;
	}
}

void _lockstepEvent(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GBASIOLockstepDriver* lockstep = context;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	MutexLock(&coordinator->mutex);
	struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, lockstep->lockstepId);
	struct GBASIO* sio = player->driver->d.p;
	mASSERT(player->playerId >= 0 && player->playerId < 4);

	bool wasDetach = false;
	if (player->queue && player->queue->type == SIO_EV_DETACH) {
		mLOG(GBA_SIO, DEBUG, "Player %i detached at timestamp %X, picking up the pieces",
		                      player->queue->playerId, player->queue->timestamp);
		wasDetach = true;
	}
	if (player->playerId == 0) {
		// We are the clock owner; advance the shared clock
		_advanceCycle(coordinator, player);
		if (!coordinator->transferActive) {
			GBASIOLockstepCoordinatorWakePlayers(coordinator);
		}
	}

	int32_t nextEvent = _untilNextSync(coordinator, player);
	//mASSERT_DEBUG(nextEvent + cyclesLate > 0);
	while (true) {
		struct GBASIOLockstepEvent* event = player->queue;
		if (!event) {
			break;
		}
		if (event->timestamp > GBASIOLockstepTime(player)) {
			break;
		}
		player->queue = event->next;
		struct GBASIOLockstepEvent reply = {
			.playerId = player->playerId,
			.timestamp = GBASIOLockstepTime(player),
		};
		mLOG(GBA_SIO, DEBUG, "Got event of type %X from %i at timestamp %X",
		                      event->type, event->playerId, event->timestamp);
		switch (event->type) {
		case SIO_EV_ATTACH:
			_setReady(coordinator, player, event->playerId, -1);
			if (player->playerId == 0) {
				struct GBASIO* sio = player->driver->d.p;
				sio->siocnt = GBASIOMultiplayerClearSlave(sio->siocnt);
			}
			reply.mode = player->mode;
			reply.type = SIO_EV_MODE_SET;
			_enqueueEvent(coordinator, &reply, TARGET(event->playerId));
			break;
		case SIO_EV_HARD_SYNC:
			GBASIOLockstepCoordinatorAckPlayer(coordinator, player);
			break;
		case SIO_EV_TRANSFER_START:
			_setData(coordinator, player->playerId, sio);
			nextEvent = event->finishCycle - GBASIOLockstepTime(player) - cyclesLate;
			player->driver->d.p->siocnt |= 0x80;
			mTimingDeschedule(&sio->p->timing, &sio->completeEvent);
			mTimingSchedule(&sio->p->timing, &sio->completeEvent, nextEvent);
			GBASIOLockstepCoordinatorAckPlayer(coordinator, player);
			break;
		case SIO_EV_MODE_SET:
			_setReady(coordinator, player, event->playerId, event->mode);
			if (event->playerId == 0) {
				GBASIOLockstepCoordinatorAckPlayer(coordinator, player);
			}
			break;
		case SIO_EV_DETACH:
			_setReady(coordinator, player, event->playerId, -1);
			_setReady(coordinator, player, player->playerId, player->mode);
			reply.mode = player->mode;
			reply.type = SIO_EV_MODE_SET;
			_enqueueEvent(coordinator, &reply, ~TARGET(event->playerId));
			if (player->mode == GBA_SIO_MULTI) {
				sio->siocnt = GBASIOMultiplayerSetId(sio->siocnt, player->playerId);
				sio->siocnt = GBASIOMultiplayerSetSlave(sio->siocnt, player->playerId || coordinator->nAttached < 2);
			}
			wasDetach = true;
			break;
		}
		event->next = player->freeList;
		player->freeList = event;
	}
	if (player->queue && player->queue->timestamp - GBASIOLockstepTime(player) < nextEvent) {
		nextEvent = player->queue->timestamp - GBASIOLockstepTime(player);
	}

	if (player->playerId != 0 && nextEvent <= LOCKSTEP_INCREMENT) {
		if (!player->queue || wasDetach) {
			GBASIOLockstepPlayerSleep(player);
			// XXX: Is there a better way to gain sync lock at the beginning?
			if (nextEvent < 4) {
				nextEvent = 4;
			}
		}
	}
	MutexUnlock(&coordinator->mutex);

	mASSERT_DEBUG(nextEvent > 0);
	mTimingSchedule(timing, &lockstep->event, nextEvent);
}

int32_t GBASIOLockstepTime(struct GBASIOLockstepPlayer* player) {
	return mTimingCurrentTime(&player->driver->d.p->p->timing) - player->cycleOffset;
}

void GBASIOLockstepCoordinatorWaitOnPlayers(struct GBASIOLockstepCoordinator* coordinator, struct GBASIOLockstepPlayer* player) {
	mASSERT(!coordinator->waiting);
	mASSERT(!player->asleep);
	mASSERT(player->playerId == 0);
	if (coordinator->nAttached < 2) {
		return;
	}

	_advanceCycle(coordinator, player);
	mLOG(GBA_SIO, DEBUG, "Primary waiting for players to ack");
	coordinator->waiting = ((1 << coordinator->nAttached) - 1) & ~TARGET(player->playerId);
	GBASIOLockstepPlayerSleep(player);
	GBASIOLockstepCoordinatorWakePlayers(coordinator);
}

void GBASIOLockstepCoordinatorWakePlayers(struct GBASIOLockstepCoordinator* coordinator) {
	//mLOG(GBA_SIO, DEBUG, "Waking all secondary players");
	int i;
	for (i = 1; i < coordinator->nAttached; ++i) {
		if (!coordinator->attachedPlayers[i]) {
			continue;
		}
		struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, coordinator->attachedPlayers[i]);
		GBASIOLockstepPlayerWake(player);
	}
}

void GBASIOLockstepPlayerWake(struct GBASIOLockstepPlayer* player) {
	if (!player->asleep) {
		return;
	}
	player->asleep = false;
	player->driver->user->wake(player->driver->user);
}

void GBASIOLockstepCoordinatorAckPlayer(struct GBASIOLockstepCoordinator* coordinator, struct GBASIOLockstepPlayer* player) {
	if (player->playerId == 0) {
		return;
	}
	mLOG(GBA_SIO, DEBUG, "Player %i acking primary", player->playerId);
	coordinator->waiting &= ~TARGET(player->playerId);
	if (!coordinator->waiting) {
		mLOG(GBA_SIO, DEBUG, "All players acked, waking primary");
		if (coordinator->transferActive) {
			int i;
			for (i = 0; i < coordinator->nAttached; ++i) {
				if (!coordinator->attachedPlayers[i]) {
					continue;
				}
				struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, coordinator->attachedPlayers[i]);
				player->dataReceived = true;
			}

			coordinator->transferActive = false;
		}

		struct GBASIOLockstepPlayer* runner = TableLookup(&coordinator->players, coordinator->attachedPlayers[0]);
		GBASIOLockstepPlayerWake(runner);
	}
	GBASIOLockstepPlayerSleep(player);
}

void GBASIOLockstepPlayerSleep(struct GBASIOLockstepPlayer* player) {
	if (player->asleep) {
		return;
	}
	//mLOG(GBA_SIO, DEBUG, "Player %i going to sleep with %i cycles until sync", player->playerId, _untilNextSync(coordinator, player));
	player->asleep = true;
	player->driver->user->sleep(player->driver->user);
	player->driver->d.p->p->cpu->nextEvent = 0;
	player->driver->d.p->p->earlyExit = true;
}

size_t GBASIOLockstepCoordinatorAttached(struct GBASIOLockstepCoordinator* coordinator) {
	size_t count;
	MutexLock(&coordinator->mutex);
	count = TableSize(&coordinator->players);
	MutexUnlock(&coordinator->mutex);
	return count;
}
