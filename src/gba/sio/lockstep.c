/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "lockstep.h"

#include "gba/gba.h"
#include "gba/io.h"
#include "gba/video.h"

#define LOCKSTEP_INCREMENT 3000

static bool _nodeWait(struct GBASIOLockstepNode* node, uint32_t mask) {
	uint32_t oldMask = 0;
	if (ATOMIC_CMPXCHG(node->p->waiting[node->id], oldMask, mask)) {
		node->p->wait(node->p, node->id);
	}
#ifndef NDEBUG
	else if (oldMask != mask) {
		abort();
	}
#endif
	else if ((node->p->waiting[node->id] & oldMask) == node->p->waiting[node->id]) {
		ATOMIC_AND(node->p->waitMask, ~mask);
		return false;
	}

	return true;
}

static bool _nodeSignal(struct GBASIOLockstepNode* node, uint32_t mask) {
	mask = ATOMIC_OR(node->p->waitMask, mask);
	bool eventTriggered = false;
	int i;
	for (i = 0; i < node->p->attached; ++i) {
		uint32_t waiting = node->p->waiting[i];
		if (waiting && (waiting & mask) == waiting && ATOMIC_CMPXCHG(node->p->waiting[i], waiting, 0)) {
			node->p->signal(node->p, i);
			eventTriggered = true;
		}
	}
	if (eventTriggered) {
		ATOMIC_STORE(node->p->waitMask, 0);
	} else {
		mLOG(GBA_SIO, WARN, "Nothing woke with mask %X", mask);
	}
	return eventTriggered;
}


static bool GBASIOLockstepNodeInit(struct GBASIODriver* driver);
static void GBASIOLockstepNodeDeinit(struct GBASIODriver* driver);
static bool GBASIOLockstepNodeLoad(struct GBASIODriver* driver);
static bool GBASIOLockstepNodeUnload(struct GBASIODriver* driver);
static uint16_t GBASIOLockstepNodeMultiWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value);
static uint16_t GBASIOLockstepNodeNormalWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value);
static int32_t GBASIOLockstepNodeProcessEvents(struct GBASIODriver* driver, int32_t cycles);
static int32_t GBASIOLockstepNodeProcessEvents(struct GBASIODriver* driver, int32_t cycles);

void GBASIOLockstepInit(struct GBASIOLockstep* lockstep) {
	lockstep->players[0] = 0;
	lockstep->players[1] = 0;
	lockstep->players[2] = 0;
	lockstep->players[3] = 0;
	lockstep->multiRecv[0] = 0xFFFF;
	lockstep->multiRecv[1] = 0xFFFF;
	lockstep->multiRecv[2] = 0xFFFF;
	lockstep->multiRecv[3] = 0xFFFF;
	lockstep->attached = 0;
	lockstep->attachedMulti = 0;
	lockstep->transferActive = 0;
	memset(lockstep->waiting, 0, sizeof(lockstep->waiting));
	lockstep->waitMask = 0;
#ifndef NDEBUG
	lockstep->transferId = 0;
#endif
}

void GBASIOLockstepDeinit(struct GBASIOLockstep* lockstep) {
	UNUSED(lockstep);
}

void GBASIOLockstepNodeCreate(struct GBASIOLockstepNode* node) {
	node->d.init = GBASIOLockstepNodeInit;
	node->d.deinit = GBASIOLockstepNodeDeinit;
	node->d.load = GBASIOLockstepNodeLoad;
	node->d.unload = GBASIOLockstepNodeUnload;
	node->d.writeRegister = 0;
	node->d.processEvents = GBASIOLockstepNodeProcessEvents;
}

bool GBASIOLockstepAttachNode(struct GBASIOLockstep* lockstep, struct GBASIOLockstepNode* node) {
	if (lockstep->attached == MAX_GBAS) {
		return false;
	}
	lockstep->players[lockstep->attached] = node;
	node->p = lockstep;
	node->id = lockstep->attached;
	++lockstep->attached;
	return true;
}

