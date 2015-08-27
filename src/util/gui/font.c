/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/gui/font.h"

unsigned GUIFontSpanWidth(const struct GUIFont* font, const char* text) {
	unsigned width = 0;
	size_t i;
	for (i = 0; text[i]; ++i) {
		char c = text[i];
		width += GUIFontGlyphWidth(font, c);
	}
	return width;
}

void GUIFontPrint(const struct GUIFont* font, int x, int y, enum GUITextAlignment align, uint32_t color, const char* text) {
	switch (align) {
	case GUI_TEXT_CENTER:
		x -= GUIFontSpanWidth(font, text) / 2;
		break;
	case GUI_TEXT_RIGHT:
		x -= GUIFontSpanWidth(font, text);
		break;
	default:
		break;
	}
	size_t i;
	for (i = 0; text[i]; ++i) {
		char c = text[i];
		GUIFontDrawGlyph(font, x, y, color, c);
		x += GUIFontGlyphWidth(font, c);
	}
}

void GUIFontPrintf(const struct GUIFont* font, int x, int y, enum GUITextAlignment align, uint32_t color, const char* text, ...) {
	char buffer[256];
	va_list args;
	va_start(args, text);
	vsnprintf(buffer, sizeof(buffer), text, args);
	va_end(args);
	GUIFontPrint(font, x, y, align, color, buffer);
}
