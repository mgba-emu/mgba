/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/sio/lockstep.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>

#define LOCKSTEP_INCREMENT 2000
#define LOCKSTEP_TRANSFER 512

static bool GBASIOLockstepNodeInit(struct GBASIODriver* driver);
static void GBASIOLockstepNodeDeinit(struct GBASIODriver* driver);
static bool GBASIOLockstepNodeLoad(struct GBASIODriver* driver);
static bool GBASIOLockstepNodeUnload(struct GBASIODriver* driver);
static uint16_t GBASIOLockstepNodeMultiWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value);
static uint16_t GBASIOLockstepNodeNormalWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value);
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
	node->d.init = GBASIOLockstepNodeInit;
	node->d.deinit = GBASIOLockstepNodeDeinit;
	node->d.load = GBASIOLockstepNodeLoad;
	node->d.unload = GBASIOLockstepNodeUnload;
	node->d.writeRegister = 0;
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
	case SIO_MULTI:
		node->d.writeRegister = GBASIOLockstepNodeMultiWriteRegister;
		node->d.p->rcnt |= 3;
		ATOMIC_ADD(node->p->attachedMulti, 1);
		node->d.p->siocnt = GBASIOMultiplayerSetReady(node->d.p->siocnt, node->p->attachedMulti == node->p->d.attached);
		if (node->id) {
			node->d.p->rcnt |= 4;
			node->d.p->siocnt = GBASIOMultiplayerFillSlave(node->d.p->siocnt);

			int try;
			for (try = 0; try < 3; ++try) {
				uint16_t masterSiocnt;
				ATOMIC_LOAD(masterSiocnt, node->p->players[0]->d.p->siocnt);
				if (ATOMIC_CMPXCHG(node->p->players[0]->d.p->siocnt, masterSiocnt, GBASIOMultiplayerClearSlave(masterSiocnt))) {
					break;
				}
			}
		} else {
			node->d.p->rcnt &= ~4;
			node->d.p->siocnt = GBASIOMultiplayerClearSlave(node->d.p->siocnt);
		}
		break;
	case SIO_NORMAL_8:
	case SIO_NORMAL_32:
		if (ATOMIC_ADD(node->p->attachedNormal, 1) > node->id + 1 && node->id < 3) {
			node->d.p->siocnt = GBASIONormalSetSi(node->d.p->siocnt, GBASIONormalGetIdleSo(node->p->players[node->id + 1]->d.p->siocnt));
		} else {
			node->d.p->siocnt = GBASIONormalFillSi(node->d.p->siocnt);
		}
		node->d.writeRegister = GBASIOLockstepNodeNormalWriteRegister;
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
	case SIO_MULTI:
		ATOMIC_SUB(node->p->attachedMulti, 1);
		break;
	case SIO_NORMAL_8:
	case SIO_NORMAL_32:
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
	node->mode = SIO_GPIO;

	mLockstepUnlock(&node->p->d);

	return true;
}

static uint16_t GBASIOLockstepNodeMultiWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value) {
	struct GBASIOLockstepNode* node = (struct GBASIOLockstepNode*) driver;

	mLockstepLock(&node->p->d);

	if (address == REG_SIOCNT) {
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
				ATOMIC_STORE(node->p->d.transferCycles, GBASIOCyclesPerTransfer[GBASIOMultiplayerGetBaud(node->d.p->siocnt)][node->p->d.attached - 1]);

				if (mTimingIsScheduled(&driver->p->p->timing, &node->event)) {
					node->eventDiff -= node->event.when - mTimingCurrentTime(&driver->p->p->timing);
					mTimingDeschedule(&driver->p->p->timing, &node->event);
				}
				mTimingSchedule(&driver->p->p->timing, &node->event, 0);
			} else {
				value &= ~0x0080;
			}
		}
		value &= 0xFF83;
		value |= driver->p->siocnt & 0x00FC;
	} else if (address == REG_SIOMLT_SEND) {
		mLOG(GBA_SIO, DEBUG, "Lockstep %i: SIOMLT_SEND <- %04X", node->id, value);
	} else {
		mLOG(GBA_SIO, STUB, "Lockstep %i: Unknown reg %03X <- %04X", node->id, address, value);
	}

	mLockstepUnlock(&node->p->d);

	return value;
}