void GBASIOLockstepDetachNode(struct GBASIOLockstep* lockstep, struct GBASIOLockstepNode* node) {
	if (lockstep->attached == 0) {
		return;
	}
	int i;
	for (i = 0; i < lockstep->attached; ++i) {
		if (lockstep->players[i] != node) {
			continue;
		}
		for (++i; i < lockstep->attached; ++i) {
			lockstep->players[i - 1] = lockstep->players[i];
			lockstep->players[i - 1]->id = i - 1;
		}
		--lockstep->attached;
		break;
	}
}

bool GBASIOLockstepNodeInit(struct GBASIODriver* driver) {
	struct GBASIOLockstepNode* node = (struct GBASIOLockstepNode*) driver;
	node->needsToWait = false;
	node->d.p->multiplayerControl.slave = node->id > 0;
	mLOG(GBA_SIO, DEBUG, "Lockstep %i: Node init", node->id);
	return true;
}

void GBASIOLockstepNodeDeinit(struct GBASIODriver* driver) {
	UNUSED(driver);
}

bool GBASIOLockstepNodeLoad(struct GBASIODriver* driver) {
	struct GBASIOLockstepNode* node = (struct GBASIOLockstepNode*) driver;
	node->nextEvent = 0;
	node->eventDiff = 0;
	node->mode = driver->p->mode;
	switch (node->mode) {
	case SIO_MULTI:
		node->d.writeRegister = GBASIOLockstepNodeMultiWriteRegister;
		node->d.p->rcnt |= 3;
		++node->p->attachedMulti;
		node->d.p->multiplayerControl.ready = node->p->attachedMulti == node->p->attached;
		if (node->id) {
			node->d.p->rcnt |= 4;
			node->d.p->multiplayerControl.slave = 1;
		}
		break;
	case SIO_NORMAL_32:
		node->d.writeRegister = GBASIOLockstepNodeNormalWriteRegister;
		break;
	default:
		break;
	}
#ifndef NDEBUG
	node->phase = node->p->transferActive;
	node->transferId = node->p->transferId;
#endif
	return true;
}

bool GBASIOLockstepNodeUnload(struct GBASIODriver* driver) {
	struct GBASIOLockstepNode* node = (struct GBASIOLockstepNode*) driver;
	node->mode = driver->p->mode;
	switch (node->mode) {
	case SIO_MULTI:
		--node->p->attachedMulti;
		break;
	default:
		break;
	}
	_nodeSignal(node, (1 << node->id) ^ 0xF);
	return true;
}

