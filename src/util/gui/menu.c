/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/gui/menu.h>

#include <mgba-util/gui.h>
#include <mgba-util/gui/font.h>

#ifdef _3DS
#include <3ds.h>
#endif

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
#ifdef _3DS
		if (!aptMainLoop()) {
			return GUI_MENU_EXIT_CANCEL;
		}
#endif
		uint32_t newInput = 0;
		GUIPollInput(params, &newInput, 0);
		unsigned cx, cy;
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
			if (cx < params->width - 16) {
				int index = (cy / lineHeight) - 2;
				if (index >= 0 && index + start < GUIMenuItemListSize(&menu->items)) {
					if (menu->index != index + start || !cursorOverItem) {
						cursorOverItem = 1;
					}
					menu->index = index + start;
				} else {
					cursorOverItem = 0;
				}
			} else if (cursor == GUI_CURSOR_DOWN || cursor == GUI_CURSOR_DRAGGING) {
				if (cy <= 2 * lineHeight && cy > lineHeight && menu->index > 0) {
					--menu->index;
				} else if (cy <= params->height && cy > params->height - lineHeight && menu->index < GUIMenuItemListSize(&menu->items) - 1) {
					++menu->index;
				} else if (cy <= params->height - lineHeight && cy > 2 * lineHeight) {
					size_t location = cy - 2 * lineHeight;
					location *= GUIMenuItemListSize(&menu->items) - 1;
					menu->index = location / (params->height - 3 * lineHeight);
				}
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
		GUIFontPrint(params->font, 0, y, GUI_ALIGN_LEFT, 0xFFFFFFFF, menu->title);
		if (menu->subtitle) {
			GUIFontPrint(params->font, 0, y * 2, GUI_ALIGN_LEFT, 0xFFFFFFFF, menu->subtitle);
		}
		y += 2 * lineHeight;
		size_t itemsPerScreen = (params->height - y) / lineHeight;
		size_t i;
		for (i = start; i < GUIMenuItemListSize(&menu->items); ++i) {
			int color = 0xE0A0A0A0;
			if (i == menu->index) {
				color = 0xFFFFFFFF;
				GUIFontDrawIcon(params->font, lineHeight * 0.8f, y, GUI_ALIGN_BOTTOM | GUI_ALIGN_RIGHT, GUI_ORIENT_0, 0xFFFFFFFF, GUI_ICON_POINTER);
			}
			struct GUIMenuItem* item = GUIMenuItemListGetPointer(&menu->items, i);
			GUIFontPrint(params->font, lineHeight, y, GUI_ALIGN_LEFT, color, item->title);
			if (item->validStates && item->validStates[item->state]) {
				GUIFontPrintf(params->font, params->width, y, GUI_ALIGN_RIGHT, color, "%s ", item->validStates[item->state]);
			}
			y += lineHeight;
			if (y + lineHeight > params->height) {
				break;
			}
		}

		if (itemsPerScreen < GUIMenuItemListSize(&menu->items)) {
			size_t top = 2 * lineHeight;
			size_t bottom = params->height - 8;
			unsigned w;
			unsigned right;
			GUIFontIconMetrics(params->font, GUI_ICON_SCROLLBAR_BUTTON, &right, 0);
			GUIFontIconMetrics(params->font, GUI_ICON_SCROLLBAR_TRACK, &w, 0);
			right = (right - w) / 2;
			GUIFontDrawIcon(params->font, params->width - 8, top, GUI_ALIGN_HCENTER | GUI_ALIGN_BOTTOM, GUI_ORIENT_VMIRROR, 0xFFFFFFFF, GUI_ICON_SCROLLBAR_BUTTON);
			GUIFontDrawIconSize(params->font, params->width - right - 8, top, 0, bottom - top, 0xFFFFFFFF, GUI_ICON_SCROLLBAR_TRACK);
			GUIFontDrawIcon(params->font, params->width - 8, bottom, GUI_ALIGN_HCENTER | GUI_ALIGN_TOP, GUI_ORIENT_0, 0xFFFFFFFF, GUI_ICON_SCROLLBAR_BUTTON);

			y = menu->index * (bottom - top - 16) / GUIMenuItemListSize(&menu->items);
			GUIFontDrawIcon(params->font, params->width - 8, top + y, GUI_ALIGN_HCENTER | GUI_ALIGN_TOP, GUI_ORIENT_0, 0xFFFFFFFF, GUI_ICON_SCROLLBAR_THUMB);
		}

		GUIDrawBattery(params);
		GUIDrawClock(params);

		if (cursor != GUI_CURSOR_NOT_PRESENT) {
			GUIFontDrawIcon(params->font, cx, cy, GUI_ALIGN_HCENTER | GUI_ALIGN_TOP, GUI_ORIENT_0, 0xFFFFFFFF, GUI_ICON_CURSOR);
		}

		if (params->guiFinish) {
			params->guiFinish();
		}
		params->drawEnd();
	}
	return GUI_MENU_EXIT_CANCEL;
}

