/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/gui/font.h"
#include "util/gui/font-metrics.h"
#include "util/png-io.h"
#include "util/vfs.h"

#include <sf2d.h>
#include "font_raw.h"

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
	memcpy(guiFont->tex->data, font_raw, font_raw_size);
	guiFont->tex->tiled = 1;
	return guiFont;
}

void GUIFontDestroy(struct GUIFont* font) {
	sf2d_free_texture(font->tex);
	free(font);
}

unsigned GUIFontHeight(const struct GUIFont* font) {
	UNUSED(font);
	return GLYPH_HEIGHT;
}

unsigned GUIFontGlyphWidth(const struct GUIFont* font, uint32_t glyph) {
	UNUSED(font);
	if (glyph > 0x7F) {
		glyph = 0;
	}
	return defaultFontMetrics[glyph].width;
}

void GUIFontDrawGlyph(const struct GUIFont* font, int x, int y, uint32_t color, uint32_t glyph) {
	if (glyph > 0x7F) {
		glyph = 0;
	}
	color = (color >> 24) | (color << 8);
	struct GUIFontGlyphMetric metric = defaultFontMetrics[glyph];
	sf2d_draw_texture_part_blend(font->tex,
	                             x - metric.padding.left,
	                             y - GLYPH_HEIGHT,
	                             (glyph & 15) * CELL_WIDTH,
	                             (glyph >> 4) * CELL_HEIGHT,
	                             CELL_WIDTH,
	                             CELL_HEIGHT,
	                             color);
}
