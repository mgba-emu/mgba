/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_SIO_LOCKSTEP_H
#define GB_SIO_LOCKSTEP_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/lockstep.h>
#include <mgba/core/timing.h>
#include <mgba/internal/gb/sio.h>

struct GBSIOLockstep {
	struct mLockstep d;
	struct GBSIOLockstepNode* players[MAX_GBS];

	uint8_t pendingSB[MAX_GBS];
	bool masterClaimed;
};

struct GBSIOLockstepNode {
	struct GBSIODriver d;
	struct GBSIOLockstep* p;
	struct mTimingEvent event;

	volatile int32_t nextEvent;
	int32_t eventDiff;
	int id;
	bool transferFinished;
#ifndef NDEBUG
	int transferId;
	enum mLockstepPhase phase;
#endif
};

void GBSIOLockstepInit(struct GBSIOLockstep*);

void GBSIOLockstepNodeCreate(struct GBSIOLockstepNode*);

bool GBSIOLockstepAttachNode(struct GBSIOLockstep*, struct GBSIOLockstepNode*);
void GBSIOLockstepDetachNode(struct GBSIOLockstep*, struct GBSIOLockstepNode*);

CXX_GUARD_END

#endif
