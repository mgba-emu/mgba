/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/gui/font.h>

#include <mgba-util/string.h>

unsigned GUIFontSpanWidth(const struct GUIFont* font, const char* text) {
	unsigned width = 0;
	size_t len = strlen(text);
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
		case 0x23E9:
			c = GUI_ICON_STATUS_FAST_FORWARD;
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
