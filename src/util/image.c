/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/image.h>

#include <mgba-util/geometry.h>
#include <mgba-util/image/png-io.h>
#include <mgba-util/vfs.h>

#define PIXEL(IM, X, Y) \
	(void*) (((IM)->stride * (Y) + (X)) * (IM)->depth + (uintptr_t) (IM)->data)

#define ROW(IM, Y) PIXEL(IM, 0, Y)

#ifdef __BIG_ENDIAN__
#define SHIFT_IN(COLOR, DEPTH) \
	if ((DEPTH) < 4) { \
		(COLOR) >>= (32 - 8 * (DEPTH)); \
	}

#define SHIFT_OUT(COLOR, DEPTH) \
	if ((DEPTH) < 4) { \
		(COLOR) <<= (32 - 8 *(DEPTH)); \
	}
#else
#define SHIFT_IN(COLOR, DEPTH)
#define SHIFT_OUT(COLOR, DEPTH)
#endif

#define GET_PIXEL(DST, SRC, DEPTH) do { \
	uint32_t _color = 0; \
	memcpy(&_color, (void*) (SRC), (DEPTH)); \
	SHIFT_IN(_color, (DEPTH)); \
	(DST) = _color; \
} while (0)

#define PUT_PIXEL(SRC, DST, DEPTH) do { \
	uint32_t _color = (SRC); \
	SHIFT_OUT(_color, (DEPTH)); \
	memcpy((void*) (DST), &_color, (DEPTH)); \
} while (0);

struct mImage* mImageCreate(unsigned width, unsigned height, enum mColorFormat format) {
	return mImageCreateWithStride(width, height, width, format);
}

struct mImage* mImageCreateWithStride(unsigned width, unsigned height, unsigned stride, enum mColorFormat format) {
	if (width < 1 || height < 1) {
		return NULL;
	}
	struct mImage* image = calloc(1, sizeof(struct mImage));
	if (!image) {
		return NULL;
	}
	image->width = width;
	image->height = height;
	image->stride = stride;
	image->format = format;
	image->depth = mColorFormatBytes(format);
	image->data = calloc(width * height, image->depth);
	if (!image->data) {
		free(image);
		return NULL;
	}
	if (format == mCOLOR_PAL8) {
		image->palette = malloc(1024);
		if (!image->palette) {
			free(image->data);
			free(image);
			return NULL;
		}
		image->palSize = 1;
	}
	return image;
}

struct mImage* mImageCreateFromConstBuffer(unsigned width, unsigned height, unsigned stride, enum mColorFormat format, const void* pixels) {
	struct mImage* image = mImageCreateWithStride(width, height, stride, format);
	if (!image) {
		return NULL;
	}
	memcpy(image->data, pixels, height * stride * image->depth);
	return image;
}

struct mImage* mImageLoad(const char* path) {
	struct VFile* vf = VFileOpen(path, O_RDONLY);
	if (!vf) {
		return NULL;
	}
	struct mImage* image = mImageLoadVF(vf);
	vf->close(vf);
	return image;
}

#ifdef USE_PNG
static struct mImage* mImageLoadPNG(struct VFile* vf) {
	png_structp png = PNGReadOpen(vf, PNG_HEADER_BYTES);
	png_infop info = png_create_info_struct(png);
	png_infop end = png_create_info_struct(png);
	if (!png || !info || !end) {
		PNGReadClose(png, info, end);
		return NULL;
	}

	if (!PNGReadHeader(png, info)) {
		PNGReadClose(png, info, end);
		return NULL;
	}
	unsigned width = png_get_image_width(png, info);
	unsigned height = png_get_image_height(png, info);

	struct mImage* image = calloc(1, sizeof(*image));
	if (!image) {
		PNGReadClose(png, info, end);
		return NULL;
	}
	bool ok = true;

	image->width = width;
	image->height = height;
	image->stride = width;

