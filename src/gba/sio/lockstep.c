/* Copyright (c) 2013-2024 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/sio/lockstep.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>

#define DRIVER_ID 0x6B636F4C
#define DRIVER_STATE_VERSION 1
#define LOCKSTEP_INTERVAL 4096
#define UNLOCKED_INTERVAL 4096
#define HARD_SYNC_INTERVAL 0x80000
#define TARGET(P) (1 << (P))
#define TARGET_ALL 0xF
#define TARGET_PRIMARY 0x1
#define TARGET_SECONDARY ((TARGET_ALL) & ~(TARGET_PRIMARY))

DECL_BITFIELD(GBASIOLockstepSerializedFlags, uint32_t);
DECL_BITS(GBASIOLockstepSerializedFlags, DriverMode, 0, 3);
DECL_BITS(GBASIOLockstepSerializedFlags, NumEvents, 3, 4);
DECL_BIT(GBASIOLockstepSerializedFlags, Asleep, 7);
DECL_BIT(GBASIOLockstepSerializedFlags, DataReceived, 8);
DECL_BIT(GBASIOLockstepSerializedFlags, EventScheduled, 9);
DECL_BITS(GBASIOLockstepSerializedFlags, Player0Mode, 10, 3);
DECL_BITS(GBASIOLockstepSerializedFlags, Player1Mode, 13, 3);
DECL_BITS(GBASIOLockstepSerializedFlags, Player2Mode, 16, 3);
DECL_BITS(GBASIOLockstepSerializedFlags, Player3Mode, 19, 3);
DECL_BITS(GBASIOLockstepSerializedFlags, TransferMode, 28, 3);
DECL_BIT(GBASIOLockstepSerializedFlags, TransferActive, 31);

DECL_BITFIELD(GBASIOLockstepSerializedEventFlags, uint32_t);
DECL_BITS(GBASIOLockstepSerializedEventFlags, Type, 0, 3);

struct GBASIOLockstepSerializedEvent {
	int32_t timestamp;
	int32_t playerId;
	GBASIOLockstepSerializedEventFlags flags;
	int32_t reserved[5];
	union {
		int32_t mode;
		int32_t finishCycle;
		int32_t padding[4];
	};
};
static_assert(sizeof(struct GBASIOLockstepSerializedEvent) == 0x30, "GBA lockstep event savestate struct sized wrong");

struct GBASIOLockstepSerializedState {
	uint32_t version;
	GBASIOLockstepSerializedFlags flags;
	uint32_t reserved[2];

	struct {
		int32_t nextEvent;
		uint32_t reservedDriver[7];
	} driver;

	struct {
		int32_t playerId;
		int32_t cycleOffset;
		uint32_t reservedPlayer[2];
		struct GBASIOLockstepSerializedEvent events[MAX_LOCKSTEP_EVENTS];
	} player;

	// playerId 0 only
	struct {
		int32_t cycle;
		uint32_t waiting;
		int32_t nextHardSync;
		uint32_t reservedCoordinator[3];
		uint16_t multiData[4];
		uint32_t normalData[4];
	} coordinator;
};
static_assert(offsetof(struct GBASIOLockstepSerializedState, driver) == 0x10, "GBA lockstep savestate driver offset wrong");
static_assert(offsetof(struct GBASIOLockstepSerializedState, player) == 0x30, "GBA lockstep savestate player offset wrong");
static_assert(offsetof(struct GBASIOLockstepSerializedState, coordinator) == 0x1C0, "GBA lockstep savestate coordinator offset wrong");
static_assert(sizeof(struct GBASIOLockstepSerializedState) == 0x1F0, "GBA lockstep savestate struct sized wrong");

static bool GBASIOLockstepDriverInit(struct GBASIODriver* driver);
static void GBASIOLockstepDriverDeinit(struct GBASIODriver* driver);
static void GBASIOLockstepDriverReset(struct GBASIODriver* driver);
static uint32_t GBASIOLockstepDriverId(const struct GBASIODriver* driver);
static bool GBASIOLockstepDriverLoadState(struct GBASIODriver* driver, const void* state, size_t size);
static void GBASIOLockstepDriverSaveState(struct GBASIODriver* driver, void** state, size_t* size);
static void GBASIOLockstepDriverSetMode(struct GBASIODriver* driver, enum GBASIOMode mode);
static bool GBASIOLockstepDriverHandlesMode(struct GBASIODriver* driver, enum GBASIOMode mode);
static int GBASIOLockstepDriverConnectedDevices(struct GBASIODriver* driver);
static int GBASIOLockstepDriverDeviceId(struct GBASIODriver* driver);
static uint16_t GBASIOLockstepDriverWriteSIOCNT(struct GBASIODriver* driver, uint16_t value);
static uint16_t GBASIOLockstepDriverWriteRCNT(struct GBASIODriver* driver, uint16_t value);
static bool GBASIOLockstepDriverStart(struct GBASIODriver* driver);
static void GBASIOLockstepDriverFinishMultiplayer(struct GBASIODriver* driver, uint16_t data[4]);
static uint8_t GBASIOLockstepDriverFinishNormal8(struct GBASIODriver* driver);
static uint32_t GBASIOLockstepDriverFinishNormal32(struct GBASIODriver* driver);

static void GBASIOLockstepCoordinatorWaitOnPlayers(struct GBASIOLockstepCoordinator*, struct GBASIOLockstepPlayer*);
static void GBASIOLockstepCoordinatorAckPlayer(struct GBASIOLockstepCoordinator*, struct GBASIOLockstepPlayer*);
static void GBASIOLockstepCoordinatorWakePlayers(struct GBASIOLockstepCoordinator*);

static int32_t GBASIOLockstepTime(struct GBASIOLockstepPlayer*);
static void GBASIOLockstepPlayerWake(struct GBASIOLockstepPlayer*);
static void GBASIOLockstepPlayerSleep(struct GBASIOLockstepPlayer*);

static void _advanceCycle(struct GBASIOLockstepCoordinator*, struct GBASIOLockstepPlayer*);
static void _removePlayer(struct GBASIOLockstepCoordinator*, struct GBASIOLockstepPlayer*);
static void _reconfigPlayers(struct GBASIOLockstepCoordinator*);
static int32_t _untilNextSync(struct GBASIOLockstepCoordinator*, struct GBASIOLockstepPlayer*);
static void _enqueueEvent(struct GBASIOLockstepCoordinator*, const struct GBASIOLockstepEvent*, uint32_t target);
static void _setData(struct GBASIOLockstepCoordinator*, uint32_t id, struct GBASIO* sio);
static void _setReady(struct GBASIOLockstepCoordinator*, struct GBASIOLockstepPlayer* activePlayer, int playerId, enum GBASIOMode mode);
static void _hardSync(struct GBASIOLockstepCoordinator*, struct GBASIOLockstepPlayer*);

static void _lockstepEvent(struct mTiming*, void* context, uint32_t cyclesLate);

static void _verifyAwake(struct GBASIOLockstepCoordinator* coordinator) {
#ifdef NDEBUG
	UNUSED(coordinator);
#else
	int i;
	int asleep = 0;
	for (i = 0; i < coordinator->nAttached; ++i) {
		if (!coordinator->attachedPlayers[i]) {
			continue;
		}
		struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, coordinator->attachedPlayers[i]);
		asleep += player->asleep;
	}
	mASSERT_DEBUG(!asleep || asleep < coordinator->nAttached);
#endif
}

void GBASIOLockstepDriverCreate(struct GBASIOLockstepDriver* driver, struct mLockstepUser* user) {
	memset(driver, 0, sizeof(*driver));
	driver->d.init = GBASIOLockstepDriverInit;
	driver->d.deinit = GBASIOLockstepDriverDeinit;
	driver->d.reset = GBASIOLockstepDriverReset;
	driver->d.driverId = GBASIOLockstepDriverId;
	driver->d.loadState = GBASIOLockstepDriverLoadState;
	driver->d.saveState = GBASIOLockstepDriverSaveState;
	driver->d.setMode = GBASIOLockstepDriverSetMode;
	driver->d.handlesMode = GBASIOLockstepDriverHandlesMode;
	driver->d.deviceId = GBASIOLockstepDriverDeviceId;
	driver->d.connectedDevices = GBASIOLockstepDriverConnectedDevices;
	driver->d.writeSIOCNT = GBASIOLockstepDriverWriteSIOCNT;
	driver->d.writeRCNT = GBASIOLockstepDriverWriteRCNT;
	driver->d.start = GBASIOLockstepDriverStart;
	driver->d.finishMultiplayer = GBASIOLockstepDriverFinishMultiplayer;
	driver->d.finishNormal8 = GBASIOLockstepDriverFinishNormal8;
	driver->d.finishNormal32 = GBASIOLockstepDriverFinishNormal32;
	driver->event.context = driver;
	driver->event.callback = _lockstepEvent;
	driver->event.name = "GBA SIO Lockstep";
	driver->event.priority = 0x80;
	driver->user = user;
}

static bool GBASIOLockstepDriverInit(struct GBASIODriver* driver) {
	GBASIOLockstepDriverReset(driver);
	return true;
}

static void GBASIOLockstepDriverDeinit(struct GBASIODriver* driver) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	MutexLock(&coordinator->mutex);
	struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, lockstep->lockstepId);
	if (player) {
		_removePlayer(coordinator, player);
	}
	MutexUnlock(&coordinator->mutex);
	mTimingDeschedule(&lockstep->d.p->p->timing, &lockstep->event);
	lockstep->lockstepId = 0;
}

static void GBASIOLockstepDriverReset(struct GBASIODriver* driver) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	struct GBASIOLockstepPlayer* player;
	if (!lockstep->lockstepId) {
		unsigned id;
		player = calloc(1, sizeof(*player));
		player->driver = lockstep;
		player->mode = driver->p->mode;
		player->playerId = -1;

		int i;
		for (i = 0; i < MAX_LOCKSTEP_EVENTS - 1; ++i) {
			player->buffer[i].next = &player->buffer[i + 1];
		}
		player->freeList = &player->buffer[0];

		MutexLock(&coordinator->mutex);
		while (true) {
			if (coordinator->nextId == UINT_MAX) {
				coordinator->nextId = 0;
			}
			++coordinator->nextId;
			id = coordinator->nextId;
			if (!TableLookup(&coordinator->players, id)) {
				TableInsert(&coordinator->players, id, player);
				lockstep->lockstepId = id;
				break;
			}
		}
		_reconfigPlayers(coordinator);
		player->cycleOffset = mTimingCurrentTime(&driver->p->p->timing) - coordinator->cycle;
		if (player->playerId != 0) {
			struct GBASIOLockstepEvent event = {
				.type = SIO_EV_ATTACH,
				.playerId = player->playerId,
				.timestamp = GBASIOLockstepTime(player),
			};
			_enqueueEvent(coordinator, &event, TARGET_ALL & ~TARGET(player->playerId));
		}
	} else {
		player = TableLookup(&coordinator->players, lockstep->lockstepId);
		player->cycleOffset = mTimingCurrentTime(&driver->p->p->timing) - coordinator->cycle;
	}

	if (mTimingIsScheduled(&lockstep->d.p->p->timing, &lockstep->event)) {
		MutexUnlock(&coordinator->mutex);
		return;
	}

	int32_t nextEvent;
	_setReady(coordinator, player, player->playerId, player->mode);
	if (TableSize(&coordinator->players) == 1) {
		coordinator->cycle = mTimingCurrentTime(&lockstep->d.p->p->timing);
		nextEvent = LOCKSTEP_INTERVAL;
	} else {
		_setReady(coordinator, player, 0, coordinator->transferMode);
		nextEvent = _untilNextSync(lockstep->coordinator, player);
	}
	MutexUnlock(&coordinator->mutex);
	mTimingSchedule(&lockstep->d.p->p->timing, &lockstep->event, nextEvent);
}

static uint32_t GBASIOLockstepDriverId(const struct GBASIODriver* driver) {
	UNUSED(driver);
	return DRIVER_ID;
}

static unsigned _modeEnumToInt(enum GBASIOMode mode) {
	switch ((int) mode) {
	case -1:
	default:
		return 0;
	case GBA_SIO_MULTI:
		return 1;
	case GBA_SIO_NORMAL_8:
		return 2;
	case GBA_SIO_NORMAL_32:
		return 3;
	case GBA_SIO_GPIO:
		return 4;
	case GBA_SIO_UART:
		return 5;
	case GBA_SIO_JOYBUS:
		return 6;
	}
}

static enum GBASIOMode _modeIntToEnum(unsigned mode) {
	const enum GBASIOMode modes[8] = {
		-1, GBA_SIO_MULTI, GBA_SIO_NORMAL_8, GBA_SIO_NORMAL_32, GBA_SIO_GPIO, GBA_SIO_UART, GBA_SIO_JOYBUS, -1
	};
	return modes[mode & 7];
}

static bool GBASIOLockstepDriverLoadState(struct GBASIODriver* driver, const void* data, size_t size) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	if (size != sizeof(struct GBASIOLockstepSerializedState)) {
		mLOG(GBA_SIO, WARN, "Incorrect state size: expected %" PRIz "X, got %" PRIz "X", sizeof(struct GBASIOLockstepSerializedState), size);
		return false;
	}
	const struct GBASIOLockstepSerializedState* state = data;
	bool error = false;
	uint32_t ucheck;
	int32_t check;
	LOAD_32LE(ucheck, 0, &state->version);
	if (ucheck > DRIVER_STATE_VERSION) {
		mLOG(GBA_SIO, WARN, "Invalid or too new save state: expected %u, got %u", DRIVER_STATE_VERSION, ucheck);
		return false;
	}

	MutexLock(&coordinator->mutex);
	struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, lockstep->lockstepId);
	LOAD_32LE(check, 0, &state->player.playerId);
	if (check != player->playerId) {
		mLOG(GBA_SIO, WARN, "State is for different player: expected %d, got %d", player->playerId, check);
		error = true;
		goto out;
	}

	GBASIOLockstepSerializedFlags flags = 0;
	LOAD_32LE(flags, 0, &state->flags);
	LOAD_32LE(player->cycleOffset, 0, &state->player.cycleOffset);
	player->dataReceived = GBASIOLockstepSerializedFlagsGetDataReceived(flags);
	player->mode = _modeIntToEnum(GBASIOLockstepSerializedFlagsGetDriverMode(flags));

	player->otherModes[0] = _modeIntToEnum(GBASIOLockstepSerializedFlagsGetPlayer0Mode(flags));
	player->otherModes[1] = _modeIntToEnum(GBASIOLockstepSerializedFlagsGetPlayer1Mode(flags));
	player->otherModes[2] = _modeIntToEnum(GBASIOLockstepSerializedFlagsGetPlayer2Mode(flags));
	player->otherModes[3] = _modeIntToEnum(GBASIOLockstepSerializedFlagsGetPlayer3Mode(flags));

	if (GBASIOLockstepSerializedFlagsGetEventScheduled(flags)) {
		int32_t when;
		LOAD_32LE(when, 0, &state->driver.nextEvent);
		mTimingSchedule(&driver->p->p->timing, &lockstep->event, when);
	}

	if (GBASIOLockstepSerializedFlagsGetAsleep(flags)) {
		if (!player->asleep && player->driver->user->sleep) {
			player->driver->user->sleep(player->driver->user);
		}
		player->asleep = true;
	} else {
		if (player->asleep && player->driver->user->wake) {
			player->driver->user->wake(player->driver->user);
		}
		player->asleep = false;
	}

	unsigned i;
	for (i = 0; i < MAX_LOCKSTEP_EVENTS - 1; ++i) {
		player->buffer[i].next = &player->buffer[i + 1];
	}
	player->freeList = &player->buffer[0];
	player->queue = NULL;

	struct GBASIOLockstepEvent** lastEvent = &player->queue;
	for (i = 0; i < GBASIOLockstepSerializedFlagsGetNumEvents(flags) && i < MAX_LOCKSTEP_EVENTS; ++i) {
		struct GBASIOLockstepEvent* event = player->freeList;
		const struct GBASIOLockstepSerializedEvent* stateEvent = &state->player.events[i];
		player->freeList = player->freeList->next;
		*lastEvent = event;
		lastEvent = &event->next;

		GBASIOLockstepSerializedEventFlags flags;
		LOAD_32LE(flags, 0, &stateEvent->flags);
		LOAD_32LE(event->timestamp, 0, &stateEvent->timestamp);
		LOAD_32LE(event->playerId, 0, &stateEvent->playerId);
		event->type = GBASIOLockstepSerializedEventFlagsGetType(flags);
		switch (event->type) {
		case SIO_EV_ATTACH:
		case SIO_EV_DETACH:
		case SIO_EV_HARD_SYNC:
			break;
		case SIO_EV_MODE_SET:
			LOAD_32LE(event->mode, 0, &stateEvent->mode);
			break;
		case SIO_EV_TRANSFER_START:
			LOAD_32LE(event->finishCycle, 0, &stateEvent->finishCycle);
			break;
		}
	}

	if (player->playerId == 0) {
		LOAD_32LE(coordinator->cycle, 0, &state->coordinator.cycle);
		LOAD_32LE(coordinator->waiting, 0, &state->coordinator.waiting);
		LOAD_32LE(coordinator->nextHardSync, 0, &state->coordinator.nextHardSync);
		for (i = 0; i < 4; ++i) {
			LOAD_16LE(coordinator->multiData[i], 0, &state->coordinator.multiData[i]);
			LOAD_32LE(coordinator->normalData[i], 0, &state->coordinator.normalData[i]);
		}
		coordinator->transferMode = _modeIntToEnum(GBASIOLockstepSerializedFlagsGetTransferMode(flags));
		coordinator->transferActive = GBASIOLockstepSerializedFlagsGetTransferActive(flags);
	}
out:
	MutexUnlock(&coordinator->mutex);
	if (!error) {
		mTimingInterrupt(&driver->p->p->timing);
	}
	return !error;
}

static void GBASIOLockstepDriverSaveState(struct GBASIODriver* driver, void** stateOut, size_t* size) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	struct GBASIOLockstepSerializedState* state = calloc(1, sizeof(*state));

	STORE_32LE(DRIVER_STATE_VERSION, 0, &state->version);

	STORE_32LE(lockstep->event.when - mTimingCurrentTime(&driver->p->p->timing), 0, &state->driver.nextEvent);

	MutexLock(&coordinator->mutex);
	struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, lockstep->lockstepId);
	GBASIOLockstepSerializedFlags flags = 0;
	STORE_32LE(player->playerId, 0, &state->player.playerId);
	STORE_32LE(player->cycleOffset, 0, &state->player.cycleOffset);
	flags = GBASIOLockstepSerializedFlagsSetAsleep(flags, player->asleep);
	flags = GBASIOLockstepSerializedFlagsSetDataReceived(flags, player->dataReceived);
	flags = GBASIOLockstepSerializedFlagsSetDriverMode(flags, _modeEnumToInt(player->mode));
	flags = GBASIOLockstepSerializedFlagsSetEventScheduled(flags, mTimingIsScheduled(&driver->p->p->timing, &lockstep->event));

	flags = GBASIOLockstepSerializedFlagsSetPlayer0Mode(flags, _modeEnumToInt(player->otherModes[0]));
	flags = GBASIOLockstepSerializedFlagsSetPlayer1Mode(flags, _modeEnumToInt(player->otherModes[1]));
	flags = GBASIOLockstepSerializedFlagsSetPlayer2Mode(flags, _modeEnumToInt(player->otherModes[2]));
	flags = GBASIOLockstepSerializedFlagsSetPlayer3Mode(flags, _modeEnumToInt(player->otherModes[3]));

	struct GBASIOLockstepEvent* event = player->queue;
	size_t i;
	for (i = 0; i < MAX_LOCKSTEP_EVENTS && event; ++i, event = event->next) {
		struct GBASIOLockstepSerializedEvent* stateEvent = &state->player.events[i];
		GBASIOLockstepSerializedEventFlags flags = GBASIOLockstepSerializedEventFlagsSetType(0, event->type);
		STORE_32LE(event->timestamp, 0, &stateEvent->timestamp);
		STORE_32LE(event->playerId, 0, &stateEvent->playerId);
		switch (event->type) {
		case SIO_EV_ATTACH:
		case SIO_EV_DETACH:
		case SIO_EV_HARD_SYNC:
			break;
		case SIO_EV_MODE_SET:
			STORE_32LE(event->mode, 0, &stateEvent->mode);
			break;
		case SIO_EV_TRANSFER_START:
			STORE_32LE(event->finishCycle, 0, &stateEvent->finishCycle);
			break;
		}
		STORE_32LE(flags, 0, &stateEvent->flags);
	}
	flags = GBASIOLockstepSerializedFlagsSetNumEvents(flags, i);

	if (player->playerId == 0) {
		STORE_32LE(coordinator->cycle, 0, &state->coordinator.cycle);
		STORE_32LE(coordinator->waiting, 0, &state->coordinator.waiting);
		STORE_32LE(coordinator->nextHardSync, 0, &state->coordinator.nextHardSync);
		for (i = 0; i < 4; ++i) {
			STORE_16LE(coordinator->multiData[i], 0, &state->coordinator.multiData[i]);
			STORE_32LE(coordinator->normalData[i], 0, &state->coordinator.normalData[i]);
		}
		flags = GBASIOLockstepSerializedFlagsSetTransferMode(flags, _modeEnumToInt(coordinator->transferMode));
		flags = GBASIOLockstepSerializedFlagsSetTransferActive(flags, coordinator->transferActive);
	}
	MutexUnlock(&lockstep->coordinator->mutex);

	STORE_32LE(flags, 0, &state->flags);
	*stateOut = state;
	*size = sizeof(*state);
}

static void GBASIOLockstepDriverSetMode(struct GBASIODriver* driver, enum GBASIOMode mode) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	MutexLock(&coordinator->mutex);
	struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, lockstep->lockstepId);
	if (mode != player->mode) {
		player->mode = mode;
		struct GBASIOLockstepEvent event = {
			.type = SIO_EV_MODE_SET,
			.playerId = player->playerId,
			.timestamp = GBASIOLockstepTime(player),
			.mode = mode,
		};
		if (player->playerId == 0) {
			mASSERT_DEBUG(!coordinator->transferActive); // TODO
			coordinator->transferMode = mode;
			GBASIOLockstepCoordinatorWaitOnPlayers(coordinator, player);
		}
		_setReady(coordinator, player, player->playerId, mode);
		_enqueueEvent(coordinator, &event, TARGET_ALL & ~TARGET(player->playerId));
	}
	MutexUnlock(&coordinator->mutex);
}

static bool GBASIOLockstepDriverHandlesMode(struct GBASIODriver* driver, enum GBASIOMode mode) {
	UNUSED(driver);
	UNUSED(mode);
	return true;
}

static int GBASIOLockstepDriverConnectedDevices(struct GBASIODriver* driver) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	if (!lockstep->lockstepId) {
		return 0;
	}
	MutexLock(&coordinator->mutex);
	int attached = coordinator->nAttached - 1;
	MutexUnlock(&coordinator->mutex);
	return attached;
}

static int GBASIOLockstepDriverDeviceId(struct GBASIODriver* driver) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	int playerId = 0;
	MutexLock(&coordinator->mutex);
	struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, lockstep->lockstepId);
	if (player && player->playerId >= 0) {
		playerId = player->playerId;
	}
	MutexUnlock(&coordinator->mutex);
	return playerId;
}

static uint16_t GBASIOLockstepDriverWriteSIOCNT(struct GBASIODriver* driver, uint16_t value) {
	UNUSED(driver);
	mLOG(GBA_SIO, DEBUG, "Lockstep: SIOCNT <- %04X", value);
	return value;
}

static uint16_t GBASIOLockstepDriverWriteRCNT(struct GBASIODriver* driver, uint16_t value) {
	UNUSED(driver);
	mLOG(GBA_SIO, DEBUG, "Lockstep: RCNT <- %04X", value);
	return value;
}

static bool GBASIOLockstepDriverStart(struct GBASIODriver* driver) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	bool ret = false;
	MutexLock(&coordinator->mutex);
	if (coordinator->transferActive) {
		mLOG(GBA_SIO, ERROR, "Transfer restarted unexpectedly");
		goto out;
	}
	struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, lockstep->lockstepId);
	if (player->playerId != 0) {
		mLOG(GBA_SIO, DEBUG, "Secondary player attempted to start transfer");
		goto out;
	}
	mLOG(GBA_SIO, DEBUG, "Transfer starting at %08X", coordinator->cycle);
	memset(coordinator->multiData, 0xFF, sizeof(coordinator->multiData));
	_setData(coordinator, 0, player->driver->d.p);

	int32_t timestamp = GBASIOLockstepTime(player);
	struct GBASIOLockstepEvent event = {
		.type = SIO_EV_TRANSFER_START,
		.timestamp = timestamp,
		.finishCycle = timestamp + GBASIOTransferCycles(player->mode, player->driver->d.p->siocnt, coordinator->nAttached - 1),
	};
	_enqueueEvent(coordinator, &event, TARGET_SECONDARY);
	GBASIOLockstepCoordinatorWaitOnPlayers(coordinator, player);
	coordinator->transferActive = true;
	ret = true;
out:
	MutexUnlock(&coordinator->mutex);
	return ret;
}

static void GBASIOLockstepDriverFinishMultiplayer(struct GBASIODriver* driver, uint16_t data[4]) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	MutexLock(&coordinator->mutex);
	if (coordinator->transferMode == GBA_SIO_MULTI) {
		struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, lockstep->lockstepId);
		if (!player->dataReceived) {
			mLOG(GBA_SIO, WARN, "MULTI did not receive data. Are we running behind?");
			memset(data, 0xFF, sizeof(uint16_t) * 4);
		} else {
			mLOG(GBA_SIO, INFO, "MULTI transfer finished: %04X %04X %04X %04X",
			     coordinator->multiData[0],
			     coordinator->multiData[1],
			     coordinator->multiData[2],
			     coordinator->multiData[3]);
			memcpy(data, coordinator->multiData, sizeof(uint16_t) * 4);
		}
		player->dataReceived = false;
		if (player->playerId == 0) {
			_hardSync(coordinator, player);
		}
	}
	MutexUnlock(&coordinator->mutex);
}

static uint8_t GBASIOLockstepDriverFinishNormal8(struct GBASIODriver* driver) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	uint8_t data = 0xFF;
	MutexLock(&coordinator->mutex);
	if (coordinator->transferMode == GBA_SIO_NORMAL_8) {
		struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, lockstep->lockstepId);
		if (player->playerId > 0) {
			if (!player->dataReceived) {
				mLOG(GBA_SIO, WARN, "NORMAL did not receive data. Are we running behind?");
			} else {
				data = coordinator->normalData[player->playerId - 1];
				mLOG(GBA_SIO, INFO, "NORMAL8 transfer finished: %02X", data);
			}
		}
		player->dataReceived = false;
		if (player->playerId == 0) {
			_hardSync(coordinator, player);
		}
	}
	MutexUnlock(&coordinator->mutex);
	return data;
}

static uint32_t GBASIOLockstepDriverFinishNormal32(struct GBASIODriver* driver) {
	struct GBASIOLockstepDriver* lockstep = (struct GBASIOLockstepDriver*) driver;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	uint32_t data = 0xFFFFFFFF;
	MutexLock(&coordinator->mutex);
	if (coordinator->transferMode == GBA_SIO_NORMAL_32) {
		struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, lockstep->lockstepId);
		if (player->playerId > 0) {
			if (!player->dataReceived) {
				mLOG(GBA_SIO, WARN, "Did not receive data. Are we running behind?");
			} else {
				data = coordinator->normalData[player->playerId - 1];
				mLOG(GBA_SIO, INFO, "NORMAL32 transfer finished: %08X", data);
			}
		}
		player->dataReceived = false;
		if (player->playerId == 0) {
			_hardSync(coordinator, player);
		}
	}
	MutexUnlock(&coordinator->mutex);
	return data;
}

void GBASIOLockstepCoordinatorInit(struct GBASIOLockstepCoordinator* coordinator) {
	memset(coordinator, 0, sizeof(*coordinator));
	MutexInit(&coordinator->mutex);
	TableInit(&coordinator->players, 8, free);
}

void GBASIOLockstepCoordinatorDeinit(struct GBASIOLockstepCoordinator* coordinator) {
	MutexDeinit(&coordinator->mutex);
	TableDeinit(&coordinator->players);
}

void GBASIOLockstepCoordinatorAttach(struct GBASIOLockstepCoordinator* coordinator, struct GBASIOLockstepDriver* driver) {
	if (driver->coordinator && driver->coordinator != coordinator) {
		// TODO
		abort();
	}
	driver->coordinator = coordinator;
}

void GBASIOLockstepCoordinatorDetach(struct GBASIOLockstepCoordinator* coordinator, struct GBASIOLockstepDriver* driver) {
	if (driver->coordinator != coordinator) {
		// TODO
		abort();
		return;
	}
	MutexLock(&coordinator->mutex);
	struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, driver->lockstepId);
	if (player) {
		_removePlayer(coordinator, player);
	}
	MutexUnlock(&coordinator->mutex);
	driver->coordinator = NULL;
}

int32_t _untilNextSync(struct GBASIOLockstepCoordinator* coordinator, struct GBASIOLockstepPlayer* player) {
	int32_t cycle = coordinator->cycle - GBASIOLockstepTime(player);
	if (player->playerId == 0) {
		if (coordinator->nAttached < 2) {
			cycle += UNLOCKED_INTERVAL;
		} else {
			cycle += LOCKSTEP_INTERVAL;
		}
	}
	return cycle;
}

void _advanceCycle(struct GBASIOLockstepCoordinator* coordinator, struct GBASIOLockstepPlayer* player) {
	int32_t newCycle = GBASIOLockstepTime(player);
	mASSERT_DEBUG(newCycle - coordinator->cycle >= 0);
	coordinator->nextHardSync -= newCycle - coordinator->cycle;
	coordinator->cycle = newCycle;
}

void _removePlayer(struct GBASIOLockstepCoordinator* coordinator, struct GBASIOLockstepPlayer* player) {
	struct GBASIOLockstepEvent event = {
		.type = SIO_EV_DETACH,
		.playerId = player->playerId,
		.timestamp = GBASIOLockstepTime(player),
	};
	_enqueueEvent(coordinator, &event, TARGET_ALL & ~TARGET(player->playerId));

	coordinator->waiting = 0;
	coordinator->transferActive = false;

	TableRemove(&coordinator->players, player->driver->lockstepId);
	_reconfigPlayers(coordinator);

	struct GBASIOLockstepPlayer* runner = TableLookup(&coordinator->players, coordinator->attachedPlayers[0]);
	if (runner) {
		GBASIOLockstepPlayerWake(runner);
	}
	_verifyAwake(coordinator);
}

void _reconfigPlayers(struct GBASIOLockstepCoordinator* coordinator) {
	size_t players = TableSize(&coordinator->players);
	memset(coordinator->attachedPlayers, 0, sizeof(coordinator->attachedPlayers));
	if (players == 0) {
		mLOG(GBA_SIO, WARN, "Reconfiguring player IDs with no players attached somehow?");
	} else if (players == 1) {
		struct TableIterator iter;
		mASSERT_LOG(GBA_SIO, TableIteratorStart(&coordinator->players, &iter), "Trying to reconfigure 1 player with empty player list");
		unsigned p0 = TableIteratorGetKey(&coordinator->players, &iter);
		coordinator->attachedPlayers[0] = p0;

		struct GBASIOLockstepPlayer* player = TableIteratorGetValue(&coordinator->players, &iter);
		coordinator->cycle = mTimingCurrentTime(&player->driver->d.p->p->timing);
		coordinator->nextHardSync = HARD_SYNC_INTERVAL;

		if (player->playerId != 0) {
			player->playerId = 0;
			if (player->driver->user->playerIdChanged) {
				player->driver->user->playerIdChanged(player->driver->user, player->playerId);
			}
		}

		if (!coordinator->transferActive) {
			coordinator->transferMode = player->mode;
		}
	} else {
		struct UIntList playerPreferences[MAX_GBAS];

		int i;
		for (i = 0; i < MAX_GBAS; ++i) {
			UIntListInit(&playerPreferences[i], 4);
		}

		// Collect the first four players' requested player IDs so we can sort through them later
		int seen = 0;
		struct TableIterator iter;
		mASSERT_LOG(GBA_SIO, TableIteratorStart(&coordinator->players, &iter), "Trying to reconfigure %" PRIz "u players with empty player list", players);
		do {
			unsigned pid = TableIteratorGetKey(&coordinator->players, &iter);
			struct GBASIOLockstepPlayer* player = TableIteratorGetValue(&coordinator->players, &iter);
			int requested = MAX_GBAS - 1;
			if (player->driver->user->requestedId) {
				requested = player->driver->user->requestedId(player->driver->user);
			}
			if (requested < 0) {
				continue;
			}
			if (requested >= MAX_GBAS) {
				requested = MAX_GBAS - 1;
			}

			*UIntListAppend(&playerPreferences[requested]) = pid;
			++seen;
		} while (TableIteratorNext(&coordinator->players, &iter) && seen < MAX_GBAS);

		// Now sort each requested player ID to figure out who gets which ID
		seen = 0;
		for (i = 0; i < MAX_GBAS; ++i) {
			int j;
			for (j = 0; j <= i; ++j) {
				while (UIntListSize(&playerPreferences[j]) && seen < MAX_GBAS) {
					unsigned pid = *UIntListGetPointer(&playerPreferences[j], 0);
					UIntListShift(&playerPreferences[j], 0, 1);
					struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, pid);
					if (!player) {
						mLOG(GBA_SIO, ERROR, "Player list appears to have changed unexpectedly. PID %u missing.", pid);
						continue;
					}
					coordinator->attachedPlayers[seen] = pid;
					if (player->playerId != seen) {
						player->playerId = seen;
						if (player->driver->user->playerIdChanged) {
							player->driver->user->playerIdChanged(player->driver->user, player->playerId);
						}
					}
					++seen;
				}
			}
		}

		for (i = 0; i < MAX_GBAS; ++i) {
			UIntListDeinit(&playerPreferences[i]);
		}
	}

	int nAttached = 0;
	size_t i;
	for (i = 0; i < MAX_GBAS; ++i) {
		unsigned pid = coordinator->attachedPlayers[i];
		if (!pid) {
			continue;
		}
		struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, pid);
		if (!player) {
			coordinator->attachedPlayers[i] = 0;
		} else {
			++nAttached;
		}
	}
	coordinator->nAttached = nAttached;
}

static void _setData(struct GBASIOLockstepCoordinator* coordinator, uint32_t id, struct GBASIO* sio) {
	switch (coordinator->transferMode) {
	case GBA_SIO_MULTI:
		coordinator->multiData[id] = sio->p->memory.io[GBA_REG(SIOMLT_SEND)];
		break;
	case GBA_SIO_NORMAL_8:
		coordinator->normalData[id] = sio->p->memory.io[GBA_REG(SIODATA8)];
		break;
	case GBA_SIO_NORMAL_32:
		coordinator->normalData[id] = sio->p->memory.io[GBA_REG(SIODATA32_LO)];
		coordinator->normalData[id] |= sio->p->memory.io[GBA_REG(SIODATA32_HI)] << 16;
		break;
	case GBA_SIO_UART:
	case GBA_SIO_GPIO:
	case GBA_SIO_JOYBUS:
		mLOG(GBA_SIO, ERROR, "Unsupported mode %i in lockstep", coordinator->transferMode);
		// TODO: Should we handle this or just abort?
		break;
	}
}

void _setReady(struct GBASIOLockstepCoordinator* coordinator, struct GBASIOLockstepPlayer* activePlayer, int playerId, enum GBASIOMode mode) {
	activePlayer->otherModes[playerId] = mode;
	bool ready = true;
	int i;
	for (i = 0; ready && i < coordinator->nAttached; ++i) {
		ready = activePlayer->otherModes[i] == activePlayer->mode;
	}
	if (activePlayer->mode == GBA_SIO_MULTI) {
		struct GBASIO* sio = activePlayer->driver->d.p;
		sio->siocnt = GBASIOMultiplayerSetReady(sio->siocnt, ready);
		sio->rcnt = GBASIORegisterRCNTSetSd(sio->rcnt, ready);
	}
}

void _hardSync(struct GBASIOLockstepCoordinator* coordinator, struct GBASIOLockstepPlayer* player) {
	mASSERT_DEBUG(player->playerId == 0);
	struct GBASIOLockstepEvent event = {
		.type = SIO_EV_HARD_SYNC,
		.playerId = 0,
		.timestamp = GBASIOLockstepTime(player),
	};
	_enqueueEvent(coordinator, &event, TARGET_SECONDARY);
	GBASIOLockstepCoordinatorWaitOnPlayers(coordinator, player);
}

void _enqueueEvent(struct GBASIOLockstepCoordinator* coordinator, const struct GBASIOLockstepEvent* event, uint32_t target) {
	mLOG(GBA_SIO, DEBUG, "Enqueuing event of type %X from %i for target %X at timestamp %X",
	                      event->type, event->playerId, target, event->timestamp);

	int i;
	for (i = 0; i < coordinator->nAttached; ++i) {
		if (!(target & TARGET(i))) {
			continue;
		}
		struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, coordinator->attachedPlayers[i]);
		mASSERT_LOG(GBA_SIO, player->freeList, "No free events");
		struct GBASIOLockstepEvent* newEvent = player->freeList;
		player->freeList = newEvent->next;

		memcpy(newEvent, event, sizeof(*event));
		struct GBASIOLockstepEvent** previous = &player->queue;
		struct GBASIOLockstepEvent* next = player->queue;
		while (next) {
			int32_t until = newEvent->timestamp - next->timestamp;
			if (until < 0) {
				break;
			}
			previous = &next->next;
			next = next->next;
		}
		newEvent->next = next;
		*previous = newEvent;
	}
}

void _lockstepEvent(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GBASIOLockstepDriver* lockstep = context;
	struct GBASIOLockstepCoordinator* coordinator = lockstep->coordinator;
	MutexLock(&coordinator->mutex);
	struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, lockstep->lockstepId);
	struct GBASIO* sio = player->driver->d.p;
	mASSERT_LOG(GBA_SIO, player->playerId >= 0 && player->playerId < 4, "Invalid multiplayer ID %i", player->playerId);

	bool wasDetach = false;
	if (player->queue && player->queue->type == SIO_EV_DETACH) {
		mLOG(GBA_SIO, DEBUG, "Player %i detached at timestamp %X, picking up the pieces",
		                      player->queue->playerId, player->queue->timestamp);
		wasDetach = true;
	}
	if (player->playerId == 0 && GBASIOLockstepTime(player) - coordinator->cycle >= 0) {
		// We are the clock owner; advance the shared clock. However, if we just became
		// the clock owner (by the previous one disconnecting) we might be slightly
		// behind the shared clock. We should wait a bit if needed in that case.
		_advanceCycle(coordinator, player);
		if (!coordinator->transferActive) {
			GBASIOLockstepCoordinatorWakePlayers(coordinator);
		}
		if (coordinator->nextHardSync < 0) {
			if (!coordinator->waiting) {
				_hardSync(coordinator, player);
			}
			coordinator->nextHardSync += HARD_SYNC_INTERVAL;
		}
	}

	int32_t nextEvent = _untilNextSync(coordinator, player);
	while (true) {
		struct GBASIOLockstepEvent* event = player->queue;
		if (!event) {
			break;
		}
		if (event->timestamp > GBASIOLockstepTime(player)) {
			break;
		}
		player->queue = event->next;
		struct GBASIOLockstepEvent reply = {
			.playerId = player->playerId,
			.timestamp = GBASIOLockstepTime(player),
		};
		mLOG(GBA_SIO, DEBUG, "Got event of type %X from %i at timestamp %X",
		                      event->type, event->playerId, event->timestamp);
		switch (event->type) {
		case SIO_EV_ATTACH:
			_setReady(coordinator, player, event->playerId, -1);
			if (player->playerId == 0) {
				struct GBASIO* sio = player->driver->d.p;
				sio->siocnt = GBASIOMultiplayerClearSlave(sio->siocnt);
			}
			reply.mode = player->mode;
			reply.type = SIO_EV_MODE_SET;
			_enqueueEvent(coordinator, &reply, TARGET(event->playerId));
			break;
		case SIO_EV_HARD_SYNC:
			GBASIOLockstepCoordinatorAckPlayer(coordinator, player);
			break;
		case SIO_EV_TRANSFER_START:
			_setData(coordinator, player->playerId, sio);
			nextEvent = event->finishCycle - GBASIOLockstepTime(player) - cyclesLate;
			player->driver->d.p->siocnt |= 0x80;
			mTimingDeschedule(&sio->p->timing, &sio->completeEvent);
			mTimingSchedule(&sio->p->timing, &sio->completeEvent, nextEvent);
			GBASIOLockstepCoordinatorAckPlayer(coordinator, player);
			break;
		case SIO_EV_MODE_SET:
			_setReady(coordinator, player, event->playerId, event->mode);
			if (event->playerId == 0) {
				GBASIOLockstepCoordinatorAckPlayer(coordinator, player);
			}
			break;
		case SIO_EV_DETACH:
			_setReady(coordinator, player, event->playerId, -1);
			_setReady(coordinator, player, player->playerId, player->mode);
			reply.mode = player->mode;
			reply.type = SIO_EV_MODE_SET;
			_enqueueEvent(coordinator, &reply, ~TARGET(event->playerId));
			if (player->mode == GBA_SIO_MULTI) {
				sio->siocnt = GBASIOMultiplayerSetId(sio->siocnt, player->playerId);
				sio->siocnt = GBASIOMultiplayerSetSlave(sio->siocnt, player->playerId || coordinator->nAttached < 2);
			}
			wasDetach = true;
			break;
		}
		event->next = player->freeList;
		player->freeList = event;
	}
	if (player->queue && player->queue->timestamp - GBASIOLockstepTime(player) < nextEvent) {
		nextEvent = player->queue->timestamp - GBASIOLockstepTime(player);
	}

	if (player->playerId != 0 && nextEvent <= LOCKSTEP_INTERVAL) {
		if (!player->queue || wasDetach) {
			GBASIOLockstepPlayerSleep(player);
			// XXX: Is there a better way to gain sync lock at the beginning?
			if (nextEvent < 4) {
				nextEvent = 4;
			}
			_verifyAwake(coordinator);
		}
	}
	MutexUnlock(&coordinator->mutex);

	mASSERT_DEBUG(nextEvent > 0);
	mTimingSchedule(timing, &lockstep->event, nextEvent);
}

int32_t GBASIOLockstepTime(struct GBASIOLockstepPlayer* player) {
	return mTimingCurrentTime(&player->driver->d.p->p->timing) - player->cycleOffset;
}

void GBASIOLockstepCoordinatorWaitOnPlayers(struct GBASIOLockstepCoordinator* coordinator, struct GBASIOLockstepPlayer* player) {
	mASSERT_LOG(GBA_SIO, !coordinator->waiting, "Multiplayer desynchronized: coordinator not waiting");
	mASSERT_LOG(GBA_SIO, !player->asleep, "Multiplayer desynchronized: player not asleep");
	mASSERT_LOG(GBA_SIO, player->playerId == 0, "Multiplayer desynchronized: invalid player %i attempting to coordinate", player->playerId);
	if (coordinator->nAttached < 2) {
		return;
	}

	_advanceCycle(coordinator, player);
	mLOG(GBA_SIO, DEBUG, "Primary waiting for players to ack");
	coordinator->waiting = ((1 << coordinator->nAttached) - 1) & ~TARGET(player->playerId);
	GBASIOLockstepPlayerSleep(player);
	GBASIOLockstepCoordinatorWakePlayers(coordinator);

	_verifyAwake(coordinator);
}

void GBASIOLockstepCoordinatorWakePlayers(struct GBASIOLockstepCoordinator* coordinator) {
	int i;
	for (i = 1; i < coordinator->nAttached; ++i) {
		if (!coordinator->attachedPlayers[i]) {
			continue;
		}
		struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, coordinator->attachedPlayers[i]);
		GBASIOLockstepPlayerWake(player);
	}
}

void GBASIOLockstepPlayerWake(struct GBASIOLockstepPlayer* player) {
	if (!player->asleep) {
		return;
	}
	player->asleep = false;
	player->driver->user->wake(player->driver->user);
}

void GBASIOLockstepCoordinatorAckPlayer(struct GBASIOLockstepCoordinator* coordinator, struct GBASIOLockstepPlayer* player) {
	if (player->playerId == 0) {
		return;
	}
	coordinator->waiting &= ~TARGET(player->playerId);
	if (!coordinator->waiting) {
		mLOG(GBA_SIO, DEBUG, "All players acked, waking primary");
		if (coordinator->transferActive) {
			int i;
			for (i = 0; i < coordinator->nAttached; ++i) {
				if (!coordinator->attachedPlayers[i]) {
					continue;
				}
				struct GBASIOLockstepPlayer* player = TableLookup(&coordinator->players, coordinator->attachedPlayers[i]);
				player->dataReceived = true;
			}

			coordinator->transferActive = false;
		}

		struct GBASIOLockstepPlayer* runner = TableLookup(&coordinator->players, coordinator->attachedPlayers[0]);
		GBASIOLockstepPlayerWake(runner);
	}
	GBASIOLockstepPlayerSleep(player);
}

void GBASIOLockstepPlayerSleep(struct GBASIOLockstepPlayer* player) {
	if (player->asleep) {
		return;
	}
	player->asleep = true;
	player->driver->user->sleep(player->driver->user);
	player->driver->d.p->p->cpu->nextEvent = 0;
	player->driver->d.p->p->earlyExit = true;
}

size_t GBASIOLockstepCoordinatorAttached(struct GBASIOLockstepCoordinator* coordinator) {
	size_t count;
	MutexLock(&coordinator->mutex);
	count = TableSize(&coordinator->players);
	MutexUnlock(&coordinator->mutex);
	return count;
}
