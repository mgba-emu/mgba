/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MultiplayerController.h"

#include "GameController.h"

using namespace QGBA;

MultiplayerController::MultiplayerController() {
	GBASIOLockstepInit(&m_lockstep);
}

MultiplayerController::~MultiplayerController() {
	GBASIOLockstepDeinit(&m_lockstep);
}

bool MultiplayerController::attachGame(GameController* controller) {
	MutexLock(&m_lockstep.mutex);
	if (m_lockstep.attached == MAX_GBAS) {
		MutexUnlock(&m_lockstep.mutex);
		return false;
	}
	GBASIOLockstepNode* node = new GBASIOLockstepNode;
	GBASIOLockstepNodeCreate(node);
	GBASIOLockstepAttachNode(&m_lockstep, node);
	MutexUnlock(&m_lockstep.mutex);

	controller->threadInterrupt();
	GBAThread* thread = controller->thread();
	if (controller->isLoaded()) {
		GBASIOSetDriver(&thread->gba->sio, &node->d, SIO_MULTI);
	}
	thread->sioDrivers.multiplayer = &node->d;
	controller->threadContinue();
	emit gameAttached();
	return true;
}

void MultiplayerController::detachGame(GameController* controller) {
	controller->threadInterrupt();
	MutexLock(&m_lockstep.mutex);
	GBAThread* thread = nullptr;
	for (int i = 0; i < m_lockstep.attached; ++i) {
		thread = controller->thread();
		if (thread->sioDrivers.multiplayer == &m_lockstep.players[i]->d) {
			break;
		}
		thread = nullptr;
	}
	if (thread) {
		GBASIOLockstepNode* node = reinterpret_cast<GBASIOLockstepNode*>(thread->sioDrivers.multiplayer);
		if (controller->isLoaded()) {
			GBASIOSetDriver(&thread->gba->sio, nullptr, SIO_MULTI);
		}
		thread->sioDrivers.multiplayer = nullptr;
		GBASIOLockstepDetachNode(&m_lockstep, node);
		delete node;
	}
	MutexUnlock(&m_lockstep.mutex);
	controller->threadContinue();
	emit gameDetached();
}

int MultiplayerController::playerId(GameController* controller) {
	MutexLock(&m_lockstep.mutex);
	int id = -1;
	for (int i = 0; i < m_lockstep.attached; ++i) {
		GBAThread* thread = controller->thread();
		if (thread->sioDrivers.multiplayer == &m_lockstep.players[i]->d) {
			id = i;
			break;
		}
	}
	MutexUnlock(&m_lockstep.mutex);
	return id;
}

int MultiplayerController::attached() {
	int num;
	MutexLock(&m_lockstep.mutex);
	num = m_lockstep.attached;
	MutexUnlock(&m_lockstep.mutex);
	return num;
}