	switch (png_get_channels(png, info)) {
	case 3:
		image->format = mCOLOR_XBGR8;
		image->depth = 4;
		image->data = malloc(width * height * 4);
		if (!PNGReadPixels(png, info, image->data, width, height, width)) {
			ok = false;
		}
		break;
	case 4:
		image->format = mCOLOR_ABGR8;
		image->depth = 4;
		image->data = malloc(width * height * 4);
		if (!PNGReadPixelsA(png, info, image->data, width, height, width)) {
			ok = false;
		}
		break;
	case 1:
		if (png_get_color_type(png, info) == PNG_COLOR_TYPE_GRAY) {
			image->format = mCOLOR_L8;
		} else {
			png_colorp palette;
			png_bytep trns;
			int count;
			int trnsCount = 0;
			image->format = mCOLOR_PAL8;
			if (png_get_PLTE(png, info, &palette, &count) == 0) {
				ok = false;
				break;
			}
			if (count > 256) {
				count = 256;
#ifndef NDEBUG
				abort();
#endif
			}
			image->palette = malloc(1024);
			image->palSize = count;
			png_get_tRNS(png, info, &trns, &trnsCount, NULL);

			int i;
			for (i = 0; i < count; ++i) {
				uint32_t color = palette[i].red << 16;
				color |= palette[i].green << 8;
				color |= palette[i].blue;

				if (i < trnsCount) {
					color |= trns[i] << 24;
				} else {
					color |= 0xFF000000;
				}
				image->palette[i] = color;
			}
		}
		image->depth = 1;
		image->data = malloc(width * height);
		if (!PNGReadPixels8(png, info, image->data, width, height, width)) {
			ok = false;
		}
		break;
	default:
		// Not supported yet
		ok = false;
		break;
	}

	PNGReadClose(png, info, end);
	if (!ok) {
		mImageDestroy(image);
		image = NULL;
	}
	return image;
}
#endif

struct mImage* mImageLoadVF(struct VFile* vf) {
	vf->seek(vf, 0, SEEK_SET);
#ifdef USE_PNG
	if (isPNG(vf)) {
		return mImageLoadPNG(vf);
	}
	vf->seek(vf, 0, SEEK_SET);
#endif
	return NULL;
}

struct mImage* mImageConvertToFormat(const struct mImage* image, enum mColorFormat format) {
	if (format == mCOLOR_PAL8) {
		// Quantization shouldn't be handled here
		return NULL;
	}
	struct mImage* newImage = calloc(1, sizeof(*newImage));
	newImage->width = image->width;
	newImage->height = image->height;
	newImage->format = format;
	if (format == image->format) {
		newImage->depth = image->depth;
		newImage->stride = image->stride;
		newImage->data = malloc(image->stride * image->height * image->depth);
		memcpy(newImage->data, image->data, image->stride * image->height * image->depth);
		return newImage;
	}
	newImage->depth = mColorFormatBytes(format);
	newImage->stride = image->width;
	newImage->data = malloc(image->width * image->height * newImage->depth);

	// TODO: Implement more specializations, e.g. alpha narrowing/widening, channel swapping
	size_t x, y;
	for (y = 0; y < newImage->height; ++y) {
		uintptr_t src = (uintptr_t) ROW(image, y);
		uintptr_t dst = (uintptr_t) ROW(newImage, y);
		for (x = 0; x < newImage->width; ++x, src += image->depth, dst += newImage->depth) {
			uint32_t color;
			GET_PIXEL(color, src, image->depth);
			color = mImageColorConvert(color, image, format);
			PUT_PIXEL(color, dst, newImage->depth);
		}
	}
	return newImage;
}

void mImageDestroy(struct mImage* image) {
	if (image->palette) {
		free(image->palette);
	}
	free(image->data);
	free(image);
}

bool mImageSave(const struct mImage* image, const char* path, const char* format) {
	struct VFile* vf = VFileOpen(path, O_WRONLY | O_CREAT | O_TRUNC);
	if (!vf) {
		return false;
	}

	char extension[PATH_MAX];
	if (!format) {
		separatePath(path, NULL, NULL, extension);
		format = extension;
	}
	bool success = mImageSaveVF(image, vf, format);
	vf->close(vf);
	return success;
}

#ifdef USE_PNG
bool mImageSavePNG(const struct mImage* image, struct VFile* vf) {
	png_structp png = PNGWriteOpen(vf);
	png_infop info = NULL;
	bool ok = false;
	if (png) {
		if (image->format == mCOLOR_PAL8) {
			info = PNGWriteHeaderPalette(png, image->width, image->height, image->palette, image->palSize);
			if (info) {
				ok = PNGWritePixelsPalette(png, image->width, image->height, image->stride, image->data);
			}
		} else {
			info = PNGWriteHeader(png, image->width, image->height, image->format);
			if (info) {
				ok = PNGWritePixels(png, image->width, image->height, image->stride, image->data, image->format);
			}
		}
		PNGWriteClose(png, info);
	}
	return ok;
}
#endif

bool mImageSaveVF(const struct mImage* image, struct VFile* vf, const char* format) {
#ifdef USE_PNG
	if (strcasecmp(format, "png") == 0) {
		return mImageSavePNG(image, vf);
	}
#endif
	return false;
}

