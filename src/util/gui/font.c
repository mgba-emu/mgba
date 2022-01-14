/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/gui/font.h>

#include <mgba-util/string.h>

unsigned GUIFontSpanWidth(const struct GUIFont* font, const char* text) {
	size_t len = strlen(text);
	return GUIFontSpanCountWidth(font, text, len);
}

unsigned GUIFontSpanCountWidth(const struct GUIFont* font, const char* text, size_t len) {
	unsigned width = 0;
	while (len) {
		uint32_t c = utf8Char(&text, &len);
		if (c == '\1') {
			c = utf8Char(&text, &len);
			if (c < GUI_ICON_MAX) {
				unsigned w;
				GUIFontIconMetrics(font, c, &w, 0);
				width += w;
			}
		} else {
			width += GUIFontGlyphWidth(font, c);
		}
	}
	return width;
}

void GUIFontPrint(struct GUIFont* font, int x, int y, enum GUIAlignment align, uint32_t color, const char* text) {
	switch (align & GUI_ALIGN_HCENTER) {
	case GUI_ALIGN_HCENTER:
		x -= GUIFontSpanWidth(font, text) / 2;
		break;
	case GUI_ALIGN_RIGHT:
		x -= GUIFontSpanWidth(font, text);
		break;
	default:
		break;
	}
	size_t len = strlen(text);
	while (len) {
		uint32_t c = utf8Char(&text, &len);
		bool icon = false;
		switch (c) {
		case 1:
			c = utf8Char(&text, &len);
			if (c < GUI_ICON_MAX) {
				icon = true;
			}
			break;
		case 0x2190:
		case 0x2191:
		case 0x2192:
		case 0x2193:
			c = GUI_ICON_LEFT + c - 0x2190;
			icon = true;
			break;
		case 0x232B:
			c = GUI_ICON_BACKSPACE;
			icon = true;
			break;
		case 0x23E9:
			c = GUI_ICON_STATUS_FAST_FORWARD;
			icon = true;
			break;
		case 0x21E7:
			c = GUI_ICON_KBD_SHIFT;
			icon = true;
			break;
		case 0x21EA:
			c = GUI_ICON_CAPSLOCK;
			icon = true;
			break;
		case 0x1F507:
			c = GUI_ICON_STATUS_MUTE;
			icon = true;
			break;
		default:
			GUIFontDrawGlyph(font, x, y, color, c);
			x += GUIFontGlyphWidth(font, c);
			break;
		}

		if (icon) {
			GUIFontDrawIcon(font, x, y, GUI_ALIGN_BOTTOM, GUI_ORIENT_0, color, c);
			unsigned w;
			GUIFontIconMetrics(font, c, &w, 0);
			x += w;
		}
	}
}

void GUIFontPrintf(struct GUIFont* font, int x, int y, enum GUIAlignment align, uint32_t color, const char* text, ...) {
	char buffer[256];
	va_list args;
	va_start(args, text);
	vsnprintf(buffer, sizeof(buffer), text, args);
	va_end(args);
	GUIFontPrint(font, x, y, align, color, buffer);
}

