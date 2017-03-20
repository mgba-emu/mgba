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

void GUIFontPrint(const struct GUIFont* font, int x, int y, enum GUIAlignment align, uint32_t color, const char* text) {
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
		if (c == '\1') {
			c = utf8Char(&text, &len);
			if (c < GUI_ICON_MAX) {
				GUIFontDrawIcon(font, x, y, GUI_ALIGN_BOTTOM, GUI_ORIENT_0, color, c);
				unsigned w;
				GUIFontIconMetrics(font, c, &w, 0);
				x += w;
			}
		} else {
			GUIFontDrawGlyph(font, x, y, color, c);
			x += GUIFontGlyphWidth(font, c);
		}
	}
}

void GUIFontPrintf(const struct GUIFont* font, int x, int y, enum GUIAlignment align, uint32_t color, const char* text, ...) {
	char buffer[256];
	va_list args;
	va_start(args, text);
	vsnprintf(buffer, sizeof(buffer), text, args);
	va_end(args);
	GUIFontPrint(font, x, y, align, color, buffer);
}
