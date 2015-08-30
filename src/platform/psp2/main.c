/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "psp2-context.h"

#include "gba/gba.h"
#include "util/gui.h"
#include "util/gui/font.h"
#include "util/gui/file-select.h"

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
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

	if (pad.buttons & PSP2_CTRL_UP) {
		input |= 1 << GUI_INPUT_UP;
	}
	if (pad.buttons & PSP2_CTRL_DOWN) {
		input |= 1 << GUI_INPUT_DOWN;
	}
	if (pad.buttons & PSP2_CTRL_LEFT) {
		input |= 1 << GUI_INPUT_LEFT;
	}
	if (pad.buttons & PSP2_CTRL_RIGHT) {
		input |= 1 << GUI_INPUT_RIGHT;
	}
	return input;
}

int main() {
	printf("%s initializing", projectName);

	vita2d_init();
	struct GUIFont* font = GUIFontCreate();
	GBAPSP2Setup();
	struct GUIParams params = {
		PSP2_HORIZONTAL_PIXELS, PSP2_VERTICAL_PIXELS,
		font, "cache0:", _drawStart, _drawEnd, _pollInput
	};
	GUIInit(&params);

	while (true) {
		char path[256];
		if (!GUISelectFile(&params, path, sizeof(path), GBAIsROM)) {
			break;
		}
		if (!GBAPSP2LoadROM(path)) {
			continue;
		}
		GBAPSP2Runloop();
		GBAPSP2UnloadROM();
	}

	GBAPSP2Teardown();

	GUIFontDestroy(font);
	vita2d_fini();

	sceKernelExitProcess(0);
	return 0;
}
