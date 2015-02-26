/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SIO_LOCKSTEP_H
#define SIO_LOCKSTEP_H

#include "gba/sio.h"

#include "util/threading.h"

struct GBASIOLockstep {
	struct GBASIOLockstepNode* players[MAX_GBAS];
	int attached;

	uint16_t data[MAX_GBAS];
	Condition barrier;
};

struct GBASIOLockstepNode {
	struct GBASIODriver d;
	struct GBASIOLockstep* p;
};

void GBASIOLockstepInit(struct GBASIOLockstep*);
void GBASIOLockstepDeinit(struct GBASIOLockstep*);

void GBASIOLockstepNodeCreate(struct GBASIOLockstepNode*);
bool GBASIOLockstepAttachNode(struct GBASIOLockstep*, struct GBASIOLockstepNode*);

#endif
