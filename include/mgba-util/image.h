/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_IMAGE_H
#define M_IMAGE_H

#include <mgba-util/common.h>

CXX_GUARD_START

#ifdef COLOR_16_BIT
typedef uint16_t color_t;
#define BYTES_PER_PIXEL 2
#else
typedef uint32_t color_t;
#define BYTES_PER_PIXEL 4
#endif

#define M_R5(X) ((X) & 0x1F)
#define M_G5(X) (((X) >> 5) & 0x1F)
#define M_B5(X) (((X) >> 10) & 0x1F)

#define M_R8(X) ((M_R5(X) * 0x21) >> 2)
#define M_G8(X) ((M_G5(X) * 0x21) >> 2)
#define M_B8(X) ((M_B5(X) * 0x21) >> 2)

#define M_RGB5_TO_BGR8(X) ((M_R5(X) << 3) | (M_G5(X) << 11) | (M_B5(X) << 19))
#define M_RGB5_TO_RGB8(X) ((M_R5(X) << 19) | (M_G5(X) << 11) | (M_B5(X) << 3))
#define M_RGB8_TO_BGR5(X) ((((X) & 0xF8) >> 3) | (((X) & 0xF800) >> 6) | (((X) & 0xF80000) >> 9))
#define M_RGB8_TO_RGB5(X) ((((X) & 0xF8) << 7) | (((X) & 0xF800) >> 6) | (((X) & 0xF80000) >> 19))

#ifndef COLOR_16_BIT
#define M_COLOR_RED   0x000000FF
#define M_COLOR_GREEN 0x0000FF00
#define M_COLOR_BLUE  0x00FF0000
#define M_COLOR_ALPHA 0xFF000000
#define M_COLOR_WHITE 0x00FFFFFF

#define M_RGB8_TO_NATIVE(X) (((X) & 0x00FF00) | (((X) & 0x0000FF) << 16) | (((X) & 0xFF0000) >> 16))
#elif defined(COLOR_5_6_5)
#define M_COLOR_RED   0x001F
#define M_COLOR_GREEN 0x07E0
#define M_COLOR_BLUE  0xF800
#define M_COLOR_ALPHA 0x0000
#define M_COLOR_WHITE 0xFFDF

#define M_RGB8_TO_NATIVE(X) ((((X) & 0xF8) << 8) | (((X) & 0xFC00) >> 5) | (((X) & 0xF80000) >> 19))
#else
#define M_COLOR_RED   0x001F
#define M_COLOR_GREEN 0x03E0
#define M_COLOR_BLUE  0x7C00
#define M_COLOR_ALPHA 0x1000
#define M_COLOR_WHITE 0x7FFF

#define M_RGB8_TO_NATIVE(X) M_RGB8_TO_BGR5(X)
#endif

enum mColorFormat {
	mCOLOR_XBGR8  = 0x00001,
	mCOLOR_XRGB8  = 0x00002,
	mCOLOR_BGRX8  = 0x00004,
	mCOLOR_RGBX8  = 0x00008,
	mCOLOR_ABGR8  = 0x00010,
	mCOLOR_ARGB8  = 0x00020,
	mCOLOR_BGRA8  = 0x00040,
	mCOLOR_RGBA8  = 0x00080,
	mCOLOR_RGB5   = 0x00100,
	mCOLOR_BGR5   = 0x00200,
	mCOLOR_RGB565 = 0x00400,
	mCOLOR_BGR565 = 0x00800,
	mCOLOR_ARGB5  = 0x01000,
	mCOLOR_ABGR5  = 0x02000,
	mCOLOR_RGBA5  = 0x04000,
	mCOLOR_BGRA5  = 0x08000,
	mCOLOR_RGB8   = 0x10000,
	mCOLOR_BGR8   = 0x20000,
	mCOLOR_L8     = 0x40000,
	mCOLOR_PAL8   = 0x80000,

	mCOLOR_ANY    = -1
};

#ifndef COLOR_16_BIT
#define mCOLOR_NATIVE mCOLOR_XBGR8
#elif !defined(COLOR_5_6_5)
#define mCOLOR_NATIVE mCOLOR_BGR5
#else
#define mCOLOR_NATIVE mCOLOR_RGB565
#endif

