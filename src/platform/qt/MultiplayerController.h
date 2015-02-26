/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_MULTIPLAYER_CONTROLLER
#define QGBA_MULTIPLAYER_CONTROLLER

extern "C" {
#include "gba/sio/lockstep.h"
}

namespace QGBA {

class GameController;

class MultiplayerController {
public:
	MultiplayerController();
	~MultiplayerController();

	bool attachGame(GameController*);
	void detachGame(GameController*);

private:
	GBASIOLockstep m_lockstep;
};

}
#endif
