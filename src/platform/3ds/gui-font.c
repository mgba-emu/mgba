/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/gui/font.h>
#include <mgba-util/gui/font-metrics.h>
#include <mgba-util/image/png-io.h>
#include <mgba-util/vfs.h>
#include "icons.h"

#include <tex3ds.h>
#include "ctr-gpu.h"

#define FONT_SIZE 15.6f

struct GUIFont {
	CFNT_s* font;
	C3D_Tex* sheets;
	C3D_Tex icons;
	float size;
};

struct GUIFont* GUIFontCreate(void) {
	fontEnsureMapped();
	struct GUIFont* guiFont = malloc(sizeof(struct GUIFont));
	if (!guiFont) {
		return 0;
	}
	C3D_Tex* tex;

	guiFont->font = fontGetSystemFont();
	TGLP_s* glyphInfo = fontGetGlyphInfo(guiFont->font);
	guiFont->size = FONT_SIZE / glyphInfo->cellHeight;
	guiFont->sheets = calloc(glyphInfo->nSheets, sizeof(*guiFont->sheets));

	int i;
	for (i = 0; i < glyphInfo->nSheets; ++i) {
		tex = &guiFont->sheets[i];
		tex->data = fontGetGlyphSheetTex(guiFont->font, i);
		tex->fmt = glyphInfo->sheetFmt;
		tex->size = glyphInfo->sheetSize;
		tex->width = glyphInfo->sheetWidth;
		tex->height = glyphInfo->sheetHeight;
		tex->param = GPU_TEXTURE_MAG_FILTER(GPU_LINEAR) | GPU_TEXTURE_MIN_FILTER(GPU_LINEAR) | GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE) | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE);
	}

	Tex3DS_Texture t3x = Tex3DS_TextureImport(icons, icons_size, &guiFont->icons, NULL, true);
	Tex3DS_TextureFree(t3x);

	return guiFont;
}

void GUIFontDestroy(struct GUIFont* font) {
	free(font->sheets);
	C3D_TexDelete(&font->icons);
	free(font);
}

unsigned GUIFontHeight(const struct GUIFont* font) {
	return fontGetInfo(font->font)->lineFeed * font->size;
}

unsigned GUIFontGlyphWidth(const struct GUIFont* font, uint32_t glyph) {
	int index = fontGlyphIndexFromCodePoint(font->font, glyph);
	charWidthInfo_s* info = fontGetCharWidthInfo(font->font, index);
	if (info) {
		return info->charWidth * font->size;
	}
	return 0;
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

void GUIFontDrawGlyph(struct GUIFont* font, int glyph_x, int glyph_y, uint32_t color, uint32_t glyph) {
	int index = fontGlyphIndexFromCodePoint(font->font, glyph);
	fontGlyphPos_s data;
	fontCalcGlyphPos(&data, font->font, index, 0, 1.0, 1.0);

	C3D_Tex* tex = &font->sheets[data.sheetIndex];
	ctrActivateTexture(tex);

	float width = data.texcoord.right - data.texcoord.left;
	float height = data.texcoord.top - data.texcoord.bottom;
	u16 x = glyph_x;
	u16 y = glyph_y + tex->height * height / 8;
	u16 u = tex->width * data.texcoord.left;
	u16 v = tex->height * data.texcoord.bottom;

	ctrAddRectEx(color, x, y,
	             tex->width * width * font->size, tex->height * height * -font->size,
	             u, v, tex->width * width, tex->height * height, 0);
}

void GUIFontDrawIcon(struct GUIFont* font, int x, int y, enum GUIAlignment align, enum GUIOrientation orient, uint32_t color, enum GUIIcon icon) {
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
	s16 origin = font->icons.height - metric.y - metric.height;

	switch (orient) {
	case GUI_ORIENT_HMIRROR:
		ctrAddRectEx(color, x + metric.width, y + metric.height,
		             -metric.width, -metric.height,
		             metric.x, origin,
		             metric.width, metric.height, 0);
		break;
	case GUI_ORIENT_VMIRROR:
		ctrAddRectEx(color, x, y,
		             metric.width, metric.height,
		             metric.x, origin,
		             metric.width, metric.height, 0);
		break;
	case GUI_ORIENT_0:
	default:
		// TODO: Rotation
		ctrAddRectEx(color, x, y + metric.height,
		             metric.width, -metric.height,
		             metric.x, origin,
		             metric.width, metric.height, 0);
		break;
	}
}

void GUIFontDrawIconSize(struct GUIFont* font, int x, int y, int w, int h, uint32_t color, enum GUIIcon icon) {
	ctrActivateTexture(&font->icons);

	if (icon >= GUI_ICON_MAX) {
		return;
	}
	struct GUIIconMetric metric = defaultIconMetrics[icon];
	ctrAddRectEx(color, x, y + (h ? h : metric.height),
	             w ? w : metric.width,
	             h ? -h : -metric.height,
	             metric.x, font->icons.height - metric.y - metric.height,
	             metric.width, metric.height, 0);
}
