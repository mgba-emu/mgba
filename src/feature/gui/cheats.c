/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gui-config.h"

#include <mgba/core/cheats.h>
#include <mgba/core/core.h>
#include "feature/gui/gui-runner.h"
#include <mgba-util/gui/menu.h>

enum mGUICheatAction {
	CHEAT_BACK = 0,
	CHEAT_ADD_LINE = 1,
	CHEAT_RENAME,
	CHEAT_DELETE,
};

static const char* const offOn[] = { "Off", "On" };

static void _rebuildCheatView(struct GUIMenuItemList* items, const struct mCheatSet* set) {
	GUIMenuItemListClear(items);
	size_t i;
	for (i = 0; i < StringListSize(&set->lines); ++i) {
		*GUIMenuItemListAppend(items) = (struct GUIMenuItem) {
			.title = *StringListGetConstPointer(&set->lines, i),
			.readonly = true
		};
	}
	*GUIMenuItemListAppend(items) = (struct GUIMenuItem) {
		.title = "Back",
		.data = GUI_V_U(CHEAT_BACK),
	};
}

static void mGUIShowCheatSet(struct mGUIRunner* runner, struct mCheatDevice* device, struct mCheatSet* set) {
	struct GUIMenu menu = {
		.title = "Edit cheat",
		.subtitle = set->name,
		.index = 0,
		.background = &runner->background.d
	};
	GUIMenuItemListInit(&menu.items, 0);

	struct GUIMenu view = {
		.title = "View cheat",
		.subtitle = set->name,
		.index = 0,
		.background = &runner->background.d
	};

	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Enable",
		.state = set->enabled,
		.validStates = offOn,
		.nStates = 2
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Add line",
		.data = GUI_V_U(CHEAT_ADD_LINE),
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "View lines",
		.submenu = &view,
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Rename",
		.data = GUI_V_U(CHEAT_RENAME),
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Delete",
		.data = GUI_V_U(CHEAT_DELETE),
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Back",
		.data = GUI_V_V,
	};

	GUIMenuItemListInit(&view.items, 0);
	_rebuildCheatView(&view.items, set);

	while (true) {
		struct GUIKeyboardParams keyboard;
		GUIKeyboardParamsInit(&keyboard);
		struct GUIMenuItem* item;
		enum GUIMenuExitReason reason = GUIShowMenu(&runner->params, &menu, &item);
		set->enabled = GUIMenuItemListGetPointer(&menu.items, 0)->state;
		if (reason != GUI_MENU_EXIT_ACCEPT || GUIVariantIsVoid(item->data)) {
			break;
		}

		enum mGUICheatAction action = (enum mGUICheatAction) item->data.v.u;
		switch (action) {
		case CHEAT_ADD_LINE:
			strlcpy(keyboard.title, "Add line", sizeof(keyboard.title));
			keyboard.maxLen = 17;
			if (runner->params.getText(&keyboard) == GUI_KEYBOARD_DONE) {
				mCheatAddLine(set, keyboard.result, 0);
				_rebuildCheatView(&view.items, set);
			}
			break;
		case CHEAT_RENAME:
			strlcpy(keyboard.title, "Rename cheat", sizeof(keyboard.title));
			strlcpy(keyboard.result, set->name, sizeof(keyboard.result));
			keyboard.maxLen = 50;
			if (runner->params.getText(&keyboard) == GUI_KEYBOARD_DONE) {
				mCheatSetRename(set, keyboard.result);
				menu.subtitle = set->name;
				view.subtitle = set->name;
			}
			break;
		case CHEAT_DELETE:
			mCheatRemoveSet(device, set);
			break;
		case CHEAT_BACK:
			// Used by submenus to return to the top menu
			break;
		}

		if (action == CHEAT_DELETE) {
			break;
		}
	}
	GUIMenuItemListDeinit(&menu.items);
	GUIMenuItemListDeinit(&view.items);
}

void mGUIShowCheats(struct mGUIRunner* runner) {
	struct mCheatDevice* device = runner->core->cheatDevice(runner->core);
	if (!device) {
		return;
	}
	struct GUIMenu menu = {
		.title = "Cheats",
		.index = 0,
		.background = &runner->background.d
	};
	GUIMenuItemListInit(&menu.items, 0);

	while (true) {
		size_t i;
		for (i = 0; i < mCheatSetsSize(&device->cheats); ++i) {
			struct mCheatSet* set = *mCheatSetsGetPointer(&device->cheats, i);
			*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
				.title = set->name,
				.data = GUI_V_P(set),
				.state = set->enabled,
				.validStates = offOn,
				.nStates = 2
			};
		}
		*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
			.title = "Add new cheat set",
			.data = GUI_V_V,
		};
		*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
			.title = "Back",
			.data = GUI_V_I(-1),
		};

		struct GUIMenuItem* item;
		enum GUIMenuExitReason reason = GUIShowMenu(&runner->params, &menu, &item);
		for (i = 0; i < mCheatSetsSize(&device->cheats); ++i) {
			struct mCheatSet* set = *mCheatSetsGetPointer(&device->cheats, i);
			struct GUIMenuItem* item = GUIMenuItemListGetPointer(&menu.items, i);
			set->enabled = item->state;
		}

		if (reason != GUI_MENU_EXIT_ACCEPT || GUIVariantCompareInt(item->data, -1)) {
			break;
		}
		struct mCheatSet* set = NULL;
		if (GUIVariantIsVoid(item->data)) {
			struct GUIKeyboardParams keyboard;
			GUIKeyboardParamsInit(&keyboard);
			keyboard.maxLen = 50;
			strlcpy(keyboard.title, "Cheat name", sizeof(keyboard.title));
			strlcpy(keyboard.result, "New cheat", sizeof(keyboard.result));
			if (runner->params.getText(&keyboard) == GUI_KEYBOARD_DONE) {
				set = device->createSet(device, keyboard.result);
				mCheatAddSet(device, set);
			}
		} else {
			set = item->data.v.p;
		}
		if (set) {
			mGUIShowCheatSet(runner, device, set);
		}
		GUIMenuItemListClear(&menu.items);
	}
	GUIMenuItemListDeinit(&menu.items);
	mCheatAutosave(device);
}
