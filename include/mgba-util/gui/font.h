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

unsigned GUIFontHeight(const struct GUIFont*);
unsigned GUIFontGlyphWidth(const struct GUIFont*, uint32_t glyph);
unsigned GUIFontSpanWidth(const struct GUIFont*, const char* text);
void GUIFontIconMetrics(const struct GUIFont*, enum GUIIcon icon, unsigned* w, unsigned* h);

ATTRIBUTE_FORMAT(printf, 6, 7)
void GUIFontPrintf(const struct GUIFont*, int x, int y, enum GUIAlignment, uint32_t color, const char* text, ...);
void GUIFontPrint(const struct GUIFont*, int x, int y, enum GUIAlignment, uint32_t color, const char* text);
void GUIFontDrawGlyph(const struct GUIFont*, int x, int y, uint32_t color, uint32_t glyph);
void GUIFontDrawIcon(const struct GUIFont*, int x, int y, enum GUIAlignment, enum GUIOrientation, uint32_t color, enum GUIIcon);
void GUIFontDrawIconSize(const struct GUIFont* font, int x, int y, int w, int h, uint32_t color, enum GUIIcon icon);

CXX_GUARD_END

#endif
