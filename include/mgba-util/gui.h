/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GUI_H
#define GUI_H

#include <mgba-util/common.h>

CXX_GUARD_START

// TODO: Fix layering violation
#include <mgba/core/input.h>
#include <mgba-util/vector.h>

#define MAX_KEYBOARD_LEN 256
#define MAX_KEYBOARD_TITLE_LEN 128

struct GUIFont;

enum GUIInput {
	GUI_INPUT_NONE = -1,
	GUI_INPUT_SELECT = 0,
	GUI_INPUT_BACK,
	GUI_INPUT_CANCEL,

	GUI_INPUT_UP,
	GUI_INPUT_DOWN,
	GUI_INPUT_LEFT,
	GUI_INPUT_RIGHT,

	GUI_INPUT_USER_START = 0x8,

	GUI_INPUT_MAX = 0x20
};

enum GUICursorState {
	GUI_CURSOR_NOT_PRESENT = 0,
	GUI_CURSOR_UP,
	GUI_CURSOR_DOWN,
	GUI_CURSOR_CLICKED,
	GUI_CURSOR_DRAGGING
};

enum GUIKeyboardStatus {
	GUI_KEYBOARD_DONE = 0,
	GUI_KEYBOARD_CANCEL,
};

enum GUIKeyFunction {
	GUI_KEYFUNC_INPUT_DATA = 0,
	GUI_KEYFUNC_CHANGE_KB,
	GUI_KEYFUNC_SHIFT_KB,
	GUI_KEYFUNC_BACKSPACE,
	GUI_KEYFUNC_ENTER,
	GUI_KEYFUNC_CANCEL,
	GUI_KEYFUNC_LEFT,
	GUI_KEYFUNC_RIGHT,
};

enum {
	BATTERY_EMPTY = 0,
	BATTERY_LOW = 25,
	BATTERY_HALF = 50,
	BATTERY_HIGH = 75,
	BATTERY_FULL = 100,
	BATTERY_VALUE = 0x7F,
	BATTERY_PERCENTAGE_VALID = 0x80,

	BATTERY_CHARGING = 0x100,
	BATTERY_NOT_PRESENT = 0x200,
};

struct GUIBackground {
	void (*draw)(struct GUIBackground*, void* context);
};

struct GUIKeyboardParams {
	char title[MAX_KEYBOARD_TITLE_LEN];
	char result[MAX_KEYBOARD_LEN];
	size_t maxLen;
	bool multiline;
};

struct GUIKey {
	const char* name;
	const void* data;
	int width;
	enum GUIKeyFunction function;
};

struct GUIKeyboard {
	struct {
		int offset;
		struct GUIKey* keys;
	} rows[5];
	int width;
};

struct GUIParams {
	unsigned width;
	unsigned height;
	struct GUIFont* font;
	const char* basePath;

	void (*drawStart)(void);
	void (*drawEnd)(void);
	uint32_t (*pollInput)(const struct mInputMap*);
	enum GUICursorState (*pollCursor)(unsigned* x, unsigned* y);
	int (*batteryState)(void);
	void (*guiPrepare)(void);
	void (*guiFinish)(void);
	enum GUIKeyboardStatus (*getText)(struct GUIKeyboardParams*);

	// State
	struct mInputMap keyMap;
	int inputHistory[GUI_INPUT_MAX];
	enum GUICursorState cursorState;
	int cx, cy;

	// Directories
	char currentPath[PATH_MAX];
	size_t fileIndex;
};

void GUIInit(struct GUIParams* params);
void GUIPollInput(struct GUIParams* params, uint32_t* newInput, uint32_t* heldInput);
enum GUICursorState GUIPollCursor(struct GUIParams* params, unsigned* x, unsigned* y);
void GUIInvalidateKeys(struct GUIParams* params);

void GUIKeyboardParamsInit(struct GUIKeyboardParams*);

CXX_GUARD_END

#endif
