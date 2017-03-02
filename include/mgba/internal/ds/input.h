/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_INPUT_H
#define DS_INPUT_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/input.h>

extern const struct mInputPlatformInfo DSInputInfo;

enum GBAKey {
	DS_KEY_A = 0,
	DS_KEY_B = 1,
	DS_KEY_SELECT = 2,
	DS_KEY_START = 3,
	DS_KEY_RIGHT = 4,
	DS_KEY_LEFT = 5,
	DS_KEY_UP = 6,
	DS_KEY_DOWN = 7,
	DS_KEY_R = 8,
	DS_KEY_L = 9,
	DS_KEY_X = 10,
	DS_KEY_Y = 11,
	DS_KEY_MAX,
	DS_KEY_NONE = -1
};

CXX_GUARD_END

#endif
