/* Copyright (c) 2013-2024 Jeffrey Pfau
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
#include <mgba-util/circle-buffer.h>
#include <mgba-util/table.h>
#include <mgba-util/threading.h>

#define MAX_LOCKSTEP_EVENTS 8

enum GBASIOLockstepEventType {
	SIO_EV_ATTACH,
	SIO_EV_DETACH,
	SIO_EV_HARD_SYNC,
	SIO_EV_MODE_SET,
	SIO_EV_TRANSFER_START,
};

struct GBASIOLockstepCoordinator {
	struct Table players;
	Mutex mutex;

	unsigned nextId;

	unsigned attachedPlayers[MAX_GBAS];
	int nAttached;
	uint32_t waiting;

	bool transferActive;
	enum GBASIOMode transferMode;

	int32_t cycle;
	int32_t nextHardSync;

	uint16_t multiData[4];
	uint32_t normalData[4];
};

struct GBASIOLockstepEvent {
	enum GBASIOLockstepEventType type;
	int32_t timestamp;
	struct GBASIOLockstepEvent* next;
	int playerId;
	union {
		enum GBASIOMode mode;
		int32_t finishCycle;
	};
};

struct GBASIOLockstepPlayer {
	struct GBASIOLockstepDriver* driver;
	int playerId;
	enum GBASIOMode mode;
	enum GBASIOMode otherModes[MAX_GBAS];
	bool asleep;
	int32_t cycleOffset;
	struct GBASIOLockstepEvent* queue;
	bool dataReceived;

	struct GBASIOLockstepEvent buffer[MAX_LOCKSTEP_EVENTS];
	struct GBASIOLockstepEvent* freeList;
};

struct GBASIOLockstepDriver {
	struct GBASIODriver d;
	struct GBASIOLockstepCoordinator* coordinator;
	struct mTimingEvent event;
	unsigned lockstepId;

	struct mLockstepUser* user;
};

void GBASIOLockstepCoordinatorInit(struct GBASIOLockstepCoordinator*);
void GBASIOLockstepCoordinatorDeinit(struct GBASIOLockstepCoordinator*);

void GBASIOLockstepCoordinatorAttach(struct GBASIOLockstepCoordinator*, struct GBASIOLockstepDriver*);
void GBASIOLockstepCoordinatorDetach(struct GBASIOLockstepCoordinator*, struct GBASIOLockstepDriver*);
size_t GBASIOLockstepCoordinatorAttached(struct GBASIOLockstepCoordinator*);

void GBASIOLockstepDriverCreate(struct GBASIOLockstepDriver*, struct mLockstepUser*);

CXX_GUARD_END

#endif
