/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_SIO_LOCKSTEP_H
#define GBA_SIO_LOCKSTEP_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/lockstep.h>
#include <mgba/core/timing.h>
#include <mgba/internal/gba/sio.h>

struct GBASIOLockstep {
	struct mLockstep d;
	struct GBASIOLockstepNode* players[MAX_GBAS];
	int attachedMulti;
	int attachedNormal;

	uint16_t multiRecv[MAX_GBAS];
	uint32_t normalRecv[MAX_GBAS];
};

struct GBASIOLockstepNode {
	struct GBASIODriver d;
	struct GBASIOLockstep* p;
	struct mTimingEvent event;

	volatile int32_t nextEvent;
	int32_t eventDiff;
	bool normalSO;
	int id;
	enum GBASIOMode mode;
	bool transferFinished;
#ifndef NDEBUG
	int transferId;
	enum mLockstepPhase phase;
#endif
};

void GBASIOLockstepInit(struct GBASIOLockstep*);

void GBASIOLockstepNodeCreate(struct GBASIOLockstepNode*);

bool GBASIOLockstepAttachNode(struct GBASIOLockstep*, struct GBASIOLockstepNode*);
void GBASIOLockstepDetachNode(struct GBASIOLockstep*, struct GBASIOLockstepNode*);

CXX_GUARD_END

#endif