static uint16_t GBASIOLockstepNodeMultiWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value) {
	struct GBASIOLockstepNode* node = (struct GBASIOLockstepNode*) driver;
	if (address == REG_SIOCNT) {
		mLOG(GBA_SIO, DEBUG, "Lockstep %i: SIOCNT <- %04x", node->id, value);
		if (value & 0x0080 && node->p->transferActive == TRANSFER_IDLE) {
			if (!node->id && node->d.p->multiplayerControl.ready) {
				mLOG(GBA_SIO, DEBUG, "Lockstep %i: Transfer initiated", node->id);
				node->p->transferActive = TRANSFER_STARTING;
				node->p->transferCycles = GBASIOCyclesPerTransfer[node->d.p->multiplayerControl.baud][node->p->attached - 1];
				node->nextEvent = 0;
			} else {
				value &= ~0x0080;
			}
		}
		value &= 0xFF83;
		value |= driver->p->siocnt & 0x00FC;
	} else if (address == REG_SIOMLT_SEND) {
		mLOG(GBA_SIO, DEBUG, "Lockstep %i: SIOMLT_SEND <- %04x", node->id, value);
	}
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
		sio->multiplayerControl.busy = 0;
		sio->multiplayerControl.id = node->id;
		if (sio->multiplayerControl.irq) {
			GBARaiseIRQ(sio->p, IRQ_SIO);
		}
		break;
	case SIO_NORMAL_8:
	case SIO_NORMAL_32:
		// TODO
		sio->normalControl.start = 0;
		if (sio->multiplayerControl.irq) {
			GBARaiseIRQ(sio->p, IRQ_SIO);
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

static void _masterUpdate(struct GBASIOLockstepNode* node) {
	ATOMIC_STORE(node->needsToWait, false);
	int i;
	switch (node->p->transferActive) {
	case TRANSFER_IDLE:
		// If the master hasn't initiated a transfer, it can keep going.
		node->nextEvent += LOCKSTEP_INCREMENT;
		node->d.p->multiplayerControl.ready = node->p->attachedMulti == node->p->attached;
		break;
	case TRANSFER_STARTING:
		// Start the transfer, but wait for the other GBAs to catch up
		node->transferFinished = false;
		node->p->multiRecv[0] = 0xFFFF;
		node->p->multiRecv[1] = 0xFFFF;
		node->p->multiRecv[2] = 0xFFFF;
		node->p->multiRecv[3] = 0xFFFF;
		ATOMIC_STORE(node->needsToWait, true);
		ATOMIC_STORE(node->p->transferActive, TRANSFER_STARTED);
		node->nextEvent += 512;
		break;
	case TRANSFER_STARTED:
		// All the other GBAs have caught up and are sleeping, we can all continue now
#ifndef NDEBUG
		/*for (i = 1; i < node->p->attached; ++i) {
			enum GBASIOLockstepPhase phase;
			ATOMIC_LOAD(phase, node->p->players[i]->phase);
			if (node->p->players[i]->mode == node->mode && phase != TRANSFER_STARTED) {
				abort();
			}
		}*/
#endif
		node->p->multiRecv[0] = node->d.p->p->memory.io[REG_SIOMLT_SEND >> 1];
		node->nextEvent += 512;
		ATOMIC_STORE(node->p->transferActive, TRANSFER_FINISHING);
		break;
	case TRANSFER_FINISHING:
		// Finish the transfer
		// We need to make sure the other GBAs catch up so they don't get behind
		node->nextEvent += node->p->transferCycles - 1024; // Split the cycles to avoid waiting too long
#ifndef NDEBUG
		ATOMIC_ADD(node->p->transferId, 1);
#endif
		ATOMIC_STORE(node->needsToWait, true);
		ATOMIC_STORE(node->p->transferActive, TRANSFER_FINISHED);
		break;
	case TRANSFER_FINISHED:
		// Everything's settled. We're done.
		_finishTransfer(node);
		node->nextEvent += LOCKSTEP_INCREMENT;
		ATOMIC_STORE(node->p->transferActive, TRANSFER_IDLE);
		break;
	}
	if (node->needsToWait) {
		int mask = 0;
		for (i = 1; i < node->p->attached; ++i) {
			if (node->p->players[i]->mode == node->mode) {
				mask |= 1 << i;
			}
		}
		if (mask) {
			_nodeWait(node, mask);
		}
	}
	// Tell the other GBAs they can continue up to where we were
	for (i = 1; i < node->p->attached; ++i) {
		ATOMIC_ADD(node->p->players[i]->nextEvent, node->eventDiff);
		ATOMIC_STORE(node->p->players[i]->needsToWait, false);
	}
#ifndef NDEBUG
	node->phase = node->p->transferActive;
#endif
	_nodeSignal(node, 1);
}

static void _slaveUpdate(struct GBASIOLockstepNode* node) {
	ATOMIC_STORE(node->needsToWait, true);
	node->d.p->multiplayerControl.ready = node->p->attachedMulti == node->p->attached;
#ifndef NDEBUG
	if (node->phase >= TRANSFER_STARTED && node->phase != TRANSFER_FINISHED && node->phase != node->p->transferActive && node->p->transferActive < TRANSFER_FINISHING) {
		//abort();
	}
	if (node->phase < TRANSFER_FINISHED && node->phase != TRANSFER_IDLE && node->p->transferActive == TRANSFER_IDLE) {
		//abort();
	}
#endif
	bool signal = false;
	switch (node->p->transferActive) {
	case TRANSFER_IDLE:
		if (!node->d.p->multiplayerControl.ready) {
			node->nextEvent += LOCKSTEP_INCREMENT;
			ATOMIC_STORE(node->needsToWait, false);
			return;
		}
		break;
	case TRANSFER_STARTING:
	case TRANSFER_FINISHING:
		break;
	case TRANSFER_STARTED:
#ifndef NDEBUG
		if (node->transferId != node->p->transferId) {
			//abort();
		}
#endif
		node->transferFinished = false;
		switch (node->mode) {
		case SIO_MULTI:
			node->d.p->rcnt &= ~1;
			node->p->multiRecv[node->id] = node->d.p->p->memory.io[REG_SIOMLT_SEND >> 1];
			node->d.p->p->memory.io[REG_SIOMULTI0 >> 1] = 0xFFFF;
			node->d.p->p->memory.io[REG_SIOMULTI1 >> 1] = 0xFFFF;
			node->d.p->p->memory.io[REG_SIOMULTI2 >> 1] = 0xFFFF;
			node->d.p->p->memory.io[REG_SIOMULTI3 >> 1] = 0xFFFF;
			node->d.p->multiplayerControl.busy = 1;
			break;
		default:
			node->p->multiRecv[node->id]= 0xFFFF;
			break;
		}
		signal = true;
		break;
	case TRANSFER_FINISHED:
		_finishTransfer(node);
		signal = true;
		break;
	}
	_nodeWait(node, 1);
#ifndef NDEBUG
	node->phase = node->p->transferActive;
#endif
	if (signal) {
		_nodeSignal(node, 1 << node->id);
	}
}

static int32_t GBASIOLockstepNodeProcessEvents(struct GBASIODriver* driver, int32_t cycles) {
	struct GBASIOLockstepNode* node = (struct GBASIOLockstepNode*) driver;
	if (node->p->attached < 2) {
		return INT_MAX;
	}
	node->eventDiff += cycles;
	cycles = ATOMIC_ADD(node->nextEvent, -cycles);
	if (cycles <= 0) {
		if (!node->id) {
			_masterUpdate(node);
		} else {
			_slaveUpdate(node);
		}
		node->eventDiff = 0;
		if (node->needsToWait) {
			return 0;
		}
		ATOMIC_LOAD(cycles, node->nextEvent);
#ifndef NDEBUG
		if (cycles <= 0 && !node->needsToWait) {
			abort();
			mLOG(GBA_SIO, WARN, "Sleeping needlessly");
		}
#endif
		return cycles;
	}
	return cycles;
}

static uint16_t GBASIOLockstepNodeNormalWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value) {
	struct GBASIOLockstepNode* node = (struct GBASIOLockstepNode*) driver;
	if (address == REG_SIOCNT) {
		mLOG(GBA_SIO, DEBUG, "Lockstep %i: SIOCNT <- %04x", node->id, value);
		value &= 0xFF8B;
		if (value & 0x0080) {
			// Internal shift clock
			if (value & 1) {
				node->p->transferActive = TRANSFER_STARTING;
			}
			// Frequency
			if (value & 2) {
				node->p->transferCycles = GBA_ARM7TDMI_FREQUENCY / 1024;
			} else {
				node->p->transferCycles = GBA_ARM7TDMI_FREQUENCY / 8192;
			}
			node->normalSO = !!(value & 8);
			// Opponent's SO
			if (node->id) {
				value |= node->p->players[node->id - 1]->normalSO << 2;
			}
		}
	} else if (address == REG_SIODATA32_LO) {
		mLOG(GBA_SIO, DEBUG, "Lockstep %i: SIODATA32_LO <- %04x", node->id, value);
	} else if (address == REG_SIODATA32_HI) {
		mLOG(GBA_SIO, DEBUG, "Lockstep %i: SIODATA32_HI <- %04x", node->id, value);
	}
	return value;
}
