/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/gui/menu.h>

#include <mgba-util/gui.h>
#include <mgba-util/gui/font.h>

#ifdef __3DS__
#include <3ds.h>
#elif defined(__SWITCH__)
#include <switch.h>
#endif

DEFINE_VECTOR(GUIMenuItemList, struct GUIMenuItem);
DEFINE_VECTOR(GUIMenuSavedList, struct GUIMenuSavedState);

void _itemNext(struct GUIMenuItem* item, bool wrap) {
	if (item->state < item->nStates - 1) {
		unsigned oldState = item->state;
		do {
			++item->state;
		} while (!item->validStates[item->state] && item->state < item->nStates - 1);
		if (!item->validStates[item->state]) {
			item->state = oldState;
		}
	} else if (wrap) {
		item->state = 0;
	}
}

void _itemPrev(struct GUIMenuItem* item, bool wrap) {
	if (item->state > 0) {
		unsigned oldState = item->state;
		do {
			--item->state;
		} while (!item->validStates[item->state] && item->state > 0);
		if (!item->validStates[item->state]) {
			item->state = oldState;
		}
	} else if (wrap) {
		item->state = item->nStates - 1;
	}
}

void GUIMenuStateInit(struct GUIMenuState* state) {
	state->start = 0;
	state->cursorOverItem = 0;
	state->cursor = GUI_CURSOR_NOT_PRESENT;
	state->resultItem = NULL;
	GUIMenuSavedListInit(&state->stack, 0);
}

void GUIMenuStateDeinit(struct GUIMenuState* state) {
	GUIMenuSavedListDeinit(&state->stack);
}

enum GUIMenuExitReason GUIShowMenu(struct GUIParams* params, struct GUIMenu* menu, struct GUIMenuItem** item) {
	struct GUIMenuState state;
	GUIMenuStateInit(&state);
	GUIInvalidateKeys(params);
	while (true) {
#ifdef __3DS__
		if (!aptMainLoop()) {
			return GUI_MENU_EXIT_CANCEL;
		}
#elif defined(__SWITCH__)
		if (!appletMainLoop()) {
			return GUI_MENU_EXIT_CANCEL;
		}
#endif
		enum GUIMenuExitReason reason = GUIMenuRun(params, menu, &state);
		switch (reason) {
		case GUI_MENU_EXIT_BACK:
			if (GUIMenuSavedListSize(&state.stack) > 0) {
				struct GUIMenuSavedState* last = GUIMenuSavedListGetPointer(&state.stack, GUIMenuSavedListSize(&state.stack) - 1);
				state.start = last->start;
				menu = last->menu;
				GUIMenuSavedListResize(&state.stack, -1);
				break;
			}
		// Fall through
		case GUI_MENU_EXIT_CANCEL:
		case GUI_MENU_EXIT_ACCEPT:
			*item = state.resultItem;
			GUIMenuStateDeinit(&state);
			return reason;
		case GUI_MENU_ENTER:
			*GUIMenuSavedListAppend(&state.stack) = (struct GUIMenuSavedState) {
				.start = state.start,
				.menu = menu
			};
			menu = state.resultItem->submenu;
			state.start = 0;
			break;
		case GUI_MENU_CONTINUE:
			break;
		}
	}
}

static enum GUIMenuExitReason GUIMenuPollInput(struct GUIParams* params, struct GUIMenu* menu, struct GUIMenuState* state) {
	size_t lineHeight = GUIFontHeight(params->font);
	size_t pageSize = params->height / lineHeight;
	if (pageSize > 4) {
		pageSize -= 4;
	} else {
		pageSize = 1;
	}
	uint32_t newInput = 0;
	GUIPollInput(params, &newInput, NULL);
	state->cursor = GUIPollCursor(params, &state->cx, &state->cy);