uint32_t mImageGetPixelRaw(const struct mImage* image, unsigned x, unsigned y) {
	if (x >= image->width || y >= image->height) {
		return 0;
	}
	const void* pixel = PIXEL(image, x, y);
	uint32_t color;
	switch (image->depth) {
	case 1:
		color = *(const uint8_t*) pixel;
		break;
	case 2:
		color = *(const uint16_t*) pixel;
		break;
	case 4:
		color = *(const uint32_t*) pixel;
		break;
	case 3:
#ifdef __BIG_ENDIAN__
		color = ((const uint8_t*) pixel)[0] << 16;
		color |= ((const uint8_t*) pixel)[1] << 8;
		color |= ((const uint8_t*) pixel)[2];
#else
		color = ((const uint8_t*) pixel)[0];
		color |= ((const uint8_t*) pixel)[1] << 8;
		color |= ((const uint8_t*) pixel)[2] << 16;
#endif
		break;
	default:
		// This should never be reached
		abort();
	}
	return color;
}

uint32_t mImageGetPixel(const struct mImage* image, unsigned x, unsigned y) {
	return mImageColorConvert(mImageGetPixelRaw(image, x, y), image, mCOLOR_ARGB8);
}

void mImageSetPixelRaw(struct mImage* image, unsigned x, unsigned y, uint32_t color) {
	if (x >= image->width || y >= image->height) {
		return;
	}
	void* pixel = PIXEL(image, x, y);
	switch (image->depth) {
	case 1:
		*(uint8_t*) pixel = color;
		break;
	case 2:
		*(uint16_t*) pixel = color;
		break;
	case 4:
		*(uint32_t*) pixel = color;
		break;
	case 3:
#ifdef __BIG_ENDIAN__
		((uint8_t*) pixel)[0] = color >> 16;
		((uint8_t*) pixel)[1] = color >> 8;
		((uint8_t*) pixel)[2] = color;
#else
		((uint8_t*) pixel)[0] = color;
		((uint8_t*) pixel)[1] = color >> 8;
		((uint8_t*) pixel)[2] = color >> 16;
#endif
		break;
	}
}

void mImageSetPixel(struct mImage* image, unsigned x, unsigned y, uint32_t color) {
	mImageSetPixelRaw(image, x, y, mColorConvert(color, mCOLOR_ARGB8, image->format));
}

void mImageSetPaletteSize(struct mImage* image, unsigned count) {
	if (image->format != mCOLOR_PAL8) {
		return;
	}
	if (count > 256) {
		count = 256;
	}
	image->palSize = count;
}

void mImageSetPaletteEntry(struct mImage* image, unsigned index, uint32_t color) {
	if (image->format != mCOLOR_PAL8) {
		return;
	}
	if (index > 256) {
		return;
	}
	image->palette[index] = color;
}

#define COMPOSITE_BOUNDS_INIT \
	struct mRectangle dstRect = { \
		.x = 0, \
		.y = 0, \
		.width = image->width, \
		.height = image->height \
	}; \
	struct mRectangle srcRect = { \
		.x = x, \
		.y = y, \
		.width = source->width, \
		.height = source->height \
	}; \
	if (!mRectangleIntersection(&srcRect, &dstRect)) { \
		return; \
	} \
	int srcStartX; \
	int srcStartY; \
	int dstStartX; \
	int dstStartY; \
	if (x < 0) { \
		dstStartX = 0; \
		srcStartX = -x; \
	} else { \
		srcStartX = 0; \
		dstStartX = srcRect.x; \
	} \
	if (y < 0) { \
		dstStartY = 0; \
		srcStartY = -y; \
	} else { \
		srcStartY = 0; \
		dstStartY = srcRect.y; \
	}

void mImageBlit(struct mImage* image, const struct mImage* source, int x, int y) {
	if (image->format == mCOLOR_PAL8) {
		// Can't blit to paletted image
		return;
	}

	COMPOSITE_BOUNDS_INIT;

	for (y = 0; y < srcRect.height; ++y) {
		uintptr_t srcPixel = (uintptr_t) PIXEL(source, srcStartX, srcStartY + y);
		uintptr_t dstPixel = (uintptr_t) PIXEL(image, dstStartX, dstStartY + y);
		for (x = 0; x < srcRect.width; ++x, srcPixel += source->depth, dstPixel += image->depth) {
			uint32_t color;
			GET_PIXEL(color, srcPixel, source->depth);
			color = mImageColorConvert(color, source, image->format);
			PUT_PIXEL(color, dstPixel, image->depth);
		}
	}
}

