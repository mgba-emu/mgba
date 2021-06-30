/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/gui.h>

#define KEY_DELAY 45
#define KEY_REPEAT 5

void GUIInit(struct GUIParams* params) {
	memset(params->inputHistory, 0, sizeof(params->inputHistory));
	strncpy(params->currentPath, params->basePath, PATH_MAX);
}

void GUIPollInput(struct GUIParams* params, uint32_t* newInputOut, uint32_t* heldInput) {
	uint32_t input = params->pollInput(&params->keyMap);
	uint32_t newInput = 0;
	int i;
	for (i = 0; i < GUI_INPUT_MAX; ++i) {
		if (input & (1 << i)) {
			++params->inputHistory[i];
		} else {
			params->inputHistory[i] = -1;
		}
		if (!params->inputHistory[i] || (params->inputHistory[i] >= KEY_DELAY && !(params->inputHistory[i] % KEY_REPEAT))) {
			newInput |= (1 << i);
		}
	}
	if (newInputOut) {
		*newInputOut = newInput;
	}
	if (heldInput) {
		*heldInput = input;
	}
}

enum GUICursorState GUIPollCursor(struct GUIParams* params, unsigned* x, unsigned* y) {
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
	int i;
	for (i = 0; i < GUI_INPUT_MAX; ++i) {
		params->inputHistory[i] = 0;
	}
}

void GUIKeyboardParamsInit(struct GUIKeyboardParams* keyboard) {
	memset(keyboard->title, 0, sizeof(keyboard->title));
	memset(keyboard->result, 0, sizeof(keyboard->result));
	keyboard->maxLen = sizeof(keyboard->result);
	keyboard->multiline = false;
}