struct mImage {
	void* data;
	uint32_t* palette;
	unsigned width;
	unsigned height;
	unsigned stride;
	unsigned depth;
	unsigned palSize;
	enum mColorFormat format;
};

struct VFile;
struct mImage* mImageCreate(unsigned width, unsigned height, enum mColorFormat format);
struct mImage* mImageCreateWithStride(unsigned width, unsigned height, unsigned stride, enum mColorFormat format);
struct mImage* mImageCreateFromConstBuffer(unsigned width, unsigned height, unsigned stride, enum mColorFormat format, const void* pixels);
struct mImage* mImageLoad(const char* path);
struct mImage* mImageLoadVF(struct VFile* vf);
struct mImage* mImageConvertToFormat(const struct mImage*, enum mColorFormat format);
void mImageDestroy(struct mImage*);

bool mImageSave(const struct mImage*, const char* path, const char* format);
bool mImageSaveVF(const struct mImage*, struct VFile* vf, const char* format);

uint32_t mImageGetPixel(const struct mImage* image, unsigned x, unsigned y);
uint32_t mImageGetPixelRaw(const struct mImage* image, unsigned x, unsigned y);
void mImageSetPixel(struct mImage* image, unsigned x, unsigned y, uint32_t color);
void mImageSetPixelRaw(struct mImage* image, unsigned x, unsigned y, uint32_t color);

void mImageSetPaletteSize(struct mImage* image, unsigned count);
void mImageSetPaletteEntry(struct mImage* image, unsigned index, uint32_t color);

void mImageBlit(struct mImage* image, const struct mImage* source, int x, int y);
void mImageComposite(struct mImage* image, const struct mImage* source, int x, int y);
void mImageCompositeWithAlpha(struct mImage* image, const struct mImage* source, int x, int y, float alpha);

uint32_t mColorConvert(uint32_t color, enum mColorFormat from, enum mColorFormat to);
uint32_t mImageColorConvert(uint32_t color, const struct mImage* from, enum mColorFormat to);

#ifndef PYCPARSE
static inline unsigned mColorFormatBytes(enum mColorFormat format) {
	switch (format) {
	case mCOLOR_XBGR8:
	case mCOLOR_XRGB8:
	case mCOLOR_BGRX8:
	case mCOLOR_RGBX8:
	case mCOLOR_ABGR8:
	case mCOLOR_ARGB8:
	case mCOLOR_BGRA8:
	case mCOLOR_RGBA8:
		return 4;
	case mCOLOR_RGB5:
	case mCOLOR_BGR5:
	case mCOLOR_RGB565:
	case mCOLOR_BGR565:
	case mCOLOR_ARGB5:
	case mCOLOR_ABGR5:
	case mCOLOR_RGBA5:
	case mCOLOR_BGRA5:
		return 2;
	case mCOLOR_RGB8:
	case mCOLOR_BGR8:
		return 3;
	case mCOLOR_L8:
	case mCOLOR_PAL8:
		return 1;
	case mCOLOR_ANY:
		break;
	}
	return 0;
}

static inline bool mColorFormatHasAlpha(enum mColorFormat format) {
	switch (format) {
	case mCOLOR_XBGR8:
	case mCOLOR_XRGB8:
	case mCOLOR_BGRX8:
	case mCOLOR_RGBX8:
	case mCOLOR_RGB5:
	case mCOLOR_BGR5:
	case mCOLOR_RGB565:
	case mCOLOR_BGR565:
	case mCOLOR_RGB8:
	case mCOLOR_BGR8:
	case mCOLOR_L8:
		return false;
	case mCOLOR_ABGR8:
	case mCOLOR_ARGB8:
	case mCOLOR_BGRA8:
	case mCOLOR_RGBA8:
	case mCOLOR_ARGB5:
	case mCOLOR_ABGR5:
	case mCOLOR_RGBA5:
	case mCOLOR_BGRA5:
	case mCOLOR_PAL8:
		return true;
	case mCOLOR_ANY:
		break;
	}
	return false;
}

