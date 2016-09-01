/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SIO_LOCKSTEP_H
#define SIO_LOCKSTEP_H

#include "gba/sio.h"

enum GBASIOLockstepPhase {
	TRANSFER_IDLE = 0,
	TRANSFER_STARTING,
	TRANSFER_STARTED,
	TRANSFER_FINISHING,
	TRANSFER_FINISHED
};

struct GBASIOLockstep {
	struct GBASIOLockstepNode* players[MAX_GBAS];
	int attached;
	int attachedMulti;
	int attachedNormal;

	uint16_t multiRecv[MAX_GBAS];
	uint32_t normalRecv[MAX_GBAS];
	enum GBASIOLockstepPhase transferActive;
	int32_t transferCycles;

	uint32_t masterWaiting;

	void (*signal)(struct GBASIOLockstep*, int id);
	void (*wait)(struct GBASIOLockstep*, int id);
	void* context;
#ifndef NDEBUG
	int transferId;
#endif
};

struct GBASIOLockstepNode {
	struct GBASIODriver d;
	struct GBASIOLockstep* p;

	volatile int32_t nextEvent;
	int32_t eventDiff;
	bool normalSO;
	int id;
	enum GBASIOMode mode;
	bool transferFinished;
#ifndef NDEBUG
	int transferId;
	enum GBASIOLockstepPhase phase;
#endif
};

void GBASIOLockstepInit(struct GBASIOLockstep*);
void GBASIOLockstepDeinit(struct GBASIOLockstep*);

void GBASIOLockstepNodeCreate(struct GBASIOLockstepNode*);

bool GBASIOLockstepAttachNode(struct GBASIOLockstep*, struct GBASIOLockstepNode*);
void GBASIOLockstepDetachNode(struct GBASIOLockstep*, struct GBASIOLockstepNode*);

#endif
