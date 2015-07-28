/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/gui/font.h"

#include <vita2d.h>

#define GLYPH_HEIGHT 11
#define GLYPH_WIDTH 14
#define FONT_TRACKING 10
#define CELL_HEIGHT 16
#define CELL_WIDTH 16

extern const uint8_t _binary_font_png_start[];

struct GUIFont {
	vita2d_texture* tex;
};

struct GUIFont* GUIFontCreate(void) {
	struct GUIFont* font = malloc(sizeof(struct GUIFont));
	if (!font) {
		return 0;
	}
	font->tex = vita2d_load_PNG_buffer(_binary_font_png_start);
	return font;
}

void GUIFontDestroy(struct GUIFont* font) {
	vita2d_free_texture(font->tex);
	free(font);
}

int GUIFontHeight(const struct GUIFont* font) {
	UNUSED(font);
	return GLYPH_HEIGHT;
}

void GUIFontPrintf(const struct GUIFont* font, int x, int y, enum GUITextAlignment align, uint32_t color, const char* text, ...) {
	UNUSED(align); // TODO
	char buffer[256];
	va_list args;
	va_start(args, text);
	int len = vsnprintf(buffer, sizeof(buffer), text, args);
	va_end(args);
	int i;
	for (i = 0; i < len; ++i) {
		char c = buffer[i];
		if (c > 0x7F) {
			c = 0;
		}
		vita2d_draw_texture_tint_part(font->tex, x, y - GLYPH_HEIGHT,
			                          (c & 15) * CELL_WIDTH + ((CELL_WIDTH - GLYPH_WIDTH) >> 1),
			                          (c >> 4) * CELL_HEIGHT + ((CELL_HEIGHT - GLYPH_HEIGHT) >> 1),
			                          GLYPH_WIDTH, GLYPH_HEIGHT, color);
		x += FONT_TRACKING;
	}
}
