/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gui-config.h"

#include "gba/gui/gui-runner.h"
#include "util/gui/file-select.h"
#include "util/gui/menu.h"

void GBAGUIShowConfig(struct GBAGUIRunner* runner, struct GUIMenuItem* extra, size_t nExtra) {
	struct GUIMenu menu = {
		.title = "Configure",
		.index = 0,
		.background = &runner->background.d
	};
	GUIMenuItemListInit(&menu.items, 0);
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Frameskip",
		.data = "frameskip",
		.submenu = 0,
		.state = 0,
		.validStates = (const char*[]) {
			"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", 0
		}
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Show framerate",
		.data = "fpsCounter",
		.submenu = 0,
		.state = false,
		.validStates = (const char*[]) {
			"Off", "On", 0
		}
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Use BIOS if found",
		.data = "useBios",
		.submenu = 0,
		.state = true,
		.validStates = (const char*[]) {
			"Off", "On", 0
		}
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Select BIOS path",
		.data = "bios",
	};
	size_t i;
	for (i = 0; i < nExtra; ++i) {
		*GUIMenuItemListAppend(&menu.items) = extra[i];
	}
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Save",
		.data = "[SAVE]",
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Cancel",
		.data = 0,
	};
	enum GUIMenuExitReason reason;
	char biosPath[256] = "";

	struct GUIMenuItem* item;
	for (i = 0; i < GUIMenuItemListSize(&menu.items); ++i) {
		item = GUIMenuItemListGetPointer(&menu.items, i);
		if (!item->validStates || !item->data) {
			continue;
		}
		GBAConfigGetUIntValue(&runner->context.config, item->data, &item->state);
	}

	while (true) {
		reason = GUIShowMenu(&runner->params, &menu, &item);
		if (reason != GUI_MENU_EXIT_ACCEPT || !item->data) {
			break;
		}
		if (!strcmp(item->data, "[SAVE]")) {
			if (biosPath[0]) {
				GBAConfigSetValue(&runner->context.config, "bios", biosPath);
			}
			for (i = 0; i < GUIMenuItemListSize(&menu.items); ++i) {
				item = GUIMenuItemListGetPointer(&menu.items, i);
				if (!item->validStates || !item->data) {
					continue;
				}
				GBAConfigSetUIntValue(&runner->context.config, item->data, item->state);
			}
			GBAConfigSave(&runner->context.config);
			break;
		}
		if (!strcmp(item->data, "bios")) {
			// TODO: show box if failed
			if (!GUISelectFile(&runner->params, biosPath, sizeof(biosPath), GBAIsBIOS)) {
				biosPath[0] = '\0';
			}
			continue;
		}
		if (item->validStates) {
			++item->state;
			if (!item->validStates[item->state]) {
				item->state = 0;
			}
		}
	}
}
