/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/image.h>

#ifdef USE_FREETYPE

#include <mgba-util/string.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#define DPI 100

static _Atomic size_t libraryOpen = 0;
static FT_Library library;

struct mFont {
	FT_Face face;
	unsigned emHeight;
};

static void _makeTemporaryImage(struct mImage* out, FT_Bitmap* in) {
	out->data = in->buffer;
	out->width = in->width;
	out->height = in->rows;
	out->palette = NULL;
	if (in->pixel_mode == FT_PIXEL_MODE_GRAY) {
		out->stride = in->pitch;
		out->format = mCOLOR_L8;
		out->depth = 1;
	} else {
		abort();
	}
}

struct mFont* mFontOpen(const char* path) {
	size_t opened = ++libraryOpen;
	if (opened == 1) {
		if (FT_Init_FreeType(&library)) {
			return NULL;
		}
	}

	FT_Face face;
	if (FT_New_Face(library, path, 0, &face)) {
		return NULL;
	}

	struct mFont* font = calloc(1, sizeof(*font));
	font->face = face;
	mFontSetSize(font, 8 * 64);
	return font;
}

void mFontDestroy(struct mFont* font) {
	FT_Done_Face(font->face);
	free(font);

	size_t opened = --libraryOpen;
	if (opened == 0) {
		FT_Done_FreeType(library);
	}
}

unsigned mFontSize(const struct mFont* font) {
	return font->emHeight;
}

void mFontSetSize(struct mFont* font, unsigned pt) {
	font->emHeight = pt;
	FT_Set_Char_Size(font->face, 0, pt, DPI, DPI);
}

int mFontSpanWidth(struct mFont* font, const char* text) {
	FT_Face face = font->face;
	uint32_t lastGlyph = 0;
	int width = 0;

	while (*text) {
		uint32_t glyph = utf8Char((const char**) &text, NULL);

		if (FT_Load_Char(face, glyph, FT_LOAD_DEFAULT)) {
			continue;
		}

		if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL)) {
			continue;
		}

		FT_Vector kerning = {0};
		FT_Get_Kerning(face, lastGlyph, glyph, FT_KERNING_DEFAULT, &kerning);
		width += kerning.x;
		width += face->glyph->advance.x;

		lastGlyph = glyph;
	}

	return width;
}

void mPainterDrawText(struct mPainter* painter, const char* text, int x, int y, enum mAlignment alignment) {
	FT_Face face = painter->font->face;
	uint32_t lastGlyph = 0;

	x <<= 6;
	y <<= 6;

	switch (alignment & mALIGN_VERTICAL) {
	case mALIGN_TOP:
		y += face->size->metrics.ascender;
		break;
	case mALIGN_BASELINE:
		break;
	case mALIGN_VCENTER:
		y += face->size->metrics.ascender - face->size->metrics.height / 2;
		break;
	case mALIGN_BOTTOM:
	default:
		y += face->size->metrics.descender;
		break;
	}

	switch (alignment & mALIGN_HORIZONTAL) {
	case mALIGN_LEFT:
	default:
		break;
	case mALIGN_HCENTER:
		x -= mFontSpanWidth(painter->font, text) >> 1;
		break;
	case mALIGN_RIGHT:
		x -= mFontSpanWidth(painter->font, text);
		break;
	}

	while (*text) {
		uint32_t glyph = utf8Char((const char**) &text, NULL);

		if (FT_Load_Char(face, glyph, FT_LOAD_DEFAULT)) {
			continue;
		}

		if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL)) {
			continue;
		}

		struct mImage image;
		_makeTemporaryImage(&image, &face->glyph->bitmap);

		FT_Vector kerning = {0};
		FT_Get_Kerning(face, lastGlyph, glyph, FT_KERNING_DEFAULT, &kerning);
		x += kerning.x;
		y += kerning.y;

		mPainterDrawMask(painter, &image, (x >> 6) + face->glyph->bitmap_left, (y >> 6) - face->glyph->bitmap_top);
		x += face->glyph->advance.x;
		y += face->glyph->advance.y;

		lastGlyph = glyph;
	}
}

#endif
