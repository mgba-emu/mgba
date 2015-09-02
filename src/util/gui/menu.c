/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "menu.h"

#include "util/gui.h"
#include "util/gui/font.h"

DEFINE_VECTOR(GUIMenuItemList, struct GUIMenuItem);

enum GUIMenuExitReason GUIShowMenu(struct GUIParams* params, struct GUIMenu* menu, struct GUIMenuItem* item) {
	size_t start = 0;
	size_t pageSize = params->height / GUIFontHeight(params->font);
	if (pageSize > 4) {
		pageSize -= 4;
	} else {
		pageSize = 1;
	}

	GUIInvalidateKeys(params);
	while (true) {
		int newInput = 0;
		GUIPollInput(params, &newInput, 0);

		if (newInput & (1 << GUI_INPUT_UP) && menu->index > 0) {
			--menu->index;
		}
		if (newInput & (1 << GUI_INPUT_DOWN) && menu->index < GUIMenuItemListSize(&menu->items) - 1) {
			++menu->index;
		}
		if (newInput & (1 << GUI_INPUT_LEFT)) {
			if (menu->index >= pageSize) {
				menu->index -= pageSize;
			} else {
				menu->index = 0;
			}
		}
		if (newInput & (1 << GUI_INPUT_RIGHT)) {
			if (menu->index + pageSize < GUIMenuItemListSize(&menu->items)) {
				menu->index += pageSize;
			} else {
				menu->index = GUIMenuItemListSize(&menu->items) - 1;
			}
		}

		if (menu->index < start) {
			start = menu->index;
		}
		while ((menu->index - start + 4) * GUIFontHeight(params->font) > params->height) {
			++start;
		}
		if (newInput & (1 << GUI_INPUT_CANCEL)) {
			break;
		}
		if (newInput & (1 << GUI_INPUT_SELECT)) {
			*item = *GUIMenuItemListGetPointer(&menu->items, menu->index);
			return GUI_MENU_EXIT_ACCEPT;
		}
		if (newInput & (1 << GUI_INPUT_BACK)) {
			return GUI_MENU_EXIT_BACK;
		}

		params->drawStart();
		if (menu->background) {
			menu->background->draw(menu->background);
		}
		if (params->guiPrepare) {
			params->guiPrepare();
		}
		unsigned y = GUIFontHeight(params->font);
		GUIFontPrint(params->font, 0, y, GUI_TEXT_LEFT, 0xFFFFFFFF, menu->title);
		y += 2 * GUIFontHeight(params->font);
		size_t i;
		for (i = start; i < GUIMenuItemListSize(&menu->items); ++i) {
			int color = 0xE0A0A0A0;
			char bullet = ' ';
			if (i == menu->index) {
				color = 0xFFFFFFFF;
				bullet = '>';
			}
			GUIFontPrintf(params->font, 0, y, GUI_TEXT_LEFT, color, "%c %s", bullet, GUIMenuItemListGetPointer(&menu->items, i)->title);
			y += GUIFontHeight(params->font);
			if (y + GUIFontHeight(params->font) > params->height) {
				break;
			}
		}
		if (params->guiFinish) {
			params->guiFinish();
		}
		y += GUIFontHeight(params->font) * 2;

		params->drawEnd();
	}
	return GUI_MENU_EXIT_CANCEL;
}
