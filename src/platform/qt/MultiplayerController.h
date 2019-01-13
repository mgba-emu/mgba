/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

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
	struct Player {
		Player(
			CoreController* _controller,
			GBSIOLockstepNode* _gbNode,
			GBASIOLockstepNode* _gbaNode,
			int _awake,
			int32_t _cyclesPosted,
			unsigned _waitMask
		);

		~Player();

		CoreController* controller;
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
