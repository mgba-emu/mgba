/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MultiplayerController.h"

#include "CoreController.h"

#ifdef M_CORE_GBA
#include <mgba/internal/gba/gba.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/gb.h>
#endif

using namespace QGBA;

#ifdef M_CORE_GB
MultiplayerController::Player::Player(CoreController* coreController, GBSIOLockstepNode* node)
	: controller(coreController)
	, gbNode(node)
{
}
#endif

#ifdef M_CORE_GBA
MultiplayerController::Player::Player(CoreController* coreController, GBASIOLockstepNode* node)
	: controller(coreController)
	, gbaNode(node)
{
}
#endif

MultiplayerController::MultiplayerController() {
	mLockstepInit(&m_lockstep);
	m_lockstep.context = this;
	m_lockstep.lock = [](mLockstep* lockstep) {
		MultiplayerController* controller = static_cast<MultiplayerController*>(lockstep->context);
		controller->m_lock.lock();
	};
	m_lockstep.unlock = [](mLockstep* lockstep) {
		MultiplayerController* controller = static_cast<MultiplayerController*>(lockstep->context);
		controller->m_lock.unlock();
	};
	m_lockstep.signal = [](mLockstep* lockstep, unsigned mask) {
		MultiplayerController* controller = static_cast<MultiplayerController*>(lockstep->context);
		Player* player = &controller->m_players[0];
		bool woke = false;
		player->waitMask &= ~mask;
		if (!player->waitMask && player->awake < 1) {
			mCoreThreadStopWaiting(player->controller->thread());
			player->awake = 1;
			woke = true;
		}
		return woke;
	};
	m_lockstep.wait = [](mLockstep* lockstep, unsigned mask) {
		MultiplayerController* controller = static_cast<MultiplayerController*>(lockstep->context);
		Player* player = &controller->m_players[0];
		bool slept = false;
		player->waitMask |= mask;
		if (player->awake > 0) {
			mCoreThreadWaitFromThread(player->controller->thread());
			player->awake = 0;
			slept = true;
		}
		return slept;
	};
	m_lockstep.addCycles = [](mLockstep* lockstep, int id, int32_t cycles) {
		if (cycles < 0) {
			abort();
		}
		MultiplayerController* controller = static_cast<MultiplayerController*>(lockstep->context);
		if (!id) {
			for (int i = 1; i < controller->m_players.count(); ++i) {
				Player* player = &controller->m_players[i];
#ifdef M_CORE_GBA
				if (player->controller->platform() == PLATFORM_GBA && player->gbaNode->d.p->mode != controller->m_players[0].gbaNode->d.p->mode) {
					player->controller->setSync(true);
					continue;
				}
#endif
				player->controller->setSync(false);
				player->cyclesPosted += cycles;
				if (player->awake < 1) {
					switch (player->controller->platform()) {
#ifdef M_CORE_GBA
					case PLATFORM_GBA:
						player->gbaNode->nextEvent += player->cyclesPosted;
						break;
#endif
#ifdef M_CORE_GB
					case PLATFORM_GB:
						player->gbNode->nextEvent += player->cyclesPosted;
						break;
#endif
					default:
						break;
					}
					mCoreThreadStopWaiting(player->controller->thread());
					player->awake = 1;
				}
			}
		} else {
			controller->m_players[id].controller->setSync(true);
			controller->m_players[id].cyclesPosted += cycles;
		}
	};
	m_lockstep.useCycles = [](mLockstep* lockstep, int id, int32_t cycles) {
		MultiplayerController* controller = static_cast<MultiplayerController*>(lockstep->context);
		Player* player = &controller->m_players[id];
		player->cyclesPosted -= cycles;
		if (player->cyclesPosted <= 0) {
			mCoreThreadWaitFromThread(player->controller->thread());
			player->awake = 0;
		}
		cycles = player->cyclesPosted;
		return cycles;
	};
	m_lockstep.unusedCycles= [](mLockstep* lockstep, int id) {
		MultiplayerController* controller = static_cast<MultiplayerController*>(lockstep->context);
		Player* player = &controller->m_players[id];
		auto cycles = player->cyclesPosted;
		return cycles;
	};
	m_lockstep.unload = [](mLockstep* lockstep, int id) {
		MultiplayerController* controller = static_cast<MultiplayerController*>(lockstep->context);
		if (id) {
			Player* player = &controller->m_players[id];
			player->controller->setSync(true);
			player->cyclesPosted = 0;

			// release master GBA if it is waiting for this GBA
			player = &controller->m_players[0];
			player->waitMask &= ~(1 << id);
			if (!player->waitMask && player->awake < 1) {
				mCoreThreadStopWaiting(player->controller->thread());
				player->awake = 1;
			}
		} else {
			for (int i = 1; i < controller->m_players.count(); ++i) {
				Player* player = &controller->m_players[i];
				player->controller->setSync(true);
				switch (player->controller->platform()) {
#ifdef M_CORE_GBA
				case PLATFORM_GBA:
					player->cyclesPosted += reinterpret_cast<GBASIOLockstep*>(lockstep)->players[0]->eventDiff;
					break;
#endif
#ifdef M_CORE_GB
				case PLATFORM_GB:
					player->cyclesPosted += reinterpret_cast<GBSIOLockstep*>(lockstep)->players[0]->eventDiff;
					break;
#endif
				default:
					break;
				}
				if (player->awake < 1) {
					switch (player->controller->platform()) {
#ifdef M_CORE_GBA
					case PLATFORM_GBA:
						player->gbaNode->nextEvent += player->cyclesPosted;
						break;
#endif
#ifdef M_CORE_GB
					case PLATFORM_GB:
						player->gbNode->nextEvent += player->cyclesPosted;
						break;
#endif
					default:
						break;
					}
					mCoreThreadStopWaiting(player->controller->thread());
					player->awake = 1;
				}
			}
		}
	};
}

