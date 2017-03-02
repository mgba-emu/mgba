/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/input.h>

const struct mInputPlatformInfo DSInputInfo = {
	.platformName = "ds",
	.keyId = (const char*[]) {
		"A",
		"B",
		"Select",
		"Start",
		"Right",
		"Left",
		"Up",
		"Down",
		"R",
		"L",
		"X",
		"Y"
	},
	.nKeys = DS_KEY_MAX,
	.hat = {
		.up = DS_KEY_UP,
		.left = DS_KEY_LEFT,
		.down = DS_KEY_DOWN,
		.right = DS_KEY_RIGHT
	}
};
