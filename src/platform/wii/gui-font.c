/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/gui/font.h"
#include "font.h"

#include <ogc/tpl.h>

#define GLYPH_HEIGHT 11
#define GLYPH_WIDTH 14
#define FONT_TRACKING 10
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
	GXTexObj tex;
	// Grumble grumble, libogc is bad about const-correctness
	struct GUIFont* ncfont = font;
	TPL_GetTexture(&ncfont->tdf, 0, &tex);
	GX_LoadTexObj(&tex, GX_TEXMAP0);

	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	for (i = 0; i < len; ++i) {
		char c = buffer[i];
		if (c > 0x7F) {
			c = 0;
		}
		s16 tx = (c & 15) * CELL_WIDTH + ((CELL_WIDTH - GLYPH_WIDTH) >> 1);
		s16 ty = (c >> 4) * CELL_HEIGHT + ((CELL_HEIGHT - GLYPH_HEIGHT) >> 1) - 1;
		GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position2s16(x, y - GLYPH_HEIGHT);
		GX_TexCoord2f32(tx / 256.f, ty / 128.f);

		GX_Position2s16(x + GLYPH_WIDTH, y - GLYPH_HEIGHT);
		GX_TexCoord2f32((tx + CELL_WIDTH) / 256.f, ty / 128.f);

		GX_Position2s16(x + GLYPH_WIDTH, y);
		GX_TexCoord2f32((tx + CELL_WIDTH) / 256.f, (ty + CELL_HEIGHT) / 128.f);

		GX_Position2s16(x, y);
		GX_TexCoord2f32(tx / 256.f, (ty + CELL_HEIGHT) / 128.f);
		GX_End();
		x += FONT_TRACKING;
	}
}
