/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/gui/font.h"
#include "util/gui/font-metrics.h"
#include "font.h"

#include <malloc.h>
#include <ogc/tpl.h>

#define GLYPH_HEIGHT 12
#define CELL_HEIGHT 16
#define CELL_WIDTH 16

struct GUIFont {
	TPLFile tdf;
};

struct GUIFont* GUIFontCreate(void) {
	struct GUIFont* guiFont = malloc(sizeof(struct GUIFont));
	if (!guiFont) {
		return 0;
	}

	// libogc's TPL code modifies and frees this itself...
	void* fontTpl = memalign(32, font_size);
	if (!fontTpl) {
		free(guiFont);
		return 0;
	}
	memcpy(fontTpl, font, font_size);
	TPL_OpenTPLFromMemory(&guiFont->tdf, fontTpl, font_size);
	return guiFont;
}

void GUIFontDestroy(struct GUIFont* font) {
	TPL_CloseTPLFile(&font->tdf);
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
	return defaultFontMetrics[glyph].width;
}

void GUIFontDrawGlyph(const struct GUIFont* font, int x, int y, uint32_t color, uint32_t glyph) {
	color = (color >> 24) | (color << 8);
	GXTexObj tex;
	// Grumble grumble, libogc is bad about const-correctness
	struct GUIFont* ncfont = font;
	TPL_GetTexture(&ncfont->tdf, 0, &tex);
	GX_LoadTexObj(&tex, GX_TEXMAP0);

	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

	if (glyph > 0x7F) {
		glyph = '?';
	}
	struct GUIFontGlyphMetric metric = defaultFontMetrics[glyph];
	s16 tx = (glyph & 15) * CELL_WIDTH + metric.padding.left;
	s16 ty = (glyph >> 4) * CELL_HEIGHT + metric.padding.top;
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	GX_Position2s16(x, y - GLYPH_HEIGHT + metric.padding.top);
	GX_Color1u32(color);
	GX_TexCoord2f32(tx / 256.f, ty / 128.f);

	GX_Position2s16(x + CELL_WIDTH - (metric.padding.left + metric.padding.right), y - GLYPH_HEIGHT + metric.padding.top);
	GX_Color1u32(color);
	GX_TexCoord2f32((tx + CELL_WIDTH - (metric.padding.left + metric.padding.right)) / 256.f, ty / 128.f);

	GX_Position2s16(x + CELL_WIDTH - (metric.padding.left + metric.padding.right), y - GLYPH_HEIGHT + CELL_HEIGHT - metric.padding.bottom);
	GX_Color1u32(color);
	GX_TexCoord2f32((tx + CELL_WIDTH - (metric.padding.left + metric.padding.right)) / 256.f, (ty + CELL_HEIGHT - (metric.padding.top + metric.padding.bottom)) / 128.f);

	GX_Position2s16(x, y - GLYPH_HEIGHT + CELL_HEIGHT - metric.padding.bottom);
	GX_Color1u32(color);
	GX_TexCoord2f32(tx / 256.f, (ty + CELL_HEIGHT - (metric.padding.top + metric.padding.bottom)) / 128.f);
	GX_End();
}
