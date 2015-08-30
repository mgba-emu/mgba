/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gui-runner.h"

#include "util/gui/file-select.h"
#include "util/gui/font.h"
#include "util/gui/menu.h"

enum {
	RUNNER_CONTINUE,
	RUNNER_EXIT
};

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
	struct GUIMenu pauseMenu = {
		.title = "Game Paused",
		.index = 0,
	};
	GUIMenuItemListInit(&pauseMenu.items, 0);
	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Unpause", .data = (void*) RUNNER_CONTINUE };
	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Exit game", .data = (void*) RUNNER_EXIT };

	while (true) {
		if (runner->params.guiPrepare) {
			runner->params.guiPrepare();
		}
		char path[256];
		if (!GUISelectFile(&runner->params, path, sizeof(path), GBAIsROM)) {
			if (runner->params.guiFinish) {
				runner->params.guiFinish();
			}
			GUIMenuItemListDeinit(&pauseMenu.items);
			return;
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
		if (runner->params.guiFinish) {
			runner->params.guiFinish();
		}
		GBAContextStart(&runner->context);
		if (runner->gameLoaded) {
			runner->gameLoaded(runner);
		}
		bool running = true;
		while (running) {
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

			if (runner->params.guiPrepare) {
				runner->params.guiPrepare();
			}
			GUIInvalidateKeys(&runner->params);
			while (true) {
				struct GUIMenuItem item;
				enum GUIMenuExitReason reason = GUIShowMenu(&runner->params, &pauseMenu, &item);
				if (reason == GUI_MENU_EXIT_ACCEPT) {
					if (item.data == (void*) RUNNER_EXIT) {
						running = false;
						break;
					}
					if (item.data == (void*) RUNNER_CONTINUE) {
						int keys = -1;
						while (keys) {
							GUIPollInput(&runner->params, 0, &keys);
						}
						break;
					}
				} else {
					int keys = -1;
					while (keys) {
						GUIPollInput(&runner->params, 0, &keys);
					}
					break;
				}
			}
			if (runner->params.guiFinish) {
				runner->params.guiFinish();
			}
		}
		GBAContextStop(&runner->context);
		if (runner->gameUnloaded) {
			runner->gameUnloaded(runner);
		}
		GBAContextUnloadROM(&runner->context);
	}
	GUIMenuItemListDeinit(&pauseMenu.items);
}
