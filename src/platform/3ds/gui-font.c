/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/gui/font.h"
#include "util/gui/font-metrics.h"
#include "util/png-io.h"
#include "util/vfs.h"
#include "platform/3ds/ctr-gpu.h"
#include "icons.h"
#include "font.h"

#define CELL_HEIGHT 16
#define CELL_WIDTH 16
#define GLYPH_HEIGHT 12

struct GUIFont {
	struct ctrTexture texture;
	struct ctrTexture icons;
};

struct GUIFont* GUIFontCreate(void) {
	struct GUIFont* guiFont = malloc(sizeof(struct GUIFont));
	if (!guiFont) {
		return 0;
	}

	struct ctrTexture* tex = &guiFont->texture;
	ctrTexture_Init(tex);
	tex->data = vramAlloc(256 * 128 * 2);
	tex->format = GPU_RGBA5551;
	tex->width = 256;
	tex->height = 128;

	GSPGPU_FlushDataCache(font, font_size);
	GX_RequestDma((u32*) font, tex->data, font_size);
	gspWaitForDMA();

	tex = &guiFont->icons;
	ctrTexture_Init(tex);
	tex->data = vramAlloc(256 * 64 * 2);
	tex->format = GPU_RGBA5551;
	tex->width = 256;
	tex->height = 64;

	GSPGPU_FlushDataCache(icons, icons_size);
	GX_RequestDma((u32*) icons, tex->data, icons_size);
	gspWaitForDMA();

	return guiFont;
}

void GUIFontDestroy(struct GUIFont* font) {
	vramFree(font->texture.data);
	vramFree(font->icons.data);
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
			*w = defaultIconMetrics[icon].width;
		}
		if (h) {
			*h = defaultIconMetrics[icon].height;
		}
	}
}

void GUIFontDrawGlyph(const struct GUIFont* font, int glyph_x, int glyph_y, uint32_t color, uint32_t glyph) {
	ctrActivateTexture(&font->texture);

	if (glyph > 0x7F) {
		glyph = '?';
	}

	struct GUIFontGlyphMetric metric = defaultFontMetrics[glyph];
	u16 x = glyph_x - metric.padding.left;
	u16 y = glyph_y - GLYPH_HEIGHT;
	u16 u = (glyph % 16u) * CELL_WIDTH;
	u16 v = (glyph / 16u) * CELL_HEIGHT;

	ctrAddRect(color, x, y, u, v, CELL_WIDTH, CELL_HEIGHT);
}

void GUIFontDrawIcon(const struct GUIFont* font, int x, int y, enum GUIAlignment align, enum GUIOrientation orient, uint32_t color, enum GUIIcon icon) {
	ctrActivateTexture(&font->icons);

	if (icon >= GUI_ICON_MAX) {
		return;
	}

	struct GUIIconMetric metric = defaultIconMetrics[icon];
	switch (align & GUI_ALIGN_HCENTER) {
	case GUI_ALIGN_HCENTER:
		x -= metric.width / 2;
		break;
	case GUI_ALIGN_RIGHT:
		x -= metric.width;
		break;
	}
	switch (align & GUI_ALIGN_VCENTER) {
	case GUI_ALIGN_VCENTER:
		y -= metric.height / 2;
		break;
	case GUI_ALIGN_BOTTOM:
		y -= metric.height;
		break;
	}
	switch (orient) {
	case GUI_ORIENT_HMIRROR:
		ctrAddRectScaled(color, x + metric.width, y, -metric.width, metric.height, metric.x, metric.y, metric.width, metric.height);
		break;
	case GUI_ORIENT_VMIRROR:
		ctrAddRectScaled(color, x, y + metric.height, metric.width, -metric.height, metric.x, metric.y, metric.width, metric.height);
		break;
	case GUI_ORIENT_0:
	default:
		// TODO: Rotation
		ctrAddRect(color, x, y, metric.x, metric.y, metric.width, metric.height);
		break;
	}
}

void GUIFontDrawIconSize(const struct GUIFont* font, int x, int y, int w, int h, uint32_t color, enum GUIIcon icon) {
	ctrActivateTexture(&font->icons);

	if (icon >= GUI_ICON_MAX) {
		return;
	}

	struct GUIIconMetric metric = defaultIconMetrics[icon];
	ctrAddRectScaled(color, x, y, w ? w : metric.width, h ? h : metric.height, metric.x, metric.y, metric.width, metric.height);
}
