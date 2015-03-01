/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "lockstep.h"

#include "gba/gba.h"
#include "gba/io.h"

#define LOCKSTEP_INCREMENT 2048

static bool GBASIOLockstepNodeInit(struct GBASIODriver* driver);
static void GBASIOLockstepNodeDeinit(struct GBASIODriver* driver);
static bool GBASIOLockstepNodeLoad(struct GBASIODriver* driver);
static bool GBASIOLockstepNodeUnload(struct GBASIODriver* driver);
static uint16_t GBASIOLockstepNodeWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value);
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
	lockstep->loaded = 0;
	lockstep->transferActive = false;
	lockstep->waiting = 0;
	lockstep->nextEvent = LOCKSTEP_INCREMENT;
	ConditionInit(&lockstep->barrier);
	MutexInit(&lockstep->mutex);
}

void GBASIOLockstepDeinit(struct GBASIOLockstep* lockstep) {
	ConditionDeinit(&lockstep->barrier);
	MutexDeinit(&lockstep->mutex);
}

void GBASIOLockstepNodeCreate(struct GBASIOLockstepNode* node) {
	node->d.init = GBASIOLockstepNodeInit;
	node->d.deinit = GBASIOLockstepNodeDeinit;
	node->d.load = GBASIOLockstepNodeLoad;
	node->d.unload = GBASIOLockstepNodeUnload;
	node->d.writeRegister = GBASIOLockstepNodeWriteRegister;
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
	node->nextEvent = LOCKSTEP_INCREMENT;
	node->d.p->multiplayerControl.slave = node->id > 0;
	return true;
}

void GBASIOLockstepNodeDeinit(struct GBASIODriver* driver) {
	UNUSED(driver);
}

bool GBASIOLockstepNodeLoad(struct GBASIODriver* driver) {
	struct GBASIOLockstepNode* node = (struct GBASIOLockstepNode*) driver;
	node->state = LOCKSTEP_IDLE;
	MutexLock(&node->p->mutex);
	++node->p->loaded;
	node->d.p->rcnt |= 3;
	if (node->id) {
		node->d.p->rcnt |= 4;
		node->d.p->multiplayerControl.slave = 1;
	}
	MutexUnlock(&node->p->mutex);
	return true;
}

bool GBASIOLockstepNodeUnload(struct GBASIODriver* driver) {
	struct GBASIOLockstepNode* node = (struct GBASIOLockstepNode*) driver;
	MutexLock(&node->p->mutex);
	--node->p->loaded;
	ConditionWake(&node->p->barrier);
	MutexUnlock(&node->p->mutex);
	return true;
}

static uint16_t GBASIOLockstepNodeWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value) {
	struct GBASIOLockstepNode* node = (struct GBASIOLockstepNode*) driver;
	if (address == REG_SIOCNT) {
		if (value & 0x0080) {
			value &= ~0x0080;
			if (!node->id) {
				MutexLock(&node->p->mutex);
				node->p->transferActive = true;
				node->p->transferCycles = GBASIOCyclesPerTransfer[node->d.p->multiplayerControl.baud][node->p->attached - 1];
				MutexUnlock(&node->p->mutex);
			}
		}
		value &= 0xFF03;
		value |= driver->p->siocnt & 0x007C;
	}
	return value;
}

static int32_t GBASIOLockstepNodeProcessEvents(struct GBASIODriver* driver, int32_t cycles) {
	struct GBASIOLockstepNode* node = (struct GBASIOLockstepNode*) driver;
	node->nextEvent -= cycles;
	while (node->nextEvent <= 0) {
		MutexLock(&node->p->mutex);
		++node->p->waiting;
		if (node->p->waiting < node->p->loaded) {
			ConditionWait(&node->p->barrier, &node->p->mutex);
		} else {
			if (node->p->transferActive) {
				node->p->transferCycles -= node->p->nextEvent;
				if (node->p->transferCycles > 0) {
					if (node->p->transferCycles < LOCKSTEP_INCREMENT) {
						node->p->nextEvent = node->p->transferCycles;
					}
				} else {
					node->p->nextEvent = LOCKSTEP_INCREMENT;
					node->p->transferActive = false;
					int i;
					for (i = 0; i < node->p->attached; ++i) {
						node->p->multiRecv[i] = node->p->players[i]->multiSend;
						node->p->players[i]->state = LOCKSTEP_FINISHED;
					}
					for (; i < MAX_GBAS; ++i) {
						node->p->multiRecv[i] = 0xFFFF;
					}
				}
			}
			ConditionWake(&node->p->barrier);
		}
		--node->p->waiting;
		if (node->state == LOCKSTEP_FINISHED) {
			node->d.p->p->memory.io[REG_SIOMULTI0 >> 1] = node->p->multiRecv[0];
			node->d.p->p->memory.io[REG_SIOMULTI1 >> 1] = node->p->multiRecv[1];
			node->d.p->p->memory.io[REG_SIOMULTI2 >> 1] = node->p->multiRecv[2];
			node->d.p->p->memory.io[REG_SIOMULTI3 >> 1] = node->p->multiRecv[3];
			node->d.p->rcnt |= 1;
			node->state = LOCKSTEP_IDLE;
			if (node->d.p->multiplayerControl.irq) {
				GBARaiseIRQ(node->d.p->p, IRQ_SIO);
			}
			node->d.p->multiplayerControl.id = node->id;
			node->d.p->multiplayerControl.busy = 0;
		} else if (node->state == LOCKSTEP_IDLE && node->p->transferActive) {
			node->state = LOCKSTEP_STARTED;
			node->d.p->p->memory.io[REG_SIOMULTI0 >> 1] = 0xFFFF;
			node->d.p->p->memory.io[REG_SIOMULTI1 >> 1] = 0xFFFF;
			node->d.p->p->memory.io[REG_SIOMULTI2 >> 1] = 0xFFFF;
			node->d.p->p->memory.io[REG_SIOMULTI3 >> 1] = 0xFFFF;
			node->d.p->rcnt &= ~1;
			node->multiSend = node->d.p->p->memory.io[REG_SIOMLT_SEND >> 1];
			if (node->id) {
				node->d.p->multiplayerControl.busy = 1;
			}
		}
		node->d.p->multiplayerControl.ready = node->p->loaded == node->p->attached;
		node->nextEvent += node->p->nextEvent;
		MutexUnlock(&node->p->mutex);
	}
	return node->nextEvent;
}
