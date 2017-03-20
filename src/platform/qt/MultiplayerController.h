/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_MULTIPLAYER_CONTROLLER
#define QGBA_MULTIPLAYER_CONTROLLER

#include <QMutex>
#include <QList>
#include <QObject>

#include <mgba/core/lockstep.h>
#ifdef M_CORE_GBA
#include <mgba/internal/gba/sio/lockstep.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/sio/lockstep.h>
#endif

struct GBSIOLockstepNode;
struct GBASIOLockstepNode;

namespace QGBA {

class GameController;

class MultiplayerController : public QObject {
Q_OBJECT

public:
	MultiplayerController();

	bool attachGame(GameController*);
	void detachGame(GameController*);

	int attached();
	int playerId(GameController*);

signals:
	void gameAttached();
	void gameDetached();

private:
	struct Player {
		GameController* controller;
		GBSIOLockstepNode* gbNode;
		GBASIOLockstepNode* gbaNode;
		int awake;
		int32_t cyclesPosted;
		unsigned waitMask;
	};
	union {
		mLockstep m_lockstep;
#ifdef M_CORE_GB
		GBSIOLockstep m_gbLockstep;
#endif
#ifdef M_CORE_GBA
		GBASIOLockstep m_gbaLockstep;
#endif
	};
	QList<Player> m_players;
	QMutex m_lock;
};

}
#endif
