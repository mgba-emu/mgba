/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/gui/font.h>
#include <mgba-util/gui/font-metrics.h>
#include <mgba-util/png-io.h>
#include <mgba-util/vfs.h>
#include "icons.h"

#include "ctr-gpu.h"

#define FONT_SIZE 15.6f

struct GUIFont {
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

	TGLP_s* glyphInfo = fontGetGlyphInfo();
	guiFont->size = FONT_SIZE / glyphInfo->cellHeight;
	guiFont->sheets = malloc(sizeof(*guiFont->sheets) * glyphInfo->nSheets);

	int i;
	for (i = 0; i < glyphInfo->nSheets; ++i) {
		tex = &guiFont->sheets[i];
		tex->data = fontGetGlyphSheetTex(i);
		tex->fmt = glyphInfo->sheetFmt;
		tex->size = glyphInfo->sheetSize;
		tex->width = glyphInfo->sheetWidth;
		tex->height = glyphInfo->sheetHeight;
		tex->param = GPU_TEXTURE_MAG_FILTER(GPU_LINEAR) | GPU_TEXTURE_MIN_FILTER(GPU_LINEAR) | GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE) | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE);
	}

	tex = &guiFont->icons;
	C3D_TexInitVRAM(tex, 256, 64, GPU_RGBA5551);

	GSPGPU_FlushDataCache(icons, icons_size);
	GX_RequestDma((u32*) icons, tex->data, icons_size);
	gspWaitForDMA();

	return guiFont;
}

void GUIFontDestroy(struct GUIFont* font) {
	free(font->sheets);
	C3D_TexDelete(&font->icons);
	free(font);
}

unsigned GUIFontHeight(const struct GUIFont* font) {
	return fontGetInfo()->lineFeed * font->size;
}

unsigned GUIFontGlyphWidth(const struct GUIFont* font, uint32_t glyph) {
	int index = fontGlyphIndexFromCodePoint(glyph);
	charWidthInfo_s* info = fontGetCharWidthInfo(index);
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

void GUIFontDrawGlyph(const struct GUIFont* font, int glyph_x, int glyph_y, uint32_t color, uint32_t glyph) {
	int index = fontGlyphIndexFromCodePoint(glyph);
	fontGlyphPos_s data;
	fontCalcGlyphPos(&data, index, 0, 1.0, 1.0);

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
		ctrAddRectEx(color, x + metric.width, y,
		             -metric.width, metric.height,
		             metric.x, metric.y,
		             metric.width, metric.height, 0);
		break;
	case GUI_ORIENT_VMIRROR:
		ctrAddRectEx(color, x, y + metric.height,
		             metric.width, -metric.height,
		             metric.x, metric.y,
		             metric.width, metric.height, 0);
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
	ctrAddRectEx(color, x, y,
	             w ? w : metric.width,
	             h ? h : metric.height,
	             metric.x, metric.y,
	             metric.width, metric.height, 0);
}
