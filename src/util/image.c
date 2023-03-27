/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/image.h>

#include <mgba-util/image/png-io.h>
#include <mgba-util/vfs.h>

#define PIXEL(IM, X, Y) \
	(void*) (((IM)->stride * (Y) + (X)) * (IM)->depth + (uintptr_t) (IM)->data)

struct mImage* mImageCreate(unsigned width, unsigned height, enum mColorFormat format) {
	struct mImage* image = calloc(1, sizeof(struct mImage));
	if (!image) {
		return NULL;
	}
	image->width = width;
	image->height = height;
	image->stride = width;
	image->format = format;
	image->depth = mColorFormatBytes(format);
	image->data = calloc(width * height, image->depth);
	if (!image->data) {
		free(image);
		return NULL;
	}
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

	image->width = width;
	image->height = height;
	image->stride = width;

	switch (png_get_channels(png, info)) {
	case 3:
		image->format = mCOLOR_XBGR8;
		image->depth = 4;
		image->data = malloc(width * height * 4);
		if (!PNGReadPixels(png, info, image->data, width, height, width)) {
			free(image->data);
			free(image);
			PNGReadClose(png, info, end);
			return NULL;
		}
		break;
	case 4:
		image->format = mCOLOR_ABGR8;
		image->depth = 4;
		image->data = malloc(width * height * 4);
		if (!PNGReadPixelsA(png, info, image->data, width, height, width)) {
			free(image->data);
			free(image);
			PNGReadClose(png, info, end);
			return NULL;
		}
		break;
	default:
		// Not supported yet
		free(image);
		PNGReadClose(png, info, end);
		return NULL;
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

void mImageDestroy(struct mImage* image) {
	free(image->data);
	free(image);
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
	}
	return color;
}

uint32_t mImageGetPixel(const struct mImage* image, unsigned x, unsigned y) {
	return mColorConvert(mImageGetPixelRaw(image, x, y), image->format, mCOLOR_ARGB8);
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
		r = (((color >> 10) & 0x1F) * 0x21) >> 2;
		g = (((color >> 5) & 0x3F) * 0x41) >> 4;
		b = ((color & 0x1F) * 0x21) >> 2;
		break;
	case mCOLOR_BGR565:
		b = (((color >> 10) & 0x1F) * 0x21) >> 2;
		g = (((color >> 5) & 0x3F) * 0x41) >> 4;
		r = ((color & 0x1F) * 0x21) >> 2;
		break;

	case mCOLOR_L8:
		r = color;
		g = color;
		b = color;
		break;

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
	case mCOLOR_ANY:
		return 0;
	}

	return color;
}
