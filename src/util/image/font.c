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
#include FT_MODULE_H
#include FT_STROKER_H

#define DPI 100

static _Atomic size_t libraryOpen = 0;
static FT_Library library;

struct mFont {
	FT_Face face;
	FT_Stroker stroker;
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

	FT_Stroker stroker;
	if (FT_Stroker_New(library, &stroker)) {
		FT_Done_Face(face);
		return NULL;
	}

	struct mFont* font = calloc(1, sizeof(*font));
	font->face = face;
	font->stroker = stroker;
	mFontSetSize(font, 8 << mFONT_FRACT_BITS);
	return font;
}

void mFontDestroy(struct mFont* font) {
	FT_Done_Face(font->face);
	FT_Stroker_Done(font->stroker);
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
	struct mTextRunMetrics metrics;
	mFontRunMetrics(font, text, &metrics);
	return metrics.width;
}

const char* mFontRunMetrics(struct mFont* font, const char* text, struct mTextRunMetrics* out) {
	FT_Face face = font->face;
	out->height = face->size->metrics.ascender - face->size->metrics.descender;
	out->baseline = -face->size->metrics.descender;

	uint32_t lastGlyph = 0;
	int width = 0;
	while (*text) {
		uint32_t codepoint = utf8Char((const char**) &text, NULL);

		if (codepoint == '\n') {
			out->width = width;
			return text;
		}

		if (FT_Load_Char(face, codepoint, FT_LOAD_DEFAULT)) {
			lastGlyph = 0;
			continue;
		}

		FT_Vector kerning = {0};
		FT_Get_Kerning(face, lastGlyph, codepoint, FT_KERNING_DEFAULT, &kerning);
		width += kerning.x;
		width += face->glyph->advance.x;

		lastGlyph = codepoint;
	}

	out->width = width;
	return NULL;
}

void mFontTextBoxSize(struct mFont* font, const char* text, int lineSpacing, struct mSize* out) {
	int width = 0;
	int height = 0;

	do {
		struct mTextRunMetrics metrics;
		text = mFontRunMetrics(font, text, &metrics);
		if (metrics.width > width) {
			width = metrics.width;
		}
		height += metrics.height + lineSpacing;
	} while (text);

	out->width = width;
	out->height = height;
}

static const char* mPainterDrawTextRun(struct mPainter* painter, const char* text, int x, int y, enum mAlignment alignment, bool stroke) {
	FT_Face face = painter->font->face;
	struct mTextRunMetrics metrics;
	mFontRunMetrics(painter->font, text, &metrics);

	switch (alignment & mALIGN_HORIZONTAL) {
	case mALIGN_LEFT:
	default:
		break;
	case mALIGN_HCENTER:
		x -= metrics.width >> 1;
		break;
	case mALIGN_RIGHT:
		x -= metrics.width;
		break;
	}

	uint32_t lastCodepoint = 0;
	while (*text) {
		uint32_t codepoint = utf8Char((const char**) &text, NULL);

		if (codepoint == '\n') {
			return text;
		}

		if (FT_Load_Char(face, codepoint, FT_LOAD_DEFAULT)) {
			lastCodepoint = 0;
			continue;
		}

		FT_Bitmap* bitmap;
		FT_Glyph glyph = NULL;
		int top;
		int left;

		if (stroke) {
			FT_Stroker_Set(painter->font->stroker, painter->strokeWidth << mFONT_FRACT_BITS, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);

			FT_Get_Glyph(face->glyph, &glyph);
			if (painter->fill) {
				FT_Glyph_Stroke(&glyph, painter->font->stroker, 1);
			} else {
				FT_Glyph_StrokeBorder(&glyph, painter->font->stroker, 0, 1);
			}

			if (FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, NULL, 1)) {
				FT_Done_Glyph(glyph);
				lastCodepoint = 0;
				continue;
			}

			FT_BitmapGlyph bitmapGlyph = (FT_BitmapGlyph) glyph;
			bitmap = &bitmapGlyph->bitmap;
			top = bitmapGlyph->top;
			left = bitmapGlyph->left;
		} else {
			if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL)) {
				lastCodepoint = 0;
				continue;
			}
			bitmap = &face->glyph->bitmap;
			top = face->glyph->bitmap_top;
			left = face->glyph->bitmap_left;
		}

		struct mImage image;
		_makeTemporaryImage(&image, bitmap);

		FT_Vector kerning = {0};
		FT_Get_Kerning(face, lastCodepoint, codepoint, FT_KERNING_DEFAULT, &kerning);
		x += kerning.x;
		y += kerning.y;

		mPainterDrawMask(painter, &image, (x >> mFONT_FRACT_BITS) + left, (y >> mFONT_FRACT_BITS) - top);
		x += face->glyph->advance.x;
		y += face->glyph->advance.y;

		if (glyph) {
			FT_Done_Glyph(glyph);
		}

		lastCodepoint = codepoint;
	}

	return NULL;
}

void mPainterDrawText(struct mPainter* painter, const char* text, int x, int y, enum mAlignment alignment) {
	FT_Face face = painter->font->face;
	struct mSize size;
	mFontTextBoxSize(painter->font, text, 0, &size);

	x <<= 6;
	y <<= 6;

	switch (alignment & mALIGN_VERTICAL) {
	case mALIGN_TOP:
		y += face->size->metrics.ascender;
		break;
	case mALIGN_BASELINE:
		break;
	case mALIGN_VCENTER:
		y += face->size->metrics.ascender - size.height / 2;
		break;
	case mALIGN_BOTTOM:
	default:
		y += face->size->metrics.ascender - size.height;
		break;
	}

#if FREETYPE_MAJOR >= 2 && FREETYPE_MINOR >= 11
	if (painter->strokeWidth) {
		int yy = y;
		const char* ltext = text;
		uint32_t fillColor = painter->fillColor;
		painter->fillColor = painter->strokeColor;
		do {
			ltext = mPainterDrawTextRun(painter, ltext, x + painter->strokeWidth , yy, alignment, true);
			yy += face->size->metrics.height;
		} while (ltext);

		painter->fillColor = fillColor;
	}
#endif

	do {
		text = mPainterDrawTextRun(painter, text, x, y, alignment, false);
		y += face->size->metrics.height;
	} while (text);
}

#endif
