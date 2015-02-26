/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "lockstep.h"

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
	lockstep->attached = 0;
	ConditionInit(&lockstep->barrier);
}

void GBASIOLockstepDeinit(struct GBASIOLockstep* lockstep) {
	ConditionDeinit(&lockstep->barrier);
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
	++lockstep->attached;
	return true;
}

bool GBASIOLockstepNodeInit(struct GBASIODriver* driver) {
	UNUSED(driver);
	return true;
}

void GBASIOLockstepNodeDeinit(struct GBASIODriver* driver) {
	UNUSED(driver);
}

bool GBASIOLockstepNodeLoad(struct GBASIODriver* driver) {
	UNUSED(driver);
	return true;
}

bool GBASIOLockstepNodeUnload(struct GBASIODriver* driver) {
	UNUSED(driver);
	return true;
}

static uint16_t GBASIOLockstepNodeWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value) {
	return value;
}

static int32_t GBASIOLockstepNodeProcessEvents(struct GBASIODriver* driver, int32_t cycles) {
	return INT_MAX;
}
