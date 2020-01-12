/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/input.h>

#include <mgba/gba/interface.h>

MGBA_EXPORT const struct mInputPlatformInfo GBAInputInfo = {
	.platformName = "gba",
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
		"L"
	},
	.nKeys = GBA_KEY_MAX,
	.hat = {
		.up = GBA_KEY_UP,
		.left = GBA_KEY_LEFT,
		.down = GBA_KEY_DOWN,
		.right = GBA_KEY_RIGHT
	}
};