void GUIFontDraw9Slice(struct GUIFont* font, int x, int y, int width, int height, uint32_t color, enum GUI9SliceStyle style) {
	switch (style) {
	case GUI_9SLICE_EMPTY:
	case GUI_9SLICE_EMPTY_CAPPED:
		GUIFontDrawIcon(font, x        , y         , GUI_ALIGN_LEFT  | GUI_ALIGN_TOP   , GUI_ORIENT_0, color, GUI_ICON_9SLICE_EMPTY_NW);
		GUIFontDrawIcon(font, x + width, y         , GUI_ALIGN_RIGHT | GUI_ALIGN_TOP   , GUI_ORIENT_0, color, GUI_ICON_9SLICE_EMPTY_NE);
		GUIFontDrawIcon(font, x        , y + height, GUI_ALIGN_LEFT  | GUI_ALIGN_BOTTOM, GUI_ORIENT_0, color, GUI_ICON_9SLICE_EMPTY_SW);
		GUIFontDrawIcon(font, x + width, y + height, GUI_ALIGN_RIGHT | GUI_ALIGN_BOTTOM, GUI_ORIENT_0, color, GUI_ICON_9SLICE_EMPTY_SE);
		break;
	case GUI_9SLICE_FILLED:
		GUIFontDrawIcon(font, x        , y         , GUI_ALIGN_LEFT  | GUI_ALIGN_TOP   , GUI_ORIENT_0, color, GUI_ICON_9SLICE_FILLED_NW);
		GUIFontDrawIcon(font, x + width, y         , GUI_ALIGN_RIGHT | GUI_ALIGN_TOP   , GUI_ORIENT_0, color, GUI_ICON_9SLICE_FILLED_NE);
		GUIFontDrawIcon(font, x        , y + height, GUI_ALIGN_LEFT  | GUI_ALIGN_BOTTOM, GUI_ORIENT_0, color, GUI_ICON_9SLICE_FILLED_SW);
		GUIFontDrawIcon(font, x + width, y + height, GUI_ALIGN_RIGHT | GUI_ALIGN_BOTTOM, GUI_ORIENT_0, color, GUI_ICON_9SLICE_FILLED_SE);
		break;
	case GUI_9SLICE_FILL_ONLY:
		GUIFontDrawIcon(font, x        , y         , GUI_ALIGN_LEFT  | GUI_ALIGN_TOP   , GUI_ORIENT_0, color, GUI_ICON_9SLICE_FILL_ONLY_NW);
		GUIFontDrawIcon(font, x + width, y         , GUI_ALIGN_RIGHT | GUI_ALIGN_TOP   , GUI_ORIENT_0, color, GUI_ICON_9SLICE_FILL_ONLY_NE);
		GUIFontDrawIcon(font, x        , y + height, GUI_ALIGN_LEFT  | GUI_ALIGN_BOTTOM, GUI_ORIENT_0, color, GUI_ICON_9SLICE_FILL_ONLY_SW);
		GUIFontDrawIcon(font, x + width, y + height, GUI_ALIGN_RIGHT | GUI_ALIGN_BOTTOM, GUI_ORIENT_0, color, GUI_ICON_9SLICE_FILL_ONLY_SE);
		break;
	}

	unsigned offX, offY;
	unsigned endX, endY;
	GUIFontIconMetrics(font, GUI_ICON_9SLICE_EMPTY_NW, &offX, &offY);
	GUIFontIconMetrics(font, GUI_ICON_9SLICE_EMPTY_SE, &endX, &endY);

	switch (style) {
	case GUI_9SLICE_EMPTY:
		GUIFontDrawIconSize(font, x + offX, y, width - offX - endX, offY, color, GUI_ICON_9SLICE_EMPTY_N);
		GUIFontDrawIconSize(font, x, y + offY, offX, height - offY - endY, color, GUI_ICON_9SLICE_EMPTY_W);
		GUIFontDrawIconSize(font, x + offX, y + height - endY, width - offX - endX, offY, color, GUI_ICON_9SLICE_EMPTY_S);
		GUIFontDrawIconSize(font, x + width - endX, y + offY, offX, height - offY - endY, color, GUI_ICON_9SLICE_EMPTY_E);
		break;
	case GUI_9SLICE_FILLED:
		GUIFontDrawIconSize(font, x + offX, y, width - offX - endX, offY, color, GUI_ICON_9SLICE_FILLED_N);
		GUIFontDrawIconSize(font, x, y + offY, offX, height - offY - endY, color, GUI_ICON_9SLICE_FILLED_W);
		GUIFontDrawIconSize(font, x + offX, y + height - endY, width - offX - endX, offY, color, GUI_ICON_9SLICE_FILLED_S);
		GUIFontDrawIconSize(font, x + width - endX, y + offY, offX, height - offY - endY, color, GUI_ICON_9SLICE_FILLED_E);
		GUIFontDrawIconSize(font, x + offX, y + offY, width - offX - endX, height - offY - endY, color, GUI_ICON_9SLICE_FILLED_C);
		break;
	case GUI_9SLICE_FILL_ONLY:
		GUIFontDrawIconSize(font, x + offX, y, width - offX - endX, offY, color, GUI_ICON_9SLICE_FILL_ONLY_N);
		GUIFontDrawIconSize(font, x, y + offY, offX, height - offY - endY, color, GUI_ICON_9SLICE_FILL_ONLY_W);
		GUIFontDrawIconSize(font, x + offX, y + height - endY, width - offX - endX, offY, color, GUI_ICON_9SLICE_FILL_ONLY_S);
		GUIFontDrawIconSize(font, x + width - endX, y + offY, offX, height - offY - endY, color, GUI_ICON_9SLICE_FILL_ONLY_E);
		GUIFontDrawIconSize(font, x + offX, y + offY, width - offX - endX, height - offY - endY, color, GUI_ICON_9SLICE_FILL_ONLY_C);
		break;
	case GUI_9SLICE_EMPTY_CAPPED:
		GUIFontDrawIcon(font, x + offX, y, GUI_ALIGN_LEFT | GUI_ALIGN_TOP, GUI_ORIENT_0, color, GUI_ICON_9SLICE_CAP_NNW);
		GUIFontDrawIcon(font, x, y + offY, GUI_ALIGN_LEFT | GUI_ALIGN_TOP, GUI_ORIENT_0, color, GUI_ICON_9SLICE_CAP_NWW);

		GUIFontDrawIcon(font, x + width - endX, y, GUI_ALIGN_RIGHT | GUI_ALIGN_TOP, GUI_ORIENT_0, color, GUI_ICON_9SLICE_CAP_NNE);
		GUIFontDrawIcon(font, x + width, y + offY, GUI_ALIGN_RIGHT | GUI_ALIGN_TOP, GUI_ORIENT_0, color, GUI_ICON_9SLICE_CAP_NEE);

		GUIFontDrawIcon(font, x + offX, y + height, GUI_ALIGN_LEFT | GUI_ALIGN_BOTTOM, GUI_ORIENT_0, color, GUI_ICON_9SLICE_CAP_SSW);
		GUIFontDrawIcon(font, x, y + height - endY, GUI_ALIGN_LEFT | GUI_ALIGN_BOTTOM, GUI_ORIENT_0, color, GUI_ICON_9SLICE_CAP_SWW);

		GUIFontDrawIcon(font, x + width - endX, y + height, GUI_ALIGN_RIGHT | GUI_ALIGN_BOTTOM, GUI_ORIENT_0, color, GUI_ICON_9SLICE_CAP_SSE);
		GUIFontDrawIcon(font, x + width, y + height - endY, GUI_ALIGN_RIGHT | GUI_ALIGN_BOTTOM, GUI_ORIENT_0, color, GUI_ICON_9SLICE_CAP_SEE);
		break;
	}
}
