/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/gui/font.h"
#include "util/gui/font-metrics.h"
#include "util/png-io.h"
#include "util/vfs.h"
#include "font.h"

#include <sf2d.h>

#define CELL_HEIGHT 16
#define CELL_WIDTH 16
#define GLYPH_HEIGHT 12

struct GUIFont {
	sf2d_texture* tex;
};

struct GUIFont* GUIFontCreate(void) {
	struct GUIFont* guiFont = malloc(sizeof(struct GUIFont));
	if (!guiFont) {
		return 0;
	}
	guiFont->tex = sf2d_create_texture(256, 128, TEXFMT_RGB5A1, SF2D_PLACE_RAM);
	memcpy(guiFont->tex->data, font, font_size);
	guiFont->tex->tiled = 1;
	return guiFont;
}

void GUIFontDestroy(struct GUIFont* font) {
	sf2d_free_texture(font->tex);
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
		struct GUIFontGlyphMetric metric = defaultFontMetrics[c];
		sf2d_draw_texture_part_blend(font->tex, x, y - GLYPH_HEIGHT + metric.padding.top,
			                         (c & 15) * CELL_WIDTH + metric.padding.left,
			                         (c >> 4) * CELL_HEIGHT + metric.padding.top,
			                         CELL_WIDTH - (metric.padding.left + metric.padding.right),
			                         CELL_HEIGHT - (metric.padding.top + metric.padding.bottom),
			                         color);
		x += metric.width;
	}
}
