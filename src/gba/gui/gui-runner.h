/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GUI_RUNNER_H
#define GUI_RUNNER_H

#include "gba/context/context.h"
#include "util/gui.h"

enum GBAGUIInput {
	GBA_GUI_INPUT_INCREASE_BRIGHTNESS = GUI_INPUT_USER_START,
	GBA_GUI_INPUT_DECREASE_BRIGHTNESS
};

struct GBAGUIBackground {
	struct GUIBackground d;
	struct GBAGUIRunner* p;
};

struct GBAGUIRunnerLux {
	struct GBALuminanceSource d;
	int luxLevel;
};

struct GBAGUIRunner {
	struct GBAContext context;
	struct GUIParams params;

	struct GBAGUIBackground background;
	struct GBAGUIRunnerLux luminanceSource;

	void (*setup)(struct GBAGUIRunner*);
	void (*teardown)(struct GBAGUIRunner*);
	void (*gameLoaded)(struct GBAGUIRunner*);
	void (*gameUnloaded)(struct GBAGUIRunner*);
	void (*prepareForFrame)(struct GBAGUIRunner*);
	void (*drawFrame)(struct GBAGUIRunner*, bool faded);
	uint16_t (*pollGameInput)(struct GBAGUIRunner*);
};

void GBAGUIInit(struct GBAGUIRunner*, const char* port);
void GBAGUIDeinit(struct GBAGUIRunner*);
void GBAGUIRunloop(struct GBAGUIRunner*);

#endif
