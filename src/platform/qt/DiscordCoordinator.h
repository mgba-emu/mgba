/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <memory>

namespace QGBA {

class CoreController;

namespace DiscordCoordinator {

void init();
void deinit();

void gameStarted(std::shared_ptr<CoreController>);
void gameStopped();

}

}