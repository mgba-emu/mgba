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
	m_lockstep.signal = [](GBASIOLockstep* lockstep, int id) {
		MultiplayerController* controller = static_cast<MultiplayerController*>(lockstep->context);
		GameController* game = controller->m_players[id];
		controller->m_lock.lock();
		if (--controller->m_asleep[id] == 0) {
			mCoreThreadStopWaiting(game->thread());
		}
		controller->m_lock.unlock();
	};
	m_lockstep.wait = [](GBASIOLockstep* lockstep, int id) {
		MultiplayerController* controller = static_cast<MultiplayerController*>(lockstep->context);
		controller->m_lock.lock();
		GameController* game = controller->m_players[id];
		if (++controller->m_asleep[id] == 1) {
			mCoreThreadWaitFromThread(game->thread());
		} else if (controller->m_asleep[id] == 0) {
			mCoreThreadStopWaiting(game->thread());
		}
		if (controller->m_asleep[id] > 1) {
			//abort();
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
		m_players.append(controller);
		m_asleep.append(0);

		GBASIOSetDriver(&gba->sio, &node->d, SIO_MULTI);
		GBASIOSetDriver(&gba->sio, &node->d, SIO_NORMAL_32);

		emit gameAttached();
		return true;
	}
#endif

	return false;
}

void MultiplayerController::detachGame(GameController* controller) {
	if (!m_players.contains(controller)) {
		return;
	}
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
	int i = m_players.indexOf(controller);
	m_players.removeAt(i);
	m_players.removeAt(i);
	emit gameDetached();
}

int MultiplayerController::playerId(GameController* controller) {
	return m_players.indexOf(controller);
}

int MultiplayerController::attached() {
	int num;
	num = m_lockstep.attached;
	return num;
}
