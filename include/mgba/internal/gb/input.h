/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_INPUT_H
#define GB_INPUT_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/input.h>

extern MGBA_EXPORT const struct mInputPlatformInfo GBInputInfo;

enum GBKey {
	GB_KEY_A = 0,
	GB_KEY_B = 1,
	GB_KEY_SELECT = 2,
	GB_KEY_START = 3,
	GB_KEY_RIGHT = 4,
	GB_KEY_LEFT = 5,
	GB_KEY_UP = 6,
	GB_KEY_DOWN = 7,
	GB_KEY_MAX,
};

CXX_GUARD_END

#endif
