/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/gui/font.h>
#include <mgba-util/gui/font-metrics.h>
#include <mgba-util/string.h>

#include <vita2d.h>

#define CELL_HEIGHT 32
#define CELL_WIDTH 32
#define FONT_SIZE 1.25f

extern const uint8_t _binary_icons2x_png_start[];

struct GUIFont {
	vita2d_pgf* pgf;
	vita2d_texture* icons;
};

struct GUIFont* GUIFontCreate(void) {
	struct GUIFont* font = malloc(sizeof(struct GUIFont));
	if (!font) {
		return 0;
	}
	font->pgf = vita2d_load_default_pgf();
	font->icons = vita2d_load_PNG_buffer(_binary_icons2x_png_start);
	return font;
}

void GUIFontDestroy(struct GUIFont* font) {
	vita2d_free_pgf(font->pgf);
	vita2d_free_texture(font->icons);
	free(font);
}

unsigned GUIFontHeight(const struct GUIFont* font) {
	return vita2d_pgf_text_height(font->pgf, FONT_SIZE, "M") + 9;
}

unsigned GUIFontGlyphWidth(const struct GUIFont* font, uint32_t glyph) {
	char base[5] = { 0 };
	toUtf8(glyph, base);
	return vita2d_pgf_text_width(font->pgf, FONT_SIZE, base);
}

void GUIFontIconMetrics(const struct GUIFont* font, enum GUIIcon icon, unsigned* w, unsigned* h) {
	UNUSED(font);
	if (icon >= GUI_ICON_MAX) {
		if (w) {
			*w = 0;
		}
		if (h) {
			*h = 0;
		}
	} else {
		if (w) {
			*w = defaultIconMetrics[icon].width * 2;
		}
		if (h) {
			*h = defaultIconMetrics[icon].height * 2;
		}
	}
}

void GUIFontDrawGlyph(struct GUIFont* font, int x, int y, uint32_t color, uint32_t glyph) {
	char base[5] = { 0 };
	toUtf8(glyph, base);
	vita2d_pgf_draw_text(font->pgf, x, y, color, FONT_SIZE, base);
}

void GUIFontDrawIcon(struct GUIFont* font, int x, int y, enum GUIAlignment align, enum GUIOrientation orient, uint32_t color, enum GUIIcon icon) {
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

void GUIFontDrawIconSize(struct GUIFont* font, int x, int y, int w, int h, uint32_t color, enum GUIIcon icon) {
	if (icon >= GUI_ICON_MAX) {
		return;
	}
	struct GUIIconMetric metric = defaultIconMetrics[icon];
	vita2d_draw_texture_tint_part_scale(font->icons, x, y,
	                                    metric.x * 2, metric.y * 2,
	                                    metric.width * 2, metric.height * 2,
	                                    w ? (w / (float) metric.width) : 1, h ? (h / (float) metric.height) : 1, color);
}
