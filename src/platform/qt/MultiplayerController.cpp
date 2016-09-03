/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MultiplayerController.h"

#include "GameController.h"

extern "C" {
#ifdef M_CORE_GBA
#include "gba/gba.h"
#endif
}


using namespace QGBA;

MultiplayerController::MultiplayerController() {
	GBASIOLockstepInit(&m_lockstep);
	m_lockstep.context = this;
	m_lockstep.signal = [](GBASIOLockstep* lockstep, unsigned mask) {
		MultiplayerController* controller = static_cast<MultiplayerController*>(lockstep->context);
		Player* player = &controller->m_players[0];
		bool woke = false;
		controller->m_lock.lock();
		player->waitMask &= ~mask;
		if (!player->waitMask && player->awake < 1) {
			mCoreThreadStopWaiting(player->controller->thread());
			player->awake = 1;
			woke = true;
		}
		controller->m_lock.unlock();
		return woke;
	};
	m_lockstep.wait = [](GBASIOLockstep* lockstep, unsigned mask) {
		MultiplayerController* controller = static_cast<MultiplayerController*>(lockstep->context);
		controller->m_lock.lock();
		Player* player = &controller->m_players[0];
		bool slept = false;
		player->waitMask |= mask;
		if (player->awake > 0) {
			mCoreThreadWaitFromThread(player->controller->thread());
			player->awake = 0;
			slept = true;
		}
		controller->m_lock.unlock();
		return slept;
	};
	m_lockstep.addCycles = [](GBASIOLockstep* lockstep, int id, int32_t cycles) {
		if (cycles < 0) {
			abort();
		}
		MultiplayerController* controller = static_cast<MultiplayerController*>(lockstep->context);
		controller->m_lock.lock();
		if (!id) {
			for (int i = 1; i < controller->m_players.count(); ++i) {
				Player* player = &controller->m_players[i];
				if (player->node->d.p->mode != controller->m_players[0].node->d.p->mode) {
					player->controller->setSync(true);
					continue;
				}
				player->controller->setSync(false);
				player->cyclesPosted += cycles;
				if (player->awake < 1) {
					player->node->nextEvent += player->cyclesPosted;
					mCoreThreadStopWaiting(player->controller->thread());
					player->awake = 1;
				}
			}
		} else {
			controller->m_players[id].controller->setSync(true);
			controller->m_players[id].cyclesPosted += cycles;
		}
		controller->m_lock.unlock();
	};
	m_lockstep.useCycles = [](GBASIOLockstep* lockstep, int id, int32_t cycles) {
		MultiplayerController* controller = static_cast<MultiplayerController*>(lockstep->context);
		controller->m_lock.lock();
		Player* player = &controller->m_players[id];
		player->cyclesPosted -= cycles;
		if (player->cyclesPosted <= 0) {
			mCoreThreadWaitFromThread(player->controller->thread());
			player->awake = 0;
		}
		cycles = player->cyclesPosted;
		controller->m_lock.unlock();
		return cycles;
	};
	m_lockstep.unload = [](GBASIOLockstep* lockstep, int id) {
		MultiplayerController* controller = static_cast<MultiplayerController*>(lockstep->context);
		controller->m_lock.lock();
		Player* player = &controller->m_players[id];
		if (id) {
			player->controller->setSync(true);
			player->waitMask &= ~(1 << id);
			if (!player->waitMask && player->awake < 1) {
				mCoreThreadStopWaiting(player->controller->thread());
				player->awake = 1;
			}
		} else {
			for (int i = 1; i < controller->m_players.count(); ++i) {
				Player* player = &controller->m_players[i];
				player->controller->setSync(true);
				player->cyclesPosted += lockstep->players[0]->eventDiff;
				if (player->awake < 1) {
					player->node->nextEvent += player->cyclesPosted;
					mCoreThreadStopWaiting(player->controller->thread());
					player->awake = 1;
				}
			}
		}
		controller->m_lock.unlock();
	};
}

MultiplayerController::~MultiplayerController() {
	GBASIOLockstepDeinit(&m_lockstep);
}

bool MultiplayerController::attachGame(GameController* controller) {
	if (m_lockstep.attached == MAX_GBAS) {
		return false;
	}

	mCoreThread* thread = controller->thread();
	if (!thread) {
		return false;
	}

#ifdef M_CORE_GBA
	if (controller->platform() == PLATFORM_GBA) {
		GBA* gba = static_cast<GBA*>(thread->core->board);

		GBASIOLockstepNode* node = new GBASIOLockstepNode;
		GBASIOLockstepNodeCreate(node);
		GBASIOLockstepAttachNode(&m_lockstep, node);
		m_players.append({
			controller,
			node,
			1,
			0,
			0
		});

		GBASIOSetDriver(&gba->sio, &node->d, SIO_MULTI);
		GBASIOSetDriver(&gba->sio, &node->d, SIO_NORMAL_32);

		emit gameAttached();
		return true;
	}
#endif

	return false;
}

void MultiplayerController::detachGame(GameController* controller) {
	mCoreThread* thread = controller->thread();
	if (!thread) {
		return;
	}
#ifdef M_CORE_GBA
	if (controller->platform() == PLATFORM_GBA) {
		GBA* gba = static_cast<GBA*>(thread->core->board);
		GBASIOLockstepNode* node = reinterpret_cast<GBASIOLockstepNode*>(gba->sio.drivers.multiplayer);
		GBASIOSetDriver(&gba->sio, nullptr, SIO_MULTI);
		GBASIOSetDriver(&gba->sio, nullptr, SIO_NORMAL_32);
		if (node) {
			GBASIOLockstepDetachNode(&m_lockstep, node);
			delete node;
		}
	}
#endif
	for (int i = 0; i < m_players.count(); ++i) {
		if (m_players[i].controller == controller) {
			m_players.removeAt(i);
			break;
		}
	}
	emit gameDetached();
}

int MultiplayerController::playerId(GameController* controller) {
	for (int i = 0; i < m_players.count(); ++i) {
		if (m_players[i].controller == controller) {
			return i;
		}
	}
	return -1;
}

int MultiplayerController::attached() {
	int num;
	num = m_lockstep.attached;
	return num;
}
