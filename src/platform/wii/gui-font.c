/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/gui/font.h>
#include <mgba-util/gui/font-metrics.h>
#include "icons.h"
#include "font.h"

#include <malloc.h>
#include <ogc/tpl.h>

#define GLYPH_HEIGHT 24
#define CELL_HEIGHT 32
#define CELL_WIDTH 32

struct GUIFont {
	TPLFile tdf;
	TPLFile iconsTdf;
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

	void* iconsTpl = memalign(32, icons_size);
	if (!iconsTpl) {
		TPL_CloseTPLFile(&guiFont->tdf);
		free(guiFont);
		return 0;
	}
	memcpy(iconsTpl, icons, icons_size);
	TPL_OpenTPLFromMemory(&guiFont->iconsTdf, iconsTpl, icons_size);
	return guiFont;
}

void GUIFontDestroy(struct GUIFont* font) {
	TPL_CloseTPLFile(&font->tdf);
	TPL_CloseTPLFile(&font->iconsTdf);
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
	color = (color >> 24) | (color << 8);
	GXTexObj tex;
	TPL_GetTexture(&font->tdf, 0, &tex);
	GX_LoadTexObj(&tex, GX_TEXMAP0);

	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	if (glyph > 0x7F) {
		glyph = '?';
	}
	struct GUIFontGlyphMetric metric = defaultFontMetrics[glyph];
	s16 tx = (glyph & 15) * CELL_WIDTH + metric.padding.left * 2;
	s16 ty = (glyph >> 4) * CELL_HEIGHT + metric.padding.top * 2;
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	GX_Position2s16(x, y - GLYPH_HEIGHT + metric.padding.top * 2);
	GX_Color1u32(color);
	GX_TexCoord2f32(tx / 512.f, ty / 256.f);

	GX_Position2s16(x + CELL_WIDTH - (metric.padding.left + metric.padding.right) * 2, y - GLYPH_HEIGHT + metric.padding.top * 2);
	GX_Color1u32(color);
	GX_TexCoord2f32((tx + CELL_WIDTH - (metric.padding.left + metric.padding.right) * 2) / 512.f, ty / 256.f);

	GX_Position2s16(x + CELL_WIDTH - (metric.padding.left + metric.padding.right) * 2, y - GLYPH_HEIGHT + CELL_HEIGHT - metric.padding.bottom * 2);
	GX_Color1u32(color);
	GX_TexCoord2f32((tx + CELL_WIDTH - (metric.padding.left + metric.padding.right) * 2) / 512.f, (ty + CELL_HEIGHT - (metric.padding.top + metric.padding.bottom) * 2) / 256.f);

	GX_Position2s16(x, y - GLYPH_HEIGHT + CELL_HEIGHT - metric.padding.bottom * 2);
	GX_Color1u32(color);
	GX_TexCoord2f32(tx / 512.f, (ty + CELL_HEIGHT - (metric.padding.top + metric.padding.bottom) * 2) / 256.f);
	GX_End();
}

void GUIFontDrawIcon(struct GUIFont* font, int x, int y, enum GUIAlignment align, enum GUIOrientation orient, uint32_t color, enum GUIIcon icon) {
	if (icon >= GUI_ICON_MAX) {
		return;
	}

	color = (color >> 24) | (color << 8);
	GXTexObj tex;

	TPL_GetTexture(&font->iconsTdf, 0, &tex);
	GX_LoadTexObj(&tex, GX_TEXMAP0);

	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

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

	float u[4];
	float v[4];

	switch (orient) {
	case GUI_ORIENT_0:
	default:
		// TODO: Rotations
		u[0] = u[3] = metric.x / 256.f;
		u[1] = u[2] = (metric.x + metric.width) / 256.f;
		v[0] = v[1] = (metric.y + metric.height) / 64.f;
		v[2] = v[3] = metric.y / 64.f;
		break;
	case GUI_ORIENT_HMIRROR:
		u[0] = u[3] = (metric.x + metric.width) / 256.f;
		u[1] = u[2] = metric.x / 256.f;
		v[0] = v[1] = (metric.y + metric.height) / 64.f;
		v[2] = v[3] = metric.y / 64.f;
		break;
	case GUI_ORIENT_VMIRROR:
		u[0] = u[3] = metric.x / 256.f;
		u[1] = u[2] = (metric.x + metric.width) / 256.f;
		v[0] = v[1] = metric.y / 64.f;
		v[2] = v[3] = (metric.y + metric.height) / 64.f;
		break;
	}

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	GX_Position2s16(x, y + metric.height * 2);
	GX_Color1u32(color);
	GX_TexCoord2f32(u[0], v[0]);

	GX_Position2s16(x + metric.width * 2, y + metric.height * 2);
	GX_Color1u32(color);
	GX_TexCoord2f32(u[1], v[1]);

	GX_Position2s16(x + metric.width * 2, y);
	GX_Color1u32(color);
	GX_TexCoord2f32(u[2], v[2]);

	GX_Position2s16(x, y);
	GX_Color1u32(color);
	GX_TexCoord2f32(u[3], v[3]);
	GX_End();
}

void GUIFontDrawIconSize(struct GUIFont* font, int x, int y, int w, int h, uint32_t color, enum GUIIcon icon) {
	if (icon >= GUI_ICON_MAX) {
		return;
	}

	color = (color >> 24) | (color << 8);
	GXTexObj tex;

	TPL_GetTexture(&font->iconsTdf, 0, &tex);
	GX_LoadTexObj(&tex, GX_TEXMAP0);

	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	struct GUIIconMetric metric = defaultIconMetrics[icon];

	float u[4];
	float v[4];

	if (!h) {
		h = metric.height * 2;
	}
	if (!w) {
		w = metric.width * 2;
	}

	u[0] = u[3] = metric.x / 256.f;
	u[1] = u[2] = (metric.x + metric.width) / 256.f;
	v[0] = v[1] = (metric.y + metric.height) / 64.f;
	v[2] = v[3] = metric.y / 64.f;

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	GX_Position2s16(x, y + h);
	GX_Color1u32(color);
	GX_TexCoord2f32(u[0], v[0]);

	GX_Position2s16(x + w, y + h);
	GX_Color1u32(color);
	GX_TexCoord2f32(u[1], v[1]);

	GX_Position2s16(x + w, y);
	GX_Color1u32(color);
	GX_TexCoord2f32(u[2], v[2]);

	GX_Position2s16(x, y);
	GX_Color1u32(color);
	GX_TexCoord2f32(u[3], v[3]);
	GX_End();
}
