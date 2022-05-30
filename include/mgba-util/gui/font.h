/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GUI_FONT_H
#define GUI_FONT_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct GUIFont;
struct GUIFont* GUIFontCreate(void);
void GUIFontDestroy(struct GUIFont*);

enum GUIAlignment {
	GUI_ALIGN_LEFT = 1,
	GUI_ALIGN_HCENTER = 3,
	GUI_ALIGN_RIGHT = 2,

	GUI_ALIGN_TOP = 4,
	GUI_ALIGN_VCENTER = 12,
	GUI_ALIGN_BOTTOM = 8,
};

enum GUIOrientation {
	GUI_ORIENT_0,
	GUI_ORIENT_90_CCW,
	GUI_ORIENT_180,
	GUI_ORIENT_270_CCW,

	GUI_ORIENT_VMIRROR,
	GUI_ORIENT_HMIRROR,

	GUI_ORIENT_90_CW = GUI_ORIENT_270_CCW,
	GUI_ORIENT_270_CW = GUI_ORIENT_90_CCW
};

enum GUIIcon {
	GUI_ICON_BATTERY_FULL,
	GUI_ICON_BATTERY_HIGH,
	GUI_ICON_BATTERY_HALF,
	GUI_ICON_BATTERY_LOW,
	GUI_ICON_BATTERY_EMPTY,
	GUI_ICON_SCROLLBAR_THUMB,
	GUI_ICON_SCROLLBAR_TRACK,
	GUI_ICON_SCROLLBAR_BUTTON,
	GUI_ICON_CURSOR,
	GUI_ICON_POINTER,
	GUI_ICON_BUTTON_CIRCLE,
	GUI_ICON_BUTTON_CROSS,
	GUI_ICON_BUTTON_TRIANGLE,
	GUI_ICON_BUTTON_SQUARE,
	GUI_ICON_BUTTON_HOME,
	GUI_ICON_STATUS_FAST_FORWARD,
	GUI_ICON_STATUS_MUTE,
	GUI_ICON_LEFT,
	GUI_ICON_UP,
	GUI_ICON_RIGHT,
	GUI_ICON_DOWN,
	GUI_ICON_9SLICE_EMPTY_NW,
	GUI_ICON_9SLICE_EMPTY_N,
	GUI_ICON_9SLICE_EMPTY_NE,
	GUI_ICON_9SLICE_EMPTY_W,
	GUI_ICON_9SLICE_EMPTY_E,
	GUI_ICON_9SLICE_EMPTY_SW,
	GUI_ICON_9SLICE_EMPTY_S,
	GUI_ICON_9SLICE_EMPTY_SE,
	GUI_ICON_9SLICE_FILLED_NW,
	GUI_ICON_9SLICE_FILLED_N,
	GUI_ICON_9SLICE_FILLED_NE,
	GUI_ICON_9SLICE_FILLED_W,
	GUI_ICON_9SLICE_FILLED_C,
	GUI_ICON_9SLICE_FILLED_E,
	GUI_ICON_9SLICE_FILLED_SW,
	GUI_ICON_9SLICE_FILLED_S,
	GUI_ICON_9SLICE_FILLED_SE,
	GUI_ICON_9SLICE_CAP_NNW,
	GUI_ICON_9SLICE_CAP_NWW,
	GUI_ICON_9SLICE_CAP_NNE,
	GUI_ICON_9SLICE_CAP_NEE,
	GUI_ICON_9SLICE_CAP_SSW,
	GUI_ICON_9SLICE_CAP_SWW,
	GUI_ICON_9SLICE_CAP_SSE,
	GUI_ICON_9SLICE_CAP_SEE,
	GUI_ICON_9SLICE_FILL_ONLY_NW,
	GUI_ICON_9SLICE_FILL_ONLY_N,
	GUI_ICON_9SLICE_FILL_ONLY_NE,
	GUI_ICON_9SLICE_FILL_ONLY_W,
	GUI_ICON_9SLICE_FILL_ONLY_C,
	GUI_ICON_9SLICE_FILL_ONLY_E,
	GUI_ICON_9SLICE_FILL_ONLY_SW,
	GUI_ICON_9SLICE_FILL_ONLY_S,
	GUI_ICON_9SLICE_FILL_ONLY_SE,
	GUI_ICON_BACKSPACE,
	GUI_ICON_KBD_SHIFT,
	GUI_ICON_CAPSLOCK,
	GUI_ICON_TEXT_CURSOR,
	GUI_ICON_MAX,
};

struct GUIFontGlyphMetric {
	int width;
	int height;
	struct {
		int top;
		int right;
		int bottom;
		int left;
	} padding;
};

struct GUIIconMetric {
	int x;
	int y;
	int width;
	int height;
};

enum GUI9SliceStyle {
	GUI_9SLICE_FILLED,
	GUI_9SLICE_EMPTY,
	GUI_9SLICE_EMPTY_CAPPED,
	GUI_9SLICE_FILL_ONLY,
};

unsigned GUIFontHeight(const struct GUIFont*);
unsigned GUIFontGlyphWidth(const struct GUIFont*, uint32_t glyph);
unsigned GUIFontSpanWidth(const struct GUIFont*, const char* text);
unsigned GUIFontSpanCountWidth(const struct GUIFont*, const char* text, size_t len);
void GUIFontIconMetrics(const struct GUIFont*, enum GUIIcon icon, unsigned* w, unsigned* h);

ATTRIBUTE_FORMAT(printf, 6, 7)
void GUIFontPrintf(struct GUIFont*, int x, int y, enum GUIAlignment, uint32_t color, const char* text, ...);
void GUIFontPrint(struct GUIFont*, int x, int y, enum GUIAlignment, uint32_t color, const char* text);
void GUIFontDrawGlyph(struct GUIFont*, int x, int y, uint32_t color, uint32_t glyph);
void GUIFontDrawIcon(struct GUIFont*, int x, int y, enum GUIAlignment, enum GUIOrientation, uint32_t color, enum GUIIcon);
void GUIFontDrawIconSize(struct GUIFont* font, int x, int y, int w, int h, uint32_t color, enum GUIIcon icon);

#ifdef __SWITCH__
void GUIFontDrawSubmit(struct GUIFont* font);
#endif

void GUIFontDraw9Slice(struct GUIFont*, int x, int y, int width, int height, uint32_t color, enum GUI9SliceStyle style);

CXX_GUARD_END

#endif