void mImageComposite(struct mImage* image, const struct mImage* source, int x, int y) {
	if (!mColorFormatHasAlpha(source->format)) {
		mImageBlit(image, source, x, y);
		return;
	}

	if (image->format == mCOLOR_PAL8) {
		// Can't blit to paletted image
		return;
	}

	COMPOSITE_BOUNDS_INIT;

	for (y = 0; y < srcRect.height; ++y) {
		uintptr_t srcPixel = (uintptr_t) PIXEL(source, srcStartX, srcStartY + y);
		uintptr_t dstPixel = (uintptr_t) PIXEL(image, dstStartX, dstStartY + y);
		for (x = 0; x < srcRect.width; ++x, srcPixel += source->depth, dstPixel += image->depth) {
			uint32_t color, colorB;
			GET_PIXEL(color, srcPixel, source->depth);
			color = mImageColorConvert(color, source, mCOLOR_ARGB8);
			if (color < 0xFF000000) {
				GET_PIXEL(colorB, dstPixel, image->depth);
				colorB = mColorConvert(colorB, image->format, mCOLOR_ARGB8);
				color = mColorMixARGB8(color, colorB);
			}
			color = mColorConvert(color, mCOLOR_ARGB8, image->format);
			PUT_PIXEL(color, dstPixel, image->depth);
		}
	}
}

void mImageCompositeWithAlpha(struct mImage* image, const struct mImage* source, int x, int y, float alpha) {
	if (alpha >= 1 && alpha < 257.f / 256.f) {
		mImageComposite(image, source, x, y);
		return;
	}
	if (image->format == mCOLOR_PAL8) {
		// Can't blit to paletted image
		return;
	}
	if (alpha <= 0) {
		return;
	}
	if (alpha > 256) {
		// TODO: Add a slow path for alpha > 1, since we need to check saturation only on this path
		alpha = 256;
	}

	COMPOSITE_BOUNDS_INIT;

	int fixedAlpha = alpha * 0x200;

	for (y = 0; y < srcRect.height; ++y) {
		uintptr_t srcPixel = (uintptr_t) PIXEL(source, srcStartX, srcStartY + y);
		uintptr_t dstPixel = (uintptr_t) PIXEL(image, dstStartX, dstStartY + y);
		for (x = 0; x < srcRect.width; ++x, srcPixel += source->depth, dstPixel += image->depth) {
			uint32_t color, colorB;
			GET_PIXEL(color, srcPixel, source->depth);
			color = mImageColorConvert(color, source, mCOLOR_ARGB8);
			uint32_t alpha = (color >> 24) * fixedAlpha;
			alpha >>= 9;
			if (alpha > 0xFF) {
				alpha = 0xFF;
			}
			color &= 0x00FFFFFF;
			color |= alpha << 24;

			GET_PIXEL(colorB, dstPixel, image->depth);
			colorB = mColorConvert(colorB, image->format, mCOLOR_ARGB8);

			color = mColorMixARGB8(color, colorB);
			color = mColorConvert(color, mCOLOR_ARGB8, image->format);
			PUT_PIXEL(color, dstPixel, image->depth);
		}
	}
}

