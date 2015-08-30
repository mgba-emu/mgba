/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gui-runner.h"

#include "util/gui/file-select.h"
#include "util/gui/font.h"

void GBAGUIInit(struct GBAGUIRunner* runner, const char* port) {
	GUIInit(&runner->params);
	GBAContextInit(&runner->context, port);
	if (runner->setup) {
		runner->setup(runner);
	}
}

void GBAGUIDeinit(struct GBAGUIRunner* runner) {
	if (runner->teardown) {
		runner->teardown(runner);
	}
	GBAContextDeinit(&runner->context);
}

void GBAGUIRunloop(struct GBAGUIRunner* runner) {
	while (true) {
		if (runner->params.guiPrepare) {
			runner->params.guiPrepare();
		}
		char path[256];
		if (!GUISelectFile(&runner->params, path, sizeof(path), GBAIsROM)) {
			if (runner->params.guiFinish) {
				runner->params.guiFinish();
			}
			return;
		}
		if (runner->params.guiFinish) {
			runner->params.guiFinish();
		}

		// TODO: Message box API
		runner->params.drawStart();
		GUIFontPrint(runner->params.font, runner->params.width / 2, (GUIFontHeight(runner->params.font) + runner->params.height) / 2, GUI_TEXT_CENTER, 0xFFFFFFFF, "Loading...");
		runner->params.drawEnd();
		runner->params.drawStart();
		GUIFontPrint(runner->params.font, runner->params.width / 2, (GUIFontHeight(runner->params.font) + runner->params.height) / 2, GUI_TEXT_CENTER, 0xFFFFFFFF, "Loading...");
		runner->params.drawEnd();

		if (!GBAContextLoadROM(&runner->context, path, true)) {
			int i;
			for (i = 0; i < 300; ++i) {
				runner->params.drawStart();
				GUIFontPrint(runner->params.font, runner->params.width / 2, (GUIFontHeight(runner->params.font) + runner->params.height) / 2, GUI_TEXT_CENTER, 0xFFFFFFFF, "Load failed!");
				runner->params.drawEnd();
			}
		}
		GBAContextStart(&runner->context);
		if (runner->gameLoaded) {
			runner->gameLoaded(runner);
		}
		while (true) {
			int guiKeys = runner->params.pollInput();
			if (guiKeys & (1 << GUI_INPUT_CANCEL)) {
				break;
			}
			uint16_t keys = runner->pollGameInput(runner);
			if (runner->prepareForFrame) {
				runner->prepareForFrame(runner);
			}
			GBAContextFrame(&runner->context, keys);
			if (runner->drawFrame) {
				runner->drawFrame(runner, false);
			}
		}
		GBAContextStop(&runner->context);
		if (runner->gameUnloaded) {
			runner->gameUnloaded(runner);
		}
		GBAContextUnloadROM(&runner->context);
	}
}
