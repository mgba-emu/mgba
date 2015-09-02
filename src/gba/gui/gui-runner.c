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
	RUNNER_CONTINUE = 1,
	RUNNER_EXIT = 2,
	RUNNER_SAVE_STATE = 3,
	RUNNER_LOAD_STATE = 4,
	RUNNER_COMMAND_MASK = 0xFFFF,

	RUNNER_STATE_1 = 0x10000,
	RUNNER_STATE_2 = 0x20000,
	RUNNER_STATE_3 = 0x30000,
	RUNNER_STATE_4 = 0x40000,
	RUNNER_STATE_5 = 0x50000,
	RUNNER_STATE_6 = 0x60000,
	RUNNER_STATE_7 = 0x70000,
	RUNNER_STATE_8 = 0x80000,
	RUNNER_STATE_9 = 0x90000,
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
	struct GUIMenu stateSaveMenu = {
		.title = "Save state",
		.index = 0,
		.background = &runner->background.d
	};
	struct GUIMenu stateLoadMenu = {
		.title = "Load state",
		.index = 0,
		.background = &runner->background.d
	};
	GUIMenuItemListInit(&pauseMenu.items, 0);
	GUIMenuItemListInit(&stateSaveMenu.items, 9);
	GUIMenuItemListInit(&stateLoadMenu.items, 9);
	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Unpause", .data = (void*) RUNNER_CONTINUE };
#if !(defined(__POWERPC__) || defined(__PPC__))
	// PPC doesn't have working savestates yet
	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Save state", .submenu = &stateSaveMenu };
	*GUIMenuItemListAppend(&pauseMenu.items) = (struct GUIMenuItem) { .title = "Load state", .submenu = &stateLoadMenu };

	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 1", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE_1) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 2", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE_2) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 3", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE_3) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 4", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE_4) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 5", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE_5) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 6", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE_6) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 7", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE_7) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 8", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE_8) };
	*GUIMenuItemListAppend(&stateSaveMenu.items) = (struct GUIMenuItem) { .title = "State 9", .data = (void*) (RUNNER_SAVE_STATE | RUNNER_STATE_9) };

	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 1", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE_1) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 2", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE_2) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 3", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE_3) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 4", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE_4) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 5", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE_5) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 6", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE_6) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 7", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE_7) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 8", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE_8) };
	*GUIMenuItemListAppend(&stateLoadMenu.items) = (struct GUIMenuItem) { .title = "State 9", .data = (void*) (RUNNER_LOAD_STATE | RUNNER_STATE_9) };
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
				switch (((int) item.data) & RUNNER_COMMAND_MASK) {
				case RUNNER_EXIT:
					running = false;
					keys = 0;
					break;
				case RUNNER_SAVE_STATE:
					vf = GBAGetState(runner->context.gba, 0, ((int) item.data) >> 16, true);
					if (vf) {
						GBASaveStateNamed(runner->context.gba, vf, true);
						vf->close(vf);
					}
					break;
				case RUNNER_LOAD_STATE:
					vf = GBAGetState(runner->context.gba, 0, ((int) item.data) >> 16, false);
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
	GUIMenuItemListDeinit(&stateSaveMenu.items);
	GUIMenuItemListDeinit(&stateLoadMenu.items);
}
