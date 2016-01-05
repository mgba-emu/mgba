/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GUI_RUNNER_H
#define GUI_RUNNER_H

#include "gba/context/context.h"
#include "util/circle-buffer.h"
#include "util/gui.h"

enum GBAGUIInput {
	GBA_GUI_INPUT_INCREASE_BRIGHTNESS = GUI_INPUT_USER_START,
	GBA_GUI_INPUT_DECREASE_BRIGHTNESS,
	GBA_GUI_INPUT_SCREEN_MODE,
};

struct GBAGUIBackground {
	struct GUIBackground d;
	struct GBAGUIRunner* p;

	uint32_t* screenshot;
	int screenshotId;
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

	struct GUIMenuItem* configExtra;
	size_t nConfigExtra;

	float fps;
	int64_t lastFpsCheck;
	int32_t totalDelta;
	struct CircleBuffer fpsBuffer;

	void (*setup)(struct GBAGUIRunner*);
	void (*teardown)(struct GBAGUIRunner*);
	void (*gameLoaded)(struct GBAGUIRunner*);
	void (*gameUnloaded)(struct GBAGUIRunner*);
	void (*prepareForFrame)(struct GBAGUIRunner*);
	void (*drawFrame)(struct GBAGUIRunner*, bool faded);
	void (*drawScreenshot)(struct GBAGUIRunner*, const uint32_t* pixels, bool faded);
	void (*paused)(struct GBAGUIRunner*);
	void (*unpaused)(struct GBAGUIRunner*);
	void (*incrementScreenMode)(struct GBAGUIRunner*);
	uint16_t (*pollGameInput)(struct GBAGUIRunner*);
};

void GBAGUIInit(struct GBAGUIRunner*, const char* port);
void GBAGUIDeinit(struct GBAGUIRunner*);
void GBAGUIRun(struct GBAGUIRunner*, const char* path);
void GBAGUIRunloop(struct GBAGUIRunner*);

#endif
