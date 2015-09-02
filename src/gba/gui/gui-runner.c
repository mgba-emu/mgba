/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gui-runner.h"

#include "gba/serialize.h"
#include "util/gui/file-select.h"
#include "util/gui/font.h"
#include "util/gui/menu.h"
#include "util/vfs.h"

enum {
	RUNNER_CONTINUE,
	RUNNER_EXIT,
	RUNNER_SAVE_STATE,
	RUNNER_LOAD_STATE,
};

static void _drawBackground(struct GUIBackground* background) {
	struct GBAGUIBackground* gbaBackground = (struct GBAGUIBackground*) background;
	if (gbaBackground->p->drawFrame) {
		gbaBackground->p->drawFrame(gbaBackground->p, true);
	}
}

void GBAGUIInit(struct GBAGUIRunner* runner, const char* port) {
	GUIInit(&runner->params);
	GBAContextInit(&runner->context, port);
	runner->background.d.draw = _drawBackground;
	runner->background.p = runner;
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
		.background = &runner->background.d
	};
	GUIMenuItemListInit(&pauseMenu.items, 0);
	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Unpause", .data = (void*) RUNNER_CONTINUE };
#if !(defined(__POWERPC__) || defined(__PPC__))
	// PPC doesn't have working savestates yet
	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Save state", .data = (void*) RUNNER_SAVE_STATE };
	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Load state", .data = (void*) RUNNER_LOAD_STATE };
#endif
	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Exit game", .data = (void*) RUNNER_EXIT };

	while (true) {
		char path[256];
		if (!GUISelectFile(&runner->params, path, sizeof(path), GBAIsROM)) {
			if (runner->params.guiFinish) {
				runner->params.guiFinish();
			}
			GUIMenuItemListDeinit(&pauseMenu.items);
			return;
		}

		if (runner->params.guiPrepare) {
			runner->params.guiPrepare();
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
					runner->params.drawStart();
					runner->drawFrame(runner, false);
					runner->params.drawEnd();
				}
			}

			GUIInvalidateKeys(&runner->params);
			int keys = -1; // Huge hack to avoid an extra variable!
			struct GUIMenuItem item;
			enum GUIMenuExitReason reason = GUIShowMenu(&runner->params, &pauseMenu, &item);
			if (reason == GUI_MENU_EXIT_ACCEPT) {
				struct VFile* vf;
				switch ((int) item.data) {
				case RUNNER_EXIT:
					running = false;
					keys = 0;
					break;
				case RUNNER_SAVE_STATE:
					vf = GBAGetState(runner->context.gba, 0, 1, true);
					if (vf) {
						GBASaveStateNamed(runner->context.gba, vf, true);
						vf->close(vf);
					}
					break;
				case RUNNER_LOAD_STATE:
					vf = GBAGetState(runner->context.gba, 0, 1, false);
					if (vf) {
						GBALoadStateNamed(runner->context.gba, vf);
						vf->close(vf);
					}
					break;
				case RUNNER_CONTINUE:
					break;
				}
			}
			while (keys) {
				GUIPollInput(&runner->params, 0, &keys);
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
