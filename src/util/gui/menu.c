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
					unsigned oldState = item->state;
					do {
						--item->state;
					} while (!item->validStates[item->state] && item->state > 0);
					if (!item->validStates[item->state]) {
						item->state = oldState;
					}
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
				if (item->state < item->nStates - 1) {
					unsigned oldState = item->state;
					do {
						++item->state;
					} while (!item->validStates[item->state] && item->state < item->nStates - 1);
					if (!item->validStates[item->state]) {
						item->state = oldState;
					}
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
		if (menu->subtitle) {
			GUIFontPrint(params->font, 0, y * 2, GUI_TEXT_LEFT, 0xFFFFFFFF, menu->subtitle);
		}
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
			if (item->validStates && item->validStates[item->state]) {
				GUIFontPrintf(params->font, params->width, y, GUI_TEXT_RIGHT, color, "%s ", item->validStates[item->state]);
			}
			y += lineHeight;
			if (y + lineHeight > params->height) {
				break;
			}
		}

		GUIDrawBattery(params);
		GUIDrawClock(params);

		if (params->guiFinish) {
			params->guiFinish();
		}
		params->drawEnd();
	}
	return GUI_MENU_EXIT_CANCEL;
}

enum GUICursorState GUIPollCursor(struct GUIParams* params, int* x, int* y) {
	if (!params->pollCursor) {
		return GUI_CURSOR_NOT_PRESENT;
	}
	enum GUICursorState state = params->pollCursor(x, y);
	if (params->cursorState == GUI_CURSOR_DOWN) {
		int dragX = *x - params->cx;
		int dragY = *y - params->cy;
		if (dragX * dragX + dragY * dragY > 25) {
			params->cursorState = GUI_CURSOR_DRAGGING;
			return GUI_CURSOR_DRAGGING;
		}
		if (state == GUI_CURSOR_UP || state == GUI_CURSOR_NOT_PRESENT) {
			params->cursorState = GUI_CURSOR_UP;
			return GUI_CURSOR_CLICKED;
		}
	} else {
		params->cx = *x;
		params->cy = *y;
	}
	if (params->cursorState == GUI_CURSOR_DRAGGING) {
		if (state == GUI_CURSOR_UP || state == GUI_CURSOR_NOT_PRESENT) {
			params->cursorState = GUI_CURSOR_UP;
			return GUI_CURSOR_UP;
		}
		return GUI_CURSOR_DRAGGING;
	}
	params->cursorState = state;
	return params->cursorState;
}

void GUIInvalidateKeys(struct GUIParams* params) {
	for (int i = 0; i < GUI_INPUT_MAX; ++i) {
		params->inputHistory[i] = 0;
	}
}

void GUIDrawBattery(struct GUIParams* params) {
	if (!params->batteryState) {
		return;
	}
	int state = params->batteryState();
	uint32_t color = 0xFF000000;
	if (state == (BATTERY_CHARGING | BATTERY_FULL)) {
		color |= 0xFFC060;
	} else if (state & BATTERY_CHARGING) {
		color |= 0x60FF60;
	} else if (state >= BATTERY_HALF) {
		color |= 0xFFFFFF;
	} else if (state == BATTERY_LOW) {
		color |= 0x30FFFF;
	} else {
		color |= 0x3030FF;
	}

	const char* batteryText;
	switch (state & ~BATTERY_CHARGING) {
	case BATTERY_EMPTY:
		batteryText = "[    ]";
		break;
	case BATTERY_LOW:
		batteryText = "[I   ]";
		break;
	case BATTERY_HALF:
		batteryText = "[II  ]";
		break;
	case BATTERY_HIGH:
		batteryText = "[III ]";
		break;
	case BATTERY_FULL:
		batteryText = "[IIII]";
		break;
	default:
		batteryText = "[????]";
		break;
	}

	GUIFontPrint(params->font, params->width, GUIFontHeight(params->font), GUI_TEXT_RIGHT, color, batteryText);
}

void GUIDrawClock(struct GUIParams* params) {
	char buffer[32];
	time_t t = time(0);
	struct tm tm;
	localtime_r(&t, &tm);
	strftime(buffer, sizeof(buffer), "%H:%M:%S", &tm);
	GUIFontPrint(params->font, params->width / 2, GUIFontHeight(params->font), GUI_TEXT_CENTER, 0xFFFFFFFF, buffer);
}
