/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "remap.h"

#include "gba/input.h"
#include "util/gui.h"
#include "util/gui/menu.h"

void GBAGUIRemapKeys(struct GUIParams* params, struct mInputMap* map, const struct GUIInputKeys* keys) {
	struct GUIMenu menu = {
		.title = "Remap keys",
		.index = 0,
		.background = 0
	};
	GUIMenuItemListInit(&menu.items, 0);
	const char* keyNames[keys->nKeys + 1];
	memcpy(&keyNames[1], keys->keyNames, keys->nKeys * sizeof(keyNames[0]));
	keyNames[0] = "Unmapped";
	size_t i;
	for (i = 0; i < GBA_KEY_MAX; ++i) {
		*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
			.title = GBAInputInfo.keyId[i],
			.data = (void*) (GUI_INPUT_MAX + i),
			.submenu = 0,
			.state = mInputQueryBinding(map, keys->id, i) + 1,
			.validStates = keyNames,
			.nStates = keys->nKeys + 1
		};
	}
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Save",
		.data = (void*) (GUI_INPUT_MAX + GBA_KEY_MAX + 2),
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Cancel",
		.data = 0,
	};

	struct GUIMenuItem* item;
	while (true) {
		enum GUIMenuExitReason reason;
		reason = GUIShowMenu(params, &menu, &item);
		if (reason != GUI_MENU_EXIT_ACCEPT || !item->data) {
			break;
		}
		if (item->data == (void*) (GUI_INPUT_MAX + GBA_KEY_MAX + 2)) {
			for (i = 0; i < GUIMenuItemListSize(&menu.items); ++i) {
				item = GUIMenuItemListGetPointer(&menu.items, i);
				if (i < GBA_KEY_MAX) {
					mInputBindKey(map, keys->id, item->state - 1, i);
				}
			}
			break;
		}
		if (item->validStates) {
			// TODO: Open remap menu
		}
	}
}