static void _finishTransfer(struct GBASIOLockstepNode* node) {
	if (node->transferFinished) {
		return;
	}

	struct GBASIO* sio = node->d.p;
	switch (node->mode) {
	case SIO_MULTI:
		sio->p->memory.io[REG_SIOMULTI0 >> 1] = node->p->multiRecv[0];
		sio->p->memory.io[REG_SIOMULTI1 >> 1] = node->p->multiRecv[1];
		sio->p->memory.io[REG_SIOMULTI2 >> 1] = node->p->multiRecv[2];
		sio->p->memory.io[REG_SIOMULTI3 >> 1] = node->p->multiRecv[3];
		sio->rcnt |= 1;
		sio->siocnt = GBASIOMultiplayerClearBusy(sio->siocnt);
		sio->siocnt = GBASIOMultiplayerSetId(sio->siocnt, node->id);
		if (GBASIOMultiplayerIsIrq(sio->siocnt)) {
			GBARaiseIRQ(sio->p, GBA_IRQ_SIO, 0);
		}
		break;
	case SIO_NORMAL_8:
		// TODO
		sio->siocnt = GBASIONormalClearStart(sio->siocnt);
		if (node->id) {
			sio->siocnt = GBASIONormalSetSi(sio->siocnt, GBASIONormalGetIdleSo(node->p->players[node->id - 1]->d.p->siocnt));
			node->d.p->p->memory.io[REG_SIODATA8 >> 1] = node->p->normalRecv[node->id - 1] & 0xFF;
		} else {
			node->d.p->p->memory.io[REG_SIODATA8 >> 1] = 0xFFFF;
		}
		if (GBASIONormalIsIrq(sio->siocnt)) {
			GBARaiseIRQ(sio->p, GBA_IRQ_SIO, 0);
		}
		break;
	case SIO_NORMAL_32:
		// TODO
		sio->siocnt = GBASIONormalClearStart(sio->siocnt);
		if (node->id) {
			sio->siocnt = GBASIONormalSetSi(sio->siocnt, GBASIONormalGetIdleSo(node->p->players[node->id - 1]->d.p->siocnt));
			node->d.p->p->memory.io[REG_SIODATA32_LO >> 1] = node->p->normalRecv[node->id - 1];
			node->d.p->p->memory.io[REG_SIODATA32_HI >> 1] = node->p->normalRecv[node->id - 1] >> 16;
		} else {
			node->d.p->p->memory.io[REG_SIODATA32_LO >> 1] = 0xFFFF;
			node->d.p->p->memory.io[REG_SIODATA32_HI >> 1] = 0xFFFF;
		}
		if (GBASIONormalIsIrq(sio->siocnt)) {
			GBARaiseIRQ(sio->p, GBA_IRQ_SIO, 0);
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
		if (node->mode == SIO_MULTI) {
			node->d.p->siocnt = GBASIOMultiplayerSetReady(node->d.p->siocnt, attachedMulti == attached);
		}
		break;
	case TRANSFER_STARTING:
		// Start the transfer, but wait for the other GBAs to catch up
		node->transferFinished = false;
		switch (node->mode) {
		case SIO_MULTI:
			node->p->multiRecv[0] = node->d.p->p->memory.io[REG_SIOMLT_SEND >> 1];
			node->d.p->p->memory.io[REG_SIOMULTI0 >> 1] = 0xFFFF;
			node->d.p->p->memory.io[REG_SIOMULTI1 >> 1] = 0xFFFF;
			node->d.p->p->memory.io[REG_SIOMULTI2 >> 1] = 0xFFFF;
			node->d.p->p->memory.io[REG_SIOMULTI3 >> 1] = 0xFFFF;
			node->p->multiRecv[1] = 0xFFFF;
			node->p->multiRecv[2] = 0xFFFF;
			node->p->multiRecv[3] = 0xFFFF;
			break;
		case SIO_NORMAL_8:
			node->p->multiRecv[0] = 0xFFFF;
			node->p->normalRecv[0] = node->d.p->p->memory.io[REG_SIODATA8 >> 1] & 0xFF;
			break;
		case SIO_NORMAL_32:
			node->p->multiRecv[0] = 0xFFFF;
			mLOG(GBA_SIO, DEBUG, "Lockstep %i: SIODATA32_LO <- %04X", node->id, node->d.p->p->memory.io[REG_SIODATA32_LO >> 1]);
			mLOG(GBA_SIO, DEBUG, "Lockstep %i: SIODATA32_HI <- %04X", node->id, node->d.p->p->memory.io[REG_SIODATA32_HI >> 1]);
			node->p->normalRecv[0] = node->d.p->p->memory.io[REG_SIODATA32_LO >> 1];
			node->p->normalRecv[0] |= node->d.p->p->memory.io[REG_SIODATA32_HI >> 1] << 16;
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

	if (node->mode == SIO_MULTI) {
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
		case SIO_MULTI:
			node->d.p->rcnt &= ~1;
			node->p->multiRecv[node->id] = node->d.p->p->memory.io[REG_SIOMLT_SEND >> 1];
			node->d.p->p->memory.io[REG_SIOMULTI0 >> 1] = 0xFFFF;
			node->d.p->p->memory.io[REG_SIOMULTI1 >> 1] = 0xFFFF;
			node->d.p->p->memory.io[REG_SIOMULTI2 >> 1] = 0xFFFF;
			node->d.p->p->memory.io[REG_SIOMULTI3 >> 1] = 0xFFFF;
			node->d.p->siocnt = GBASIOMultiplayerFillBusy(node->d.p->siocnt);
			break;
		case SIO_NORMAL_8:
			node->p->multiRecv[node->id] = 0xFFFF;
			node->p->normalRecv[node->id] = node->d.p->p->memory.io[REG_SIODATA8 >> 1] & 0xFF;
			break;
		case SIO_NORMAL_32:
			node->p->multiRecv[node->id] = 0xFFFF;
			node->p->normalRecv[node->id] = node->d.p->p->memory.io[REG_SIODATA32_LO >> 1];
			node->p->normalRecv[node->id] |= node->d.p->p->memory.io[REG_SIODATA32_HI >> 1] << 16;
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
		case SIO_MULTI:
			cycles = GBASIOCyclesPerTransfer[GBASIOMultiplayerGetBaud(node->d.p->siocnt)][0];
			break;
		case SIO_NORMAL_8:
		case SIO_NORMAL_32:
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

static uint16_t GBASIOLockstepNodeNormalWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value) {
	struct GBASIOLockstepNode* node = (struct GBASIOLockstepNode*) driver;

	mLockstepLock(&node->p->d);

	if (address == REG_SIOCNT) {
		mLOG(GBA_SIO, DEBUG, "Lockstep %i: SIOCNT <- %04X", node->id, value);
		int attached;
		ATOMIC_LOAD(attached, node->p->attachedNormal);
		value &= 0xFF8B;
		if (node->id < 3 && attached > node->id + 1) {
			value = GBASIONormalSetSi(value, GBASIONormalGetIdleSo(node->p->players[node->id + 1]->d.p->siocnt));
		} else {
			value = GBASIONormalFillSi(value);
		}

		enum mLockstepPhase transferActive;
		ATOMIC_LOAD(transferActive, node->p->d.transferActive);
		if (node->id > 0 && transferActive == TRANSFER_IDLE) {
			int try;
			for (try = 0; try < 3; ++try) {
				GBASIONormal parentSiocnt;
				ATOMIC_LOAD(parentSiocnt, node->p->players[node->id - 1]->d.p->siocnt);
				if (ATOMIC_CMPXCHG(node->p->players[node->id - 1]->d.p->siocnt, parentSiocnt, GBASIONormalSetSi(parentSiocnt, GBASIONormalGetIdleSo(value)))) {
					break;
				}
			}
		}
		if ((value & 0x0081) == 0x0081) {
			if (!node->id) {
				// Frequency
				int32_t cycles;
				if (value & 2) {
					cycles = 8 * 8;
				} else {
					cycles = 64 * 8;
				}
				if (value & 0x1000) {
					cycles *= 4;
				}

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
	} else if (address == REG_SIODATA32_LO) {
		mLOG(GBA_SIO, DEBUG, "Lockstep %i: SIODATA32_LO <- %04X", node->id, value);
	} else if (address == REG_SIODATA32_HI) {
		mLOG(GBA_SIO, DEBUG, "Lockstep %i: SIODATA32_HI <- %04X", node->id, value);
	} else if (address == REG_SIODATA8) {
		mLOG(GBA_SIO, DEBUG, "Lockstep %i: SIODATA8 <- %02X", node->id, value);
	}

	mLockstepUnlock(&node->p->d);

	return value;
}