	// Check for new direction presses
	if (newInput & (1 << GUI_INPUT_UP) && menu->index > 0) {
		--menu->index;
	}
	if (newInput & (1 << GUI_INPUT_DOWN) && menu->index < GUIMenuItemListSize(&menu->items) - 1) {
		++menu->index;
	}
	if (newInput & (1 << GUI_INPUT_LEFT)) {
		struct GUIMenuItem* item = GUIMenuItemListGetPointer(&menu->items, menu->index);
		if (item->validStates && !item->readonly) {
			_itemPrev(item, false);
		} else if (menu->index >= pageSize) {
			menu->index -= pageSize;
		} else {
			menu->index = 0;
		}
	}
	if (newInput & (1 << GUI_INPUT_RIGHT)) {
		struct GUIMenuItem* item = GUIMenuItemListGetPointer(&menu->items, menu->index);
		if (item->validStates && !item->readonly) {
			_itemNext(item, false);
		} else if (menu->index + pageSize < GUIMenuItemListSize(&menu->items)) {
			menu->index += pageSize;
		} else {
			menu->index = GUIMenuItemListSize(&menu->items) - 1;
		}
	}

	// Handle cursor movement
	if (state->cursor != GUI_CURSOR_NOT_PRESENT) {
		unsigned cx = state->cx;
		unsigned cy = state->cy;
		if (cx < params->width - 16) {
			int index = (cy / lineHeight) - 2;
			if (index >= 0 && index + state->start < GUIMenuItemListSize(&menu->items)) {
				if (menu->index != index + state->start || !state->cursorOverItem) {
					state->cursorOverItem = 1;
				}
				menu->index = index + state->start;
			} else {
				state->cursorOverItem = 0;
			}
		} else if (state->cursor == GUI_CURSOR_DOWN || state->cursor == GUI_CURSOR_DRAGGING) {
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

	// Move view up if the active item is before the top of the view
	if (menu->index < state->start) {
		state->start = menu->index;
	}
	// Move the view down if the active item is after the bottom of the view
	while ((menu->index - state->start + 4) * lineHeight > params->height) {
		// TODO: Should this loop be replaced with division?
		++state->start;
	}

	// Handle action inputs
	if (newInput & (1 << GUI_INPUT_CANCEL)) {
		return GUI_MENU_EXIT_CANCEL;
	}
	if (newInput & (1 << GUI_INPUT_SELECT) || (state->cursorOverItem == 2 && state->cursor == GUI_CURSOR_CLICKED)) {
		struct GUIMenuItem* item = GUIMenuItemListGetPointer(&menu->items, menu->index);
		if (!item->readonly) {
			if (item->submenu) {
				// Selected menus get shown inline
				state->resultItem = item;
				return GUI_MENU_ENTER;
			} else if (item->validStates && GUIVariantIsString(item->data)) {
				// Selected items with multiple (named) states get scrolled through
				_itemNext(item, true);
			} else {
				// Otherwise tell caller item was accepted
				state->resultItem = item;
				return GUI_MENU_EXIT_ACCEPT;
			}
		}
	}
	if (state->cursorOverItem == 1 && (state->cursor == GUI_CURSOR_UP || state->cursor == GUI_CURSOR_NOT_PRESENT)) {
		state->cursorOverItem = 2;
	}
	if (newInput & (1 << GUI_INPUT_BACK)) {
		return GUI_MENU_EXIT_BACK;
	}

	// No action taken
	return GUI_MENU_CONTINUE;
}

static void GUIMenuDraw(struct GUIParams* params, const struct GUIMenu* menu, const struct GUIMenuState* state) {
	size_t lineHeight = GUIFontHeight(params->font);
	params->drawStart();
	if (menu->background) {
		menu->background->draw(menu->background, GUIMenuItemListGetConstPointer(&menu->items, menu->index)->data.v.p);
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
	unsigned right;
	GUIFontIconMetrics(params->font, GUI_ICON_SCROLLBAR_BUTTON, &right, 0);
	size_t itemsPerScreen = (params->height - y) / lineHeight;
	size_t i;
	for (i = state->start; i < GUIMenuItemListSize(&menu->items); ++i) {
		int color = 0xE0A0A0A0;
		const struct GUIMenuItem* item = GUIMenuItemListGetConstPointer(&menu->items, i);
		if (i == menu->index) {
			color = item->readonly ? 0xD0909090 : 0xFFFFFFFF;
			GUIFontDrawIcon(params->font, lineHeight * 0.8f, y, GUI_ALIGN_BOTTOM | GUI_ALIGN_RIGHT, GUI_ORIENT_0, color, GUI_ICON_POINTER);
		}
		GUIFontPrint(params->font, item->readonly ? lineHeight * 3 / 2 : lineHeight, y, GUI_ALIGN_LEFT, color, item->title);
		if (item->validStates && item->validStates[item->state]) {
			GUIFontPrintf(params->font, params->width - right - 8, y, GUI_ALIGN_RIGHT, color, "%s ", item->validStates[item->state]);
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
		GUIFontIconMetrics(params->font, GUI_ICON_SCROLLBAR_TRACK, &w, 0);
		right = (right - w) / 2;
		GUIFontDrawIconSize(params->font, params->width - right - 8, top, 0, bottom - top, 0xA0FFFFFF, GUI_ICON_SCROLLBAR_TRACK);
		GUIFontDrawIcon(params->font, params->width - 8, top, GUI_ALIGN_HCENTER | GUI_ALIGN_BOTTOM, GUI_ORIENT_VMIRROR, 0xFFFFFFFF, GUI_ICON_SCROLLBAR_BUTTON);
		GUIFontDrawIcon(params->font, params->width - 8, bottom, GUI_ALIGN_HCENTER | GUI_ALIGN_TOP, GUI_ORIENT_0, 0xFFFFFFFF, GUI_ICON_SCROLLBAR_BUTTON);

		y = menu->index * (bottom - top - 16) / GUIMenuItemListSize(&menu->items);
		GUIFontDrawIcon(params->font, params->width - 8, top + y, GUI_ALIGN_HCENTER | GUI_ALIGN_TOP, GUI_ORIENT_0, 0xFFFFFFFF, GUI_ICON_SCROLLBAR_THUMB);
	}

	GUIDrawBattery(params);
	GUIDrawClock(params);

	if (state->cursor != GUI_CURSOR_NOT_PRESENT) {
		GUIFontDrawIcon(params->font, state->cx, state->cy, GUI_ALIGN_HCENTER | GUI_ALIGN_TOP, GUI_ORIENT_0, 0xFFFFFFFF, GUI_ICON_CURSOR);
	}

	if (params->guiFinish) {
		params->guiFinish();
	}
	params->drawEnd();
}

enum GUIMenuExitReason GUIMenuRun(struct GUIParams* params, struct GUIMenu* menu, struct GUIMenuState* state) {
	enum GUIMenuExitReason reason = GUIMenuPollInput(params, menu, state);
	if (reason != GUI_MENU_CONTINUE) {
		return reason;
	}
	GUIMenuDraw(params, menu, state);
	return GUI_MENU_CONTINUE;
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
	if (state == BATTERY_NOT_PRESENT) {
		return;
	}
	uint32_t color = 0xFF000000;
	if ((state & (BATTERY_CHARGING | BATTERY_FULL)) == (BATTERY_CHARGING | BATTERY_FULL)) {
		color |= 0xFFC060;
	} else if (state & BATTERY_CHARGING) {
		color |= 0x60FF60;
	} else if ((state & BATTERY_VALUE) >= BATTERY_HALF) {
		color |= 0xFFFFFF;
	} else if ((state & BATTERY_VALUE) >= BATTERY_LOW) {
		color |= 0x30FFFF;
	} else {
		color |= 0x3030FF;
	}

	enum GUIIcon batteryIcon;
	switch ((state & BATTERY_VALUE) - (state & BATTERY_VALUE) % 25) {
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

	GUIFontDrawIcon(params->font, params->width, GUIFontHeight(params->font) + 2, GUI_ALIGN_RIGHT | GUI_ALIGN_BOTTOM, GUI_ORIENT_0, color, batteryIcon);
	if (state & BATTERY_PERCENTAGE_VALID) {
		unsigned width;
		GUIFontIconMetrics(params->font, batteryIcon, &width, NULL);
		GUIFontPrintf(params->font, params->width - width, GUIFontHeight(params->font), GUI_ALIGN_RIGHT, color, "%u%%", state & BATTERY_VALUE);
	}
}

void GUIDrawClock(struct GUIParams* params) {
	char buffer[32];
	time_t t = time(0);
	struct tm tm;
	localtime_r(&t, &tm);
	strftime(buffer, sizeof(buffer), "%H:%M:%S", &tm);
	GUIFontPrint(params->font, params->width / 2, GUIFontHeight(params->font), GUI_ALIGN_HCENTER, 0xFFFFFFFF, buffer);
}
