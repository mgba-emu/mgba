/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/gui/font.h"
#include "util/gui/font-metrics.h"

#include <vita2d.h>

#define CELL_HEIGHT 32
#define CELL_WIDTH 32
#define GLYPH_HEIGHT 24

extern const uint8_t _binary_font2x_png_start[];
extern const uint8_t _binary_icons2x_png_start[];

struct GUIFont {
	vita2d_texture* tex;
	vita2d_texture* icons;
};

struct GUIFont* GUIFontCreate(void) {
	struct GUIFont* font = malloc(sizeof(struct GUIFont));
	if (!font) {
		return 0;
	}
	font->tex = vita2d_load_PNG_buffer(_binary_font2x_png_start);
	font->icons = vita2d_load_PNG_buffer(_binary_icons2x_png_start);
	return font;
}

void GUIFontDestroy(struct GUIFont* font) {
	vita2d_free_texture(font->tex);
	vita2d_free_texture(font->icons);
	free(font);
}

unsigned GUIFontHeight(const struct GUIFont* font) {
	UNUSED(font);
	return GLYPH_HEIGHT;
}

unsigned GUIFontGlyphWidth(const struct GUIFont* font, uint32_t glyph) {
	UNUSED(font);
	if (glyph > 0x7F) {
		glyph = '?';
	}
	return defaultFontMetrics[glyph].width * 2;
}

void GUIFontDrawGlyph(const struct GUIFont* font, int x, int y, uint32_t color, uint32_t glyph) {
	if (glyph > 0x7F) {
		glyph = '?';
	}
	struct GUIFontGlyphMetric metric = defaultFontMetrics[glyph];
	vita2d_draw_texture_tint_part(font->tex, x, y - GLYPH_HEIGHT + metric.padding.top * 2,
	                                    (glyph & 15) * CELL_WIDTH + metric.padding.left * 2,
	                                    (glyph >> 4) * CELL_HEIGHT + metric.padding.top * 2,
	                                    CELL_WIDTH - (metric.padding.left + metric.padding.right) * 2,
	                                    CELL_HEIGHT - (metric.padding.top + metric.padding.bottom) * 2,
	                                    color);
}

void GUIFontDrawIcon(const struct GUIFont* font, int x, int y, enum GUIAlignment align, enum GUIOrientation orient, uint32_t color, enum GUIIcon icon) {
	if (icon >= GUI_ICON_MAX) {
		return;
	}
	struct GUIIconMetric metric = defaultIconMetrics[icon];
	switch (align & GUI_ALIGN_HCENTER) {
	case GUI_ALIGN_HCENTER:
		x -= metric.width;
		break;
	case GUI_ALIGN_RIGHT:
		x -= metric.width * 2;
		break;
	}
	switch (align & GUI_ALIGN_VCENTER) {
	case GUI_ALIGN_VCENTER:
		y -= metric.height;
		break;
	case GUI_ALIGN_BOTTOM:
		y -= metric.height * 2;
		break;
	}

	switch (orient) {
	case GUI_ORIENT_HMIRROR:
		vita2d_draw_texture_tint_part_scale(font->icons, x + metric.width * 2, y,
		                                    metric.x * 2, metric.y * 2,
		                                    metric.width * 2, metric.height * 2,
		                                    -1, 1, color);
		return;
	case GUI_ORIENT_VMIRROR:
		vita2d_draw_texture_tint_part_scale(font->icons, x, y + metric.height * 2,
		                                    metric.x * 2, metric.y * 2,
		                                    metric.width * 2, metric.height * 2,
		                                    1, -1, color);
		return;
	case GUI_ORIENT_0:
	default:
		// TOOD: Rotate
		vita2d_draw_texture_tint_part(font->icons, x, y,
		                                    metric.x * 2, metric.y * 2,
		                                    metric.width * 2, metric.height * 2,
		                                    color);
		break;
	}
}

void GUIFontDrawIconSize(const struct GUIFont* font, int x, int y, int w, int h, uint32_t color, enum GUIIcon icon) {
	if (icon >= GUI_ICON_MAX) {
		return;
	}
	struct GUIIconMetric metric = defaultIconMetrics[icon];
	vita2d_draw_texture_tint_part_scale(font->icons, x, y,
	                                    metric.x * 2, metric.y * 2,
	                                    metric.width * 2, metric.height * 2,
	                                    w / (float) metric.width, h / (float) metric.height, color);
}