static inline color_t mColorFrom555(uint16_t value) {
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	color_t color = 0;
	color |= (value & 0x001F) << 11;
	color |= (value & 0x03E0) << 1;
	color |= (value & 0x7C00) >> 10;
#else
	color_t color = value;
#endif
#else
	color_t color = M_RGB5_TO_BGR8(value);
	color |= (color >> 5) & 0x070707;
#endif
	return color;
}

ATTRIBUTE_UNUSED static unsigned mColorMix5Bit(int weightA, unsigned colorA, int weightB, unsigned colorB) {
	unsigned c = 0;
	unsigned a, b;
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	a = colorA & 0xF81F;
	b = colorB & 0xF81F;
	a |= (colorA & 0x7C0) << 16;
	b |= (colorB & 0x7C0) << 16;
	c = ((a * weightA + b * weightB) / 16);
	if (c & 0x08000000) {
		c = (c & ~0x0FC00000) | 0x07C00000;
	}
	if (c & 0x0020) {
		c = (c & ~0x003F) | 0x001F;
	}
	if (c & 0x10000) {
		c = (c & ~0x1F800) | 0xF800;
	}
	c = (c & 0xF81F) | ((c >> 16) & 0x07C0);
#else
	a = colorA & 0x7C1F;
	b = colorB & 0x7C1F;
	a |= (colorA & 0x3E0) << 16;
	b |= (colorB & 0x3E0) << 16;
	c = ((a * weightA + b * weightB) / 16);
	if (c & 0x04000000) {
		c = (c & ~0x07E00000) | 0x03E00000;
	}
	if (c & 0x0020) {
		c = (c & ~0x003F) | 0x001F;
	}
	if (c & 0x8000) {
		c = (c & ~0xF800) | 0x7C00;
	}
	c = (c & 0x7C1F) | ((c >> 16) & 0x03E0);
#endif
#else
	a = colorA & 0xFF;
	b = colorB & 0xFF;
	c |= ((a * weightA + b * weightB) / 16) & 0x1FF;
	if (c & 0x00000100) {
		c = 0x000000FF;
	}

	a = colorA & 0xFF00;
	b = colorB & 0xFF00;
	c |= ((a * weightA + b * weightB) / 16) & 0x1FF00;
	if (c & 0x00010000) {
		c = (c & 0x000000FF) | 0x0000FF00;
	}

	a = colorA & 0xFF0000;
	b = colorB & 0xFF0000;
	c |= ((a * weightA + b * weightB) / 16) & 0x1FF0000;
	if (c & 0x01000000) {
		c = (c & 0x0000FFFF) | 0x00FF0000;
	}
#endif
	return c;
}

ATTRIBUTE_UNUSED static uint32_t mColorMixARGB8(uint32_t colorA, uint32_t colorB) {
	uint32_t alpha = colorA >> 24;
	if (!alpha) {
		return colorB;
	}

	uint32_t color = 0;
	uint32_t a, b;
	a = colorA & 0xFF00FF;
	a *= alpha + 1;
	color += (a >> 8) & 0xFF00FF;

	a = colorB & 0xFF00FF;
	a *= 0x100 - alpha;
	color += (a >> 8) & 0xFF00FF;

	if (color & 0x100) {
		color &= ~0xFF;
		color |= 0xFF;
	}
	if (color & 0x1000000) {
		color &= ~0xFF0000;
		color |= 0xFF0000;
	}

	b = 0;
	a = colorA & 0xFF00;
	a *= alpha + 1;
	b += a & 0xFF0000;

	a = colorB & 0xFF00;
	a *= 0x100 - alpha;
	b += a & 0xFF0000;

	if (b & 0x1000000) {
		b &= ~0xFF0000;
		b |= 0xFF0000;
	}
	color |= b >> 8;

	alpha += colorB >> 24;
	if (alpha > 0xFF) {
		color |= 0xFF000000;
	} else {
		color |= alpha << 24;
	}

	return color;
}
#endif

CXX_GUARD_END

#endif
