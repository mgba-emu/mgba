/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SIO_LOCKSTEP_H
#define SIO_LOCKSTEP_H

#include "gba/sio.h"

#include "util/threading.h"

enum LockstepState {
	LOCKSTEP_IDLE = 0,
	LOCKSTEP_STARTED = 1,
	LOCKSTEP_FINISHED = 2
};

struct GBASIOLockstep {
	struct GBASIOLockstepNode* players[MAX_GBAS];
	int attached;
	int loadedMulti;
	int loadedNormal;

	uint16_t multiRecv[MAX_GBAS];
	bool transferActive;
	int32_t transferCycles;
	int32_t nextEvent;

	int waiting;
	Mutex mutex;
	Condition barrier;
};

struct GBASIOLockstepNode {
	struct GBASIODriver d;
	struct GBASIOLockstep* p;

	int32_t nextEvent;
	uint16_t multiSend;
	bool normalSO;
	enum LockstepState state;
	int id;
	enum GBASIOMode mode;
};

void GBASIOLockstepInit(struct GBASIOLockstep*);
void GBASIOLockstepDeinit(struct GBASIOLockstep*);

void GBASIOLockstepNodeCreate(struct GBASIOLockstepNode*);

bool GBASIOLockstepAttachNode(struct GBASIOLockstep*, struct GBASIOLockstepNode*);
void GBASIOLockstepDetachNode(struct GBASIOLockstep*, struct GBASIOLockstepNode*);

#endif