enum GUIMenuExitReason GUIShowMessageBox(struct GUIParams* params, int buttons, int frames, const char* format, ...) {
	va_list args;
	va_start(args, format);
	char message[256] = {0};
	vsnprintf(message, sizeof(message) - 1, format, args);
	va_end(args);

	while (true) {
		if (frames) {
			--frames;
			if (!frames) {
				break;
			}
		}
		params->drawStart();
		if (params->guiPrepare) {
			params->guiPrepare();
		}
		GUIFontPrint(params->font, params->width / 2, (GUIFontHeight(params->font) + params->height) / 2, GUI_ALIGN_HCENTER, 0xFFFFFFFF, message);
		if (params->guiFinish) {
			params->guiFinish();
		}
		params->drawEnd();

		uint32_t input = 0;
		GUIPollInput(params, &input, 0);
		if (input) {
			if (input & (1 << GUI_INPUT_SELECT)) {
				if (buttons & GUI_MESSAGE_BOX_OK) {
					return GUI_MENU_EXIT_ACCEPT;
				}
				if (buttons & GUI_MESSAGE_BOX_CANCEL) {
					return GUI_MENU_EXIT_CANCEL;
				}
			}
			if (input & (1 << GUI_INPUT_BACK)) {
				if (buttons & GUI_MESSAGE_BOX_CANCEL) {
					return GUI_MENU_EXIT_BACK;
				}
				if (buttons & GUI_MESSAGE_BOX_OK) {
					return GUI_MENU_EXIT_ACCEPT;
				}
			}
			if (input & (1 << GUI_INPUT_CANCEL)) {
				if (buttons & GUI_MESSAGE_BOX_CANCEL) {
					return GUI_MENU_EXIT_CANCEL;
				}
				if (buttons & GUI_MESSAGE_BOX_OK) {
					return GUI_MENU_EXIT_ACCEPT;
				}
			}
		}
	}
	return GUI_MENU_EXIT_CANCEL;
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

	enum GUIIcon batteryIcon;
	switch (state & ~BATTERY_CHARGING) {
	case BATTERY_EMPTY:
		batteryIcon = GUI_ICON_BATTERY_EMPTY;
		break;
	case BATTERY_LOW:
		batteryIcon = GUI_ICON_BATTERY_LOW;
		break;
	case BATTERY_HALF:
		batteryIcon = GUI_ICON_BATTERY_HALF;
		break;
	case BATTERY_HIGH:
		batteryIcon = GUI_ICON_BATTERY_HIGH;
		break;
	case BATTERY_FULL:
		batteryIcon = GUI_ICON_BATTERY_FULL;
		break;
	default:
		batteryIcon = GUI_ICON_BATTERY_EMPTY;
		break;
	}

	GUIFontDrawIcon(params->font, params->width, 0, GUI_ALIGN_RIGHT, GUI_ORIENT_0, color, batteryIcon);
}

void GUIDrawClock(struct GUIParams* params) {
	char buffer[32];
	time_t t = time(0);
	struct tm tm;
	localtime_r(&t, &tm);
	strftime(buffer, sizeof(buffer), "%H:%M:%S", &tm);
	GUIFontPrint(params->font, params->width / 2, GUIFontHeight(params->font), GUI_ALIGN_HCENTER, 0xFFFFFFFF, buffer);
}
