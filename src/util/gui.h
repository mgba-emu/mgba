/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GUI_H
#define GUI_H

#include "util/common.h"

struct GUIFont;

enum GUIInput {
	GUI_INPUT_NONE = -1,
	GUI_INPUT_SELECT = 0,
	GUI_INPUT_BACK,
	GUI_INPUT_CANCEL,

	GUI_INPUT_UP,
	GUI_INPUT_DOWN,
	GUI_INPUT_LEFT,
	GUI_INPUT_RIGHT,
};

struct GUIParams {
	int width;
	int height;
	const struct GUIFont* font;

	void (*drawStart)(void);
	void (*drawEnd)(void);
	int (*pollInput)(void);
};

#endif
