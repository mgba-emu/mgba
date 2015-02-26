/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MultiplayerController.h"

using namespace QGBA;

MultiplayerController::MultiplayerController() {
	GBASIOLockstepInit(&m_lockstep);
}

MultiplayerController::~MultiplayerController() {
	GBASIOLockstepDeinit(&m_lockstep);
}

bool MultiplayerController::attachGame(GameController* controller) {
	return false;
}
