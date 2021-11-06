/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SDL_LOG_H
#define SDL_LOG_H

#include "main.h"
#include <mgba/core/core.h>
#include <mgba/core/log.h>

CXX_GUARD_START

struct mLogger getLogger(struct mCore* core);

CXX_GUARD_END

#endif
