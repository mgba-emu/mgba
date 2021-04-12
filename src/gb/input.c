/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/input.h>

#include <mgba/gb/interface.h>

const struct mInputPlatformInfo GBInputInfo = {
	.platformName = "gb",
	.keyId = (const char*[]) {
		"A",
		"B",
		"Select",
		"Start",
		"Right",
		"Left",
		"Up",
		"Down",
	},
	.nKeys = GB_KEY_MAX,
	.hat = {
		.up = GB_KEY_UP,
		.left = GB_KEY_LEFT,
		.down = GB_KEY_DOWN,
		.right = GB_KEY_RIGHT
	}
};
