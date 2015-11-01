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
#include "ctr-gpu.h"

#define CELL_HEIGHT 16
#define CELL_WIDTH 16
#define GLYPH_HEIGHT 12

struct GUIFont {
	struct ctrTexture texture;
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

	GSPGPU_FlushDataCache(NULL, (u8*)font, font_size);
	GX_RequestDma(NULL, (u32*)font, tex->data, font_size);
	gspWaitForDMA();

	return guiFont;
}

void GUIFontDestroy(struct GUIFont* font) {
	vramFree(font->texture.data);
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
