/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/lockstep.h>

void mLockstepInit(struct mLockstep* lockstep) {
	lockstep->attached = 0;
	lockstep->transferActive = 0;
#ifndef NDEBUG
	lockstep->transferId = 0;
#endif
	lockstep->lock = NULL;
	lockstep->unlock = NULL;
}

void mLockstepDeinit(struct mLockstep* lockstep) {
	UNUSED(lockstep);
}

// TODO: Migrate nodes
