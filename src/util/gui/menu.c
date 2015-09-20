/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "menu.h"

#include "util/gui.h"
#include "util/gui/font.h"

DEFINE_VECTOR(GUIMenuItemList, struct GUIMenuItem);

enum GUIMenuExitReason GUIShowMenu(struct GUIParams* params, struct GUIMenu* menu, struct GUIMenuItem** item) {
	size_t start = 0;
	size_t lineHeight = GUIFontHeight(params->font);
	size_t pageSize = params->height / lineHeight;
	if (pageSize > 4) {
		pageSize -= 4;
	} else {
		pageSize = 1;
	}
	int cursorOverItem = 0;

	GUIInvalidateKeys(params);
	while (true) {
		uint32_t newInput = 0;
		GUIPollInput(params, &newInput, 0);
		int cx, cy;
		enum GUICursorState cursor = GUIPollCursor(params, &cx, &cy);

		if (newInput & (1 << GUI_INPUT_UP) && menu->index > 0) {
			--menu->index;
		}
		if (newInput & (1 << GUI_INPUT_DOWN) && menu->index < GUIMenuItemListSize(&menu->items) - 1) {
			++menu->index;
		}
		if (newInput & (1 << GUI_INPUT_LEFT)) {
			struct GUIMenuItem* item = GUIMenuItemListGetPointer(&menu->items, menu->index);
			if (item->validStates) {
				if (item->state > 0) {
					--item->state;
				}
			} else if (menu->index >= pageSize) {
				menu->index -= pageSize;
			} else {
				menu->index = 0;
			}
		}
		if (newInput & (1 << GUI_INPUT_RIGHT)) {
			struct GUIMenuItem* item = GUIMenuItemListGetPointer(&menu->items, menu->index);
			if (item->validStates) {
				if (item->validStates[item->state + 1]) {
					++item->state;
				}
			} else if (menu->index + pageSize < GUIMenuItemListSize(&menu->items)) {
				menu->index += pageSize;
			} else {
				menu->index = GUIMenuItemListSize(&menu->items) - 1;
			}
		}
		if (cursor != GUI_CURSOR_NOT_PRESENT) {
			int index = (cy / lineHeight) - 2;
			if (index >= 0 && index + start < GUIMenuItemListSize(&menu->items)) {
				if (menu->index != index + start || !cursorOverItem) {
					cursorOverItem = 1;
				}
				menu->index = index + start;
			} else {
				cursorOverItem = 0;
			}
		}

		if (menu->index < start) {
			start = menu->index;
		}
		while ((menu->index - start + 4) * lineHeight > params->height) {
			++start;
		}
		if (newInput & (1 << GUI_INPUT_CANCEL)) {
			break;
		}
		if (newInput & (1 << GUI_INPUT_SELECT) || (cursorOverItem == 2 && cursor == GUI_CURSOR_CLICKED)) {
			*item = GUIMenuItemListGetPointer(&menu->items, menu->index);
			if ((*item)->submenu) {
				enum GUIMenuExitReason reason = GUIShowMenu(params, (*item)->submenu, item);
				if (reason != GUI_MENU_EXIT_BACK) {
					return reason;
				}
			} else {
				return GUI_MENU_EXIT_ACCEPT;
			}
		}
		if (cursorOverItem == 1 && (cursor == GUI_CURSOR_UP || cursor == GUI_CURSOR_NOT_PRESENT)) {
			cursorOverItem = 2;
		}
		if (newInput & (1 << GUI_INPUT_BACK)) {
			return GUI_MENU_EXIT_BACK;
		}

		params->drawStart();
		if (menu->background) {
			menu->background->draw(menu->background, GUIMenuItemListGetPointer(&menu->items, menu->index)->data);
		}
		if (params->guiPrepare) {
			params->guiPrepare();
		}
		unsigned y = lineHeight;
		GUIFontPrint(params->font, 0, y, GUI_TEXT_LEFT, 0xFFFFFFFF, menu->title);
		y += 2 * lineHeight;
		size_t i;
		for (i = start; i < GUIMenuItemListSize(&menu->items); ++i) {
			int color = 0xE0A0A0A0;
			char bullet = ' ';
			if (i == menu->index) {
				color = 0xFFFFFFFF;
				bullet = '>';
			}
			struct GUIMenuItem* item = GUIMenuItemListGetPointer(&menu->items, i);
			GUIFontPrintf(params->font, 0, y, GUI_TEXT_LEFT, color, "%c %s", bullet, item->title);
			if (item->validStates) {
				GUIFontPrintf(params->font, params->width, y, GUI_TEXT_RIGHT, color, "%s ", item->validStates[item->state]);
			}
			y += lineHeight;
			if (y + lineHeight > params->height) {
				break;
			}
		}
		if (params->guiFinish) {
			params->guiFinish();
		}
		params->drawEnd();
	}
	return GUI_MENU_EXIT_CANCEL;
}
