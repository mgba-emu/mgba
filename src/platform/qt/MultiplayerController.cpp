/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MultiplayerController.h"

#include "CoreController.h"
#include "LogController.h"

#ifdef M_CORE_GBA
#include <mgba/internal/gba/gba.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/gb.h>
#endif

#include <algorithm>

using namespace QGBA;

MultiplayerController::Player::Player(CoreController* coreController)
	: controller(coreController)
{
}

int MultiplayerController::Player::id() const {
	switch (controller->platform()) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		return node.gba->id;
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		return node.gb->id;
#endif
	case mPLATFORM_NONE:
		break;
	}
	return -1;
}

bool MultiplayerController::Player::operator<(const MultiplayerController::Player& other) const {
	return id() < other.id();
}

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
		Player* player = controller->player(0);
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
		Player* player = controller->player(0);
		bool slept = false;
		player->waitMask |= mask;
		if (player->awake > 0) {
			mCoreThreadWaitFromThread(player->controller->thread());
			player->awake = 0;
			slept = true;
		}
		player->controller->setSync(true);
		return slept;
	};
	m_lockstep.addCycles = [](mLockstep* lockstep, int id, int32_t cycles) {
		if (cycles < 0) {
			abort();
		}
		MultiplayerController* controller = static_cast<MultiplayerController*>(lockstep->context);
		Player* player = controller->player(id);
		switch (player->controller->platform()) {
#ifdef M_CORE_GBA
		case mPLATFORM_GBA:
			if (!id) {
				for (int i = 1; i < controller->m_players.count(); ++i) {
					player = controller->player(i);
					if (player->node.gba->d.p->mode > SIO_MULTI) {
						player->controller->setSync(true);
						continue;
					}
					player->controller->setSync(false);
					player->cyclesPosted += cycles;
					if (player->awake < 1) {
						player->node.gba->nextEvent += player->cyclesPosted;
					}
					mCoreThreadStopWaiting(player->controller->thread());
					player->awake = 1;
				}
			} else {
				player->controller->setSync(true);
				player->cyclesPosted += cycles;
			}
			break;
#endif
#ifdef M_CORE_GB
		case mPLATFORM_GB:
			if (!id) {
				player = controller->player(1);
				player->controller->setSync(false);
				player->cyclesPosted += cycles;
				if (player->awake < 1) {
					player->node.gb->nextEvent += player->cyclesPosted;
				}
				mCoreThreadStopWaiting(player->controller->thread());
				player->awake = 1;
			} else {
				player->controller->setSync(true);
				player->cyclesPosted += cycles;
			}
			break;
#endif
		default:
			break;
		}
	};
	m_lockstep.useCycles = [](mLockstep* lockstep, int id, int32_t cycles) {
		MultiplayerController* controller = static_cast<MultiplayerController*>(lockstep->context);
		Player* player = controller->player(id);
		player->cyclesPosted -= cycles;
		if (player->cyclesPosted <= 0) {
			mCoreThreadWaitFromThread(player->controller->thread());
			player->awake = 0;
		}
		cycles = player->cyclesPosted;
		return cycles;
	};
	m_lockstep.unusedCycles = [](mLockstep* lockstep, int id) {
		MultiplayerController* controller = static_cast<MultiplayerController*>(lockstep->context);
		Player* player = controller->player(id);
		auto cycles = player->cyclesPosted;
		return cycles;
	};
	m_lockstep.unload = [](mLockstep* lockstep, int id) {
		MultiplayerController* controller = static_cast<MultiplayerController*>(lockstep->context);
		if (id) {
			Player* player = controller->player(id);
			player->controller->setSync(true);
			player->cyclesPosted = 0;

			// release master GBA if it is waiting for this GBA
			player = controller->player(0);
			player->waitMask &= ~(1 << id);
			if (!player->waitMask && player->awake < 1) {
				mCoreThreadStopWaiting(player->controller->thread());
				player->awake = 1;
			}
		} else {
			for (int i = 1; i < controller->m_players.count(); ++i) {
				Player* player = controller->player(i);
				player->controller->setSync(true);
				switch (player->controller->platform()) {
#ifdef M_CORE_GBA
				case mPLATFORM_GBA:
					player->cyclesPosted += reinterpret_cast<GBASIOLockstep*>(lockstep)->players[0]->eventDiff;
					break;
#endif
#ifdef M_CORE_GB
				case mPLATFORM_GB:
					player->cyclesPosted += reinterpret_cast<GBSIOLockstep*>(lockstep)->players[0]->eventDiff;
					break;
#endif
				default:
					break;
				}
				if (player->awake < 1) {
					switch (player->controller->platform()) {
#ifdef M_CORE_GBA
					case mPLATFORM_GBA:
						player->node.gba->nextEvent += player->cyclesPosted;
						break;
#endif
#ifdef M_CORE_GB
					case mPLATFORM_GB:
						player->node.gb->nextEvent += player->cyclesPosted;
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
	QList<CoreController::Interrupter> interrupters;
	interrupters.append(controller);
	for (Player& p : m_pids.values()) {
		interrupters.append(p.controller);
	}

	if (m_lockstep.attached == 0) {
		switch (controller->platform()) {
#ifdef M_CORE_GBA
		case mPLATFORM_GBA:
			GBASIOLockstepInit(&m_gbaLockstep);
			break;
#endif
#ifdef M_CORE_GB
		case mPLATFORM_GB:
			GBSIOLockstepInit(&m_gbLockstep);
			break;
#endif
		default:
			return false;
		}
		m_platform = controller->platform();
	} else if (controller->platform() != m_platform) {
		return false;
	}

	mCoreThread* thread = controller->thread();
	if (!thread) {
		return false;
	}

	Player player{controller};
	switch (controller->platform()) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA: {
		if (m_lockstep.attached >= MAX_GBAS) {
			return false;
		}

		GBA* gba = static_cast<GBA*>(thread->core->board);

		GBASIOLockstepNode* node = new GBASIOLockstepNode;
		GBASIOLockstepNodeCreate(node);
		GBASIOLockstepAttachNode(&m_gbaLockstep, node);
		player.node.gba = node;

		GBASIOSetDriver(&gba->sio, &node->d, SIO_MULTI);
		GBASIOSetDriver(&gba->sio, &node->d, SIO_NORMAL_32);
		break;
	}
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB: {
		if (m_lockstep.attached >= 2) {
			return false;
		}

		GB* gb = static_cast<GB*>(thread->core->board);

		GBSIOLockstepNode* node = new GBSIOLockstepNode;
		GBSIOLockstepNodeCreate(node);
		GBSIOLockstepAttachNode(&m_gbLockstep, node);
		player.node.gb = node;

		GBSIOSetDriver(&gb->sio, &node->d);
		break;
	}
#endif
	default:
		return false;
	}

	QPair<QString, QString> path(controller->path(), controller->baseDirectory());
	int claimed = m_claimed[path];

	int saveId = 0;
	mCoreConfigGetIntValue(&controller->thread()->core->config, "savePlayerId", &saveId);

	if (claimed) {
		player.saveId = 0;
		for (int i = 0; i < MAX_GBAS; ++i) {
			if (claimed & (1 << i)) {
				continue;
			}
			player.saveId = i + 1;
			break;
		}
		if (!player.saveId) {
			LOG(QT, ERROR) << "Couldn't find available save ID";
			player.saveId = 1;
		}
	} else if (saveId) {
		player.saveId = saveId;
	} else {
		player.saveId = 1;
	}
	m_claimed[path] |= 1 << (player.saveId - 1);

	m_pids.insert(m_nextPid, player);
	++m_nextPid;
	fixOrder();

	emit gameAttached();
	return true;
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

	int pid = -1;
	for (int i = 0; i < m_players.count(); ++i) {
		Player* p = player(i);
		if (!p) {
			LOG(QT, ERROR) << tr("Trying to detach a multiplayer player that's not attached");
			return;
		}
		CoreController* playerController = p->controller;
		if (playerController == controller) {
			pid = m_players[i];
		}
		interrupters.append(playerController);
	}
	switch (controller->platform()) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA: {
		GBA* gba = static_cast<GBA*>(thread->core->board);
		GBASIOLockstepNode* node = reinterpret_cast<GBASIOLockstepNode*>(gba->sio.drivers.multiplayer);
		GBASIOSetDriver(&gba->sio, nullptr, SIO_MULTI);
		GBASIOSetDriver(&gba->sio, nullptr, SIO_NORMAL_32);
		if (node) {
			GBASIOLockstepDetachNode(&m_gbaLockstep, node);
			delete node;
		}
		break;
	}
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB: {
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

	// TODO: This might change if we replace the ROM--make sure to handle this properly
	QPair<QString, QString> path(controller->path(), controller->baseDirectory());
	Player& p = m_pids.find(pid).value();
	if (!p.saveId) {
		LOG(QT, ERROR) << "Clearing invalid save ID";
	} else {
		m_claimed[path] &= ~(1 << (p.saveId - 1));
		if (!m_claimed[path]) {
			m_claimed.remove(path);
		}
	}

	m_pids.remove(pid);
	if (m_pids.size() == 0) {
		m_platform = mPLATFORM_NONE;
	} else {
		fixOrder();
	}
	emit gameDetached();
}

int MultiplayerController::playerId(CoreController* controller) const {
	for (int i = 0; i < m_players.count(); ++i) {
		const Player* p = player(i);
		if (!p) {
			LOG(QT, ERROR) << tr("Trying to get player ID for a multiplayer player that's not attached");
			return -1;
		}
		if (p->controller == controller) {
			return i;
		}
	}
	return -1;
}

int MultiplayerController::saveId(CoreController* controller) const {
	for (int i = 0; i < m_players.count(); ++i) {
		const Player* p = player(i);
		if (!p) {
			LOG(QT, ERROR) << tr("Trying to get save ID for a multiplayer player that's not attached");
			return -1;
		}
		if (p->controller == controller) {
			return p->saveId;
		}
	}
	return -1;
}

int MultiplayerController::attached() {
	int num;
	num = m_lockstep.attached;
	return num;
}

MultiplayerController::Player* MultiplayerController::player(int id) {
	if (id >= m_players.size()) {
		return nullptr;
	}
	int pid = m_players[id];
	auto iter = m_pids.find(pid);
	if (iter == m_pids.end()) {
		return nullptr;
	}
	return &iter.value();
}

const MultiplayerController::Player* MultiplayerController::player(int id) const {
	if (id >= m_players.size()) {
		return nullptr;
	}
	int pid = m_players[id];
	auto iter = m_pids.find(pid);
	if (iter == m_pids.end()) {
		return nullptr;
	}
	return &iter.value();
}

void MultiplayerController::fixOrder() {
	m_players.clear();
	m_players = m_pids.keys();
	std::sort(m_players.begin(), m_players.end());
	switch (m_platform) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		for (int pid : m_pids.keys()) {
			Player& p = m_pids.find(pid).value();
			GBA* gba = static_cast<GBA*>(p.controller->thread()->core->board);
			GBASIOLockstepNode* node = reinterpret_cast<GBASIOLockstepNode*>(gba->sio.drivers.multiplayer);
			m_players[node->id] = pid;
		}
		break;
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		if (player(0)->node.gb->id == 1) {
			std::swap(m_players[0], m_players[1]);
		}
		break;
#endif
	case mPLATFORM_NONE:
		break;
	}
}