uint32_t mColorConvert(uint32_t color, enum mColorFormat from, enum mColorFormat to) {
	if (from == to) {
		return color;
	}

	int r;
	int g;
	int b;
	int a = 0xFF;

	switch (from) {
	case mCOLOR_ARGB8:
		a = color >> 24;
		// Fall through
	case mCOLOR_XRGB8:
	case mCOLOR_RGB8:
		r = (color >> 16) & 0xFF;
		g = (color >> 8) & 0xFF;
		b = color & 0xFF;
		break;

	case mCOLOR_ABGR8:
		a = color >> 24;
		// Fall through
	case mCOLOR_XBGR8:
	case mCOLOR_BGR8:
		b = (color >> 16) & 0xFF;
		g = (color >> 8) & 0xFF;
		r = color & 0xFF;
		break;

	case mCOLOR_RGBA8:
		a = color & 0xFF;
		// Fall through
	case mCOLOR_RGBX8:
		r = (color >> 24) & 0xFF;
		g = (color >> 16) & 0xFF;
		b = (color >> 8) & 0xFF;
		break;

	case mCOLOR_BGRA8:
		a = color & 0xFF;
		// Fall through
	case mCOLOR_BGRX8:
		b = (color >> 24) & 0xFF;
		g = (color >> 16) & 0xFF;
		r = (color >> 8) & 0xFF;
		break;

	case mCOLOR_ARGB5:
		a = (color >> 15) * 0xFF;
		// Fall through
	case mCOLOR_RGB5:
		r = (((color >> 10) & 0x1F) * 0x21) >> 2;
		g = (((color >> 5) & 0x1F) * 0x21) >> 2;
		b = ((color & 0x1F) * 0x21) >> 2;
		break;

	case mCOLOR_ABGR5:
		a = (color >> 15) * 0xFF;
		// Fall through
	case mCOLOR_BGR5:
		b = (((color >> 10) & 0x1F) * 0x21) >> 2;
		g = (((color >> 5) & 0x1F) * 0x21) >> 2;
		r = ((color & 0x1F) * 0x21) >> 2;
		break;

	case mCOLOR_RGBA5:
		a = (color & 1) * 0xFF;
		r = (((color >> 11) & 0x1F) * 0x21) >> 2;
		g = (((color >> 6) & 0x1F) * 0x21) >> 2;
		b = (((color >> 1) & 0x1F) * 0x21) >> 2;
		break;
	case mCOLOR_BGRA5:
		a = (color & 1) * 0xFF;
		b = (((color >> 11) & 0x1F) * 0x21) >> 2;
		g = (((color >> 6) & 0x1F) * 0x21) >> 2;
		r = (((color >> 1) & 0x1F) * 0x21) >> 2;
		break;

	case mCOLOR_RGB565:
		r = (((color >> 11) & 0x1F) * 0x21) >> 2;
		g = (((color >> 5) & 0x3F) * 0x41) >> 4;
		b = ((color & 0x1F) * 0x21) >> 2;
		break;
	case mCOLOR_BGR565:
		b = (((color >> 11) & 0x1F) * 0x21) >> 2;
		g = (((color >> 5) & 0x3F) * 0x41) >> 4;
		r = ((color & 0x1F) * 0x21) >> 2;
		break;

	case mCOLOR_L8:
		r = color;
		g = color;
		b = color;
		break;

	case mCOLOR_PAL8:
	case mCOLOR_ANY:
		return 0;
	}

	color = 0;
	switch (to) {
	case mCOLOR_XRGB8:
		a = 0xFF;
		// Fall through
	case mCOLOR_ARGB8:
		color |= a << 24;
		// Fall through
	case mCOLOR_RGB8:
		color |= r << 16;
		color |= g << 8;
		color |= b;
		break;
	case mCOLOR_XBGR8:
		a = 0xFF;
		// Fall through
	case mCOLOR_ABGR8:
		color |= a << 24;
		// Fall through
	case mCOLOR_BGR8:
		color |= b << 16;
		color |= g << 8;
		color |= r;
		break;
	case mCOLOR_RGBX8:
		a = 0xFF;
		// Fall through
	case mCOLOR_RGBA8:
		color |= a;
		color |= r << 24;
		color |= g << 16;
		color |= b << 8;
		break;
	case mCOLOR_BGRX8:
		a = 0xFF;
		// Fall through
	case mCOLOR_BGRA8:
		color |= a;
		color |= b << 24;
		color |= g << 16;
		color |= r << 8;
		break;
	case mCOLOR_ARGB5:
		color |= (!!a << 15);
		// Fall through
	case mCOLOR_RGB5:
		color |= (r >> 3) << 10;
		color |= (g >> 3) << 5;
		color |= b >> 3;
		break;
	case mCOLOR_ABGR5:
		color |= (!!a << 15);
		// Fall through
	case mCOLOR_BGR5:
		color |= (b >> 3) << 10;
		color |= (g >> 3) << 5;
		color |= r >> 3;
		break;
	case mCOLOR_RGBA5:
		color |= !!a;
		color |= (r >> 3) << 11;
		color |= (g >> 3) << 6;
		color |= (b >> 3) << 1;
		break;
	case mCOLOR_BGRA5:
		color |= !!a;
		color |= (b >> 3) << 11;
		color |= (g >> 3) << 6;
		color |= (r >> 3) << 1;
		break;
	case mCOLOR_RGB565:
		color |= (r >> 3) << 11;
		color |= (g >> 2) << 5;
		color |= b >> 3;
		break;
	case mCOLOR_BGR565:
		color |= (b >> 3) << 11;
		color |= (g >> 2) << 5;
		color |= r >> 3;
		break;
	case mCOLOR_L8:
		// sRGB primaries in fixed point, roughly fudged to saturate to 0xFFFF
		color = (55 * r + 184 * g + 18 * b) >> 8;
		break;
	case mCOLOR_PAL8:
	case mCOLOR_ANY:
		return 0;
	}

	return color;
}

uint32_t mImageColorConvert(uint32_t color, const struct mImage* from, enum mColorFormat to) {
	if (from->format != mCOLOR_PAL8) {
		return mColorConvert(color, from->format, to);
	}
	if (color < from->palSize) {
		color = from->palette[color];
	}
	return mColorConvert(color, mCOLOR_ARGB8, to);
}