MultiplayerController::~MultiplayerController() {
	mLockstepDeinit(&m_lockstep);
}

bool MultiplayerController::attachGame(CoreController* controller) {
	if (m_lockstep.attached == MAX_GBAS) {
		return false;
	}

	if (m_lockstep.attached == 0) {
		switch (controller->platform()) {
#ifdef M_CORE_GBA
		case PLATFORM_GBA:
			GBASIOLockstepInit(&m_gbaLockstep);
			break;
#endif
#ifdef M_CORE_GB
		case PLATFORM_GB:
			GBSIOLockstepInit(&m_gbLockstep);
			break;
#endif
		default:
			return false;
		}
	}

	mCoreThread* thread = controller->thread();
	if (!thread) {
		return false;
	}

	switch (controller->platform()) {
#ifdef M_CORE_GBA
	case PLATFORM_GBA: {
		GBA* gba = static_cast<GBA*>(thread->core->board);

		GBASIOLockstepNode* node = new GBASIOLockstepNode;
		GBASIOLockstepNodeCreate(node);
		GBASIOLockstepAttachNode(&m_gbaLockstep, node);
		m_players.append({controller, node});

		GBASIOSetDriver(&gba->sio, &node->d, SIO_MULTI);

		emit gameAttached();
		return true;
	}
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB: {
		GB* gb = static_cast<GB*>(thread->core->board);

		GBSIOLockstepNode* node = new GBSIOLockstepNode;
		GBSIOLockstepNodeCreate(node);
		GBSIOLockstepAttachNode(&m_gbLockstep, node);
		m_players.append({controller, node});

		GBSIOSetDriver(&gb->sio, &node->d);

		emit gameAttached();
		return true;
	}
#endif
	default:
		break;
	}

	return false;
}

void MultiplayerController::detachGame(CoreController* controller) {
	if (m_players.empty()) {
		return;
	}
	mCoreThread* thread = controller->thread();
	if (!thread) {
		return;
	}
	QList<CoreController::Interrupter> interrupters;

	for (int i = 0; i < m_players.count(); ++i) {
		interrupters.append(m_players[i].controller);
	}
	switch (controller->platform()) {
#ifdef M_CORE_GBA
	case PLATFORM_GBA: {
		GBA* gba = static_cast<GBA*>(thread->core->board);
		GBASIOLockstepNode* node = reinterpret_cast<GBASIOLockstepNode*>(gba->sio.drivers.multiplayer);
		GBASIOSetDriver(&gba->sio, nullptr, SIO_MULTI);
		if (node) {
			GBASIOLockstepDetachNode(&m_gbaLockstep, node);
			delete node;
		}
		break;
	}
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB: {
		GB* gb = static_cast<GB*>(thread->core->board);
		GBSIOLockstepNode* node = reinterpret_cast<GBSIOLockstepNode*>(gb->sio.driver);
		GBSIOSetDriver(&gb->sio, nullptr);
		if (node) {
			GBSIOLockstepDetachNode(&m_gbLockstep, node);
			delete node;
		}
		break;
	}
#endif
	default:
		break;
	}

	for (int i = 0; i < m_players.count(); ++i) {
		if (m_players[i].controller == controller) {
			m_players.removeAt(i);
			break;
		}
	}
	emit gameDetached();
}

int MultiplayerController::playerId(CoreController* controller) {
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
