/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QList>
#include <QMutex>
#include <QObject>

#include <mgba/core/core.h>
#include <mgba/core/lockstep.h>
#ifdef M_CORE_GBA
#include <mgba/internal/gba/sio/lockstep.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/sio/lockstep.h>
#endif

#include <memory>

struct GBSIOLockstepNode;
struct GBASIOLockstepNode;

namespace QGBA {

class CoreController;

class MultiplayerController : public QObject {
Q_OBJECT

public:
	MultiplayerController();
	~MultiplayerController();

	bool attachGame(CoreController*);
	void detachGame(CoreController*);

	int attached();
	int playerId(CoreController*);

signals:
	void gameAttached();
	void gameDetached();

private:
	union Node {
		GBSIOLockstepNode* gb;
		GBASIOLockstepNode* gba;
	};
	struct Player {
#ifdef M_CORE_GB
		Player(CoreController* controller, GBSIOLockstepNode* node);
#endif
#ifdef M_CORE_GBA
		Player(CoreController* controller, GBASIOLockstepNode* node);
#endif

		int id() const;
		bool operator<(const Player&) const;

		CoreController* controller;
		Node node = {nullptr};
		int awake = 1;
		int32_t cyclesPosted = 0;
		unsigned waitMask = 0;
	};

	Player* player(int id);

	union {
		mLockstep m_lockstep;
#ifdef M_CORE_GB
		GBSIOLockstep m_gbLockstep;
#endif
#ifdef M_CORE_GBA
		GBASIOLockstep m_gbaLockstep;
#endif
	};

	mPlatform m_platform = mPLATFORM_NONE;
	QList<Player> m_players;
	QMutex m_lock;
};

}
