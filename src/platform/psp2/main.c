/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "psp2-context.h"

#include "gba/gba.h"
#include "gba/gui/gui-runner.h"
#include "util/gui.h"
#include "util/gui/font.h"
#include "util/gui/file-select.h"

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/moduleinfo.h>

#include <vita2d.h>

PSP2_MODULE_INFO(0, 0, "mGBA");

static void _drawStart(void) {
	vita2d_start_drawing();
	vita2d_clear_screen();
}

static void _drawEnd(void) {
	vita2d_end_drawing();
	vita2d_swap_buffers();
}

static int _pollInput(void) {
	SceCtrlData pad;
	sceCtrlPeekBufferPositive(0, &pad, 1);
	int input = 0;
	if (pad.buttons & PSP2_CTRL_TRIANGLE) {
		input |= 1 << GUI_INPUT_CANCEL;
	}
	if (pad.buttons & PSP2_CTRL_CIRCLE) {
		input |= 1 << GUI_INPUT_BACK;
	}
	if (pad.buttons & PSP2_CTRL_CROSS) {
		input |= 1 << GUI_INPUT_SELECT;
	}

	if (pad.buttons & PSP2_CTRL_UP || pad.ly < 64) {
		input |= 1 << GUI_INPUT_UP;
	}
	if (pad.buttons & PSP2_CTRL_DOWN || pad.ly >= 192) {
		input |= 1 << GUI_INPUT_DOWN;
	}
	if (pad.buttons & PSP2_CTRL_LEFT || pad.lx < 64) {
		input |= 1 << GUI_INPUT_LEFT;
	}
	if (pad.buttons & PSP2_CTRL_RIGHT || pad.lx >= 192) {
		input |= 1 << GUI_INPUT_RIGHT;
	}

	return input;
}

int main() {
	printf("%s initializing", projectName);

	vita2d_init();
	struct GUIFont* font = GUIFontCreate();
	struct GBAGUIRunner runner = {
		.params = {
			PSP2_HORIZONTAL_PIXELS, PSP2_VERTICAL_PIXELS,
			font, "cache0:", _drawStart, _drawEnd, _pollInput, 0, 0,

			GUI_PARAMS_TRAIL
		},
		.setup = GBAPSP2Setup,
		.teardown = GBAPSP2Teardown,
		.gameLoaded = GBAPSP2LoadROM,
		.gameUnloaded = GBAPSP2UnloadROM,
		.prepareForFrame = GBAPSP2PrepareForFrame,
		.drawFrame = GBAPSP2Draw,
		.pollGameInput = GBAPSP2PollInput
	};

	GBAGUIInit(&runner, 0);
	GBAGUIRunloop(&runner);
	GBAGUIDeinit(&runner);

	GUIFontDestroy(font);
	vita2d_fini();

	sceKernelExitProcess(0);
	return 0;
}
