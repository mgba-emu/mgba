/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SDL_RPI_COMMON_H
#define SDL_RPI_COMMON_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include "main.h"

void mRPIGLCommonSwap(struct VideoBackend* context);
void mRPIGLCommonInit(struct mSDLRenderer* renderer);

CXX_GUARD_END

#endif
