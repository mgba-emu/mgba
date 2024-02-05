/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/image/png-io.h>

#ifdef USE_PNG

#include <mgba-util/vfs.h>

static bool PNGWritePalette(png_structp png, png_infop info, const uint32_t* palette, unsigned entries);

static void _pngWrite(png_structp png, png_bytep buffer, png_size_t size) {
	struct VFile* vf = png_get_io_ptr(png);
	size_t written = vf->write(vf, buffer, size);
	if (written != size) {
		png_error(png, "Could not write PNG");
	}
}

static void _pngRead(png_structp png, png_bytep buffer, png_size_t size) {
	struct VFile* vf = png_get_io_ptr(png);
	size_t read = vf->read(vf, buffer, size);
	if (read != size) {
		png_error(png, "Could not read PNG");
	}
}

png_structp PNGWriteOpen(struct VFile* source) {
	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	if (!png) {
		return 0;
	}
	if (setjmp(png_jmpbuf(png))) {
		png_destroy_write_struct(&png, 0);
		return 0;
	}
	png_set_write_fn(png, source, _pngWrite, 0);
	return png;
}

static png_infop _pngWriteHeader(png_structp png, unsigned width, unsigned height, const uint32_t* palette, unsigned entries, int type) {
	png_infop info = png_create_info_struct(png);
	if (!info) {
		return NULL;
	}
	if (setjmp(png_jmpbuf(png))) {
		return NULL;
	}
	png_set_IHDR(png, info, width, height, 8, type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	if (type == PNG_COLOR_TYPE_PALETTE) {
		if (!palette) {
			return NULL;
		}
		if (!PNGWritePalette(png, info, palette, entries)) {
			return NULL;
		}
	}
	png_write_info(png, info);
	return info;
}

png_infop PNGWriteHeader(png_structp png, unsigned width, unsigned height, enum mColorFormat fmt) {
	int type;
	switch (fmt) {
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
		type = PNG_COLOR_TYPE_RGB;
		break;
	case mCOLOR_ABGR8:
	case mCOLOR_ARGB8:
	case mCOLOR_BGRA8:
	case mCOLOR_RGBA8:
	case mCOLOR_ARGB5:
	case mCOLOR_ABGR5:
	case mCOLOR_RGBA5:
	case mCOLOR_BGRA5:
	case mCOLOR_ANY:
		type = PNG_COLOR_TYPE_RGB_ALPHA;
		break;
	case mCOLOR_L8:
		type = PNG_COLOR_TYPE_GRAY;
		break;
	case mCOLOR_PAL8:
		type = PNG_COLOR_TYPE_PALETTE;
		break;
	}
	return _pngWriteHeader(png, width, height, NULL, 0, type);
}

png_infop PNGWriteHeaderPalette(png_structp png, unsigned width, unsigned height, const uint32_t* palette, unsigned entries) {
	return _pngWriteHeader(png, width, height, palette, entries, PNG_COLOR_TYPE_PALETTE);
}

static bool PNGWritePalette(png_structp png, png_infop info, const uint32_t* palette, unsigned entries) {
	if (!palette || !entries) {
		return false;
	}
	if (setjmp(png_jmpbuf(png))) {
		return false;
	}
	png_color colors[256];
	png_byte trans[256];
	unsigned i;
	for (i = 0; i < entries && i < 256; ++i) {
		colors[i].red = palette[i] >> 16;
		colors[i].green = palette[i] >> 8;
		colors[i].blue = palette[i];
		trans[i] = palette[i] >> 24;
	}
	png_set_PLTE(png, info, colors, entries);
	png_set_tRNS(png, info, trans, entries, NULL);
	return true;
}

static void _convertRowXBGR8(png_bytep row, const png_byte* pixelData, unsigned width) {
	unsigned x;
	for (x = 0; x < width; ++x) {
#ifdef __BIG_ENDIAN__
		row[x * 3] = pixelData[x * 4 + 3];
		row[x * 3 + 1] = pixelData[x * 4 + 2];
		row[x * 3 + 2] = pixelData[x * 4 + 1];
#else
		row[x * 3] = pixelData[x * 4];
		row[x * 3 + 1] = pixelData[x * 4 + 1];
		row[x * 3 + 2] = pixelData[x * 4 + 2];
#endif
	}
}

static void _convertRowXRGB8(png_bytep row, const png_byte* pixelData, unsigned width) {
	unsigned x;
	for (x = 0; x < width; ++x) {
#ifdef __BIG_ENDIAN__
		row[x * 3] = pixelData[x * 4 + 1];
		row[x * 3 + 1] = pixelData[x * 4 + 2];
		row[x * 3 + 2] = pixelData[x * 4 + 3];
#else
		row[x * 3] = pixelData[x * 4 + 2];
		row[x * 3 + 1] = pixelData[x * 4 + 1];
		row[x * 3 + 2] = pixelData[x * 4];
#endif
	}
}

static void _convertRowBGRX8(png_bytep row, const png_byte* pixelData, unsigned width) {
	unsigned x;
	for (x = 0; x < width; ++x) {
#ifdef __BIG_ENDIAN__
		row[x * 3] = pixelData[x * 4 + 2];
		row[x * 3 + 1] = pixelData[x * 4 + 1];
		row[x * 3 + 2] = pixelData[x * 4];
#else
		row[x * 3] = pixelData[x * 4 + 1];
		row[x * 3 + 1] = pixelData[x * 4 + 2];
		row[x * 3 + 2] = pixelData[x * 4 + 3];
#endif
	}
}

static void _convertRowRGBX8(png_bytep row, const png_byte* pixelData, unsigned width) {
	unsigned x;
	for (x = 0; x < width; ++x) {
#ifdef __BIG_ENDIAN__
		row[x * 3] = pixelData[x * 4];
		row[x * 3 + 1] = pixelData[x * 4 + 1];
		row[x * 3 + 2] = pixelData[x * 4 + 2];
#else
		row[x * 3] = pixelData[x * 4 + 3];
		row[x * 3 + 1] = pixelData[x * 4 + 2];
		row[x * 3 + 2] = pixelData[x * 4 + 1];
#endif
	}
}

static void _convertRowABGR8(png_bytep row, const png_byte* pixelData, unsigned width) {
	unsigned x;
	for (x = 0; x < width; ++x) {
#ifdef __BIG_ENDIAN__
		row[x * 4] = pixelData[x * 4 + 3];
		row[x * 4 + 1] = pixelData[x * 4 + 2];
		row[x * 4 + 2] = pixelData[x * 4 + 1];
		row[x * 4 + 3] = pixelData[x * 4];
#else
		row[x * 4] = pixelData[x * 4];
		row[x * 4 + 1] = pixelData[x * 4 + 1];
		row[x * 4 + 2] = pixelData[x * 4 + 2];
		row[x * 4 + 3] = pixelData[x * 4 + 3];
#endif
	}
}

static void _convertRowARGB8(png_bytep row, const png_byte* pixelData, unsigned width) {
	unsigned x;
	for (x = 0; x < width; ++x) {
#ifdef __BIG_ENDIAN__
		row[x * 4] = pixelData[x * 4 + 1];
		row[x * 4 + 1] = pixelData[x * 4 + 2];
		row[x * 4 + 2] = pixelData[x * 4 + 3];
		row[x * 4 + 3] = pixelData[x * 4];
#else
		row[x * 4] = pixelData[x * 4 + 2];
		row[x * 4 + 1] = pixelData[x * 4 + 1];
		row[x * 4 + 2] = pixelData[x * 4];
		row[x * 4 + 3] = pixelData[x * 4 + 3];
#endif
	}
}

static void _convertRowBGRA8(png_bytep row, const png_byte* pixelData, unsigned width) {
	unsigned x;
	for (x = 0; x < width; ++x) {
#ifdef __BIG_ENDIAN__
		row[x * 4] = pixelData[x * 4 + 2];
		row[x * 4 + 1] = pixelData[x * 4 + 1];
		row[x * 4 + 2] = pixelData[x * 4];
		row[x * 4 + 3] = pixelData[x * 4 + 3];
#else
		row[x * 4] = pixelData[x * 4 + 1];
		row[x * 4 + 1] = pixelData[x * 4 + 2];
		row[x * 4 + 2] = pixelData[x * 4 + 3];
		row[x * 4 + 3] = pixelData[x * 4];
#endif
	}
}

static void _convertRowRGBA8(png_bytep row, const png_byte* pixelData, unsigned width) {
	unsigned x;
	for (x = 0; x < width; ++x) {
#ifdef __BIG_ENDIAN__
		row[x * 4] = pixelData[x * 4];
		row[x * 4 + 1] = pixelData[x * 4 + 1];
		row[x * 4 + 2] = pixelData[x * 4 + 2];
		row[x * 4 + 3] = pixelData[x * 4 + 3];
#else
		row[x * 4] = pixelData[x * 4 + 3];
		row[x * 4 + 1] = pixelData[x * 4 + 2];
		row[x * 4 + 2] = pixelData[x * 4 + 1];
		row[x * 4 + 3] = pixelData[x * 4];
#endif
	}
}

static void _convertRowRGB5(png_bytep row, const png_byte* pixelData, unsigned width) {
	unsigned x;
	for (x = 0; x < width; ++x) {
		uint16_t c = ((uint16_t*) pixelData)[x];
		row[x * 3] = (c >> 7) & 0xF8;
		row[x * 3 + 1] = (c >> 2) & 0xF8;
		row[x * 3 + 2] = (c << 3) & 0xF8;
	}
}

static void _convertRowBGR5(png_bytep row, const png_byte* pixelData, unsigned width) {
	unsigned x;
	for (x = 0; x < width; ++x) {
		uint16_t c = ((uint16_t*) pixelData)[x];
		row[x * 3] = (c << 3) & 0xF8;
		row[x * 3 + 1] = (c >> 2) & 0xF8;
		row[x * 3 + 2] = (c >> 7) & 0xF8;
	}
}

static void _convertRowARGB5(png_bytep row, const png_byte* pixelData, unsigned width) {
	unsigned x;
	for (x = 0; x < width; ++x) {
		uint16_t c = ((uint16_t*) pixelData)[x];
		row[x * 4] = (c >> 7) & 0xF8;
		row[x * 4 + 1] = (c >> 2) & 0xF8;
		row[x * 4 + 2] = (c << 3) & 0xF8;
		row[x * 4 + 3] = (c >> 15) * 0xFF;
	}
}

static void _convertRowABGR5(png_bytep row, const png_byte* pixelData, unsigned width) {
	unsigned x;
	for (x = 0; x < width; ++x) {
		uint16_t c = ((uint16_t*) pixelData)[x];
		row[x * 4] = (c << 3) & 0xF8;
		row[x * 4 + 1] = (c >> 2) & 0xF8;
		row[x * 4 + 2] = (c >> 7) & 0xF8;
		row[x * 4 + 3] = (c >> 15) * 0xFF;
	}
}

static void _convertRowRGBA5(png_bytep row, const png_byte* pixelData, unsigned width) {
	unsigned x;
	for (x = 0; x < width; ++x) {
		uint16_t c = ((uint16_t*) pixelData)[x];
		row[x * 4] = (c >> 8) & 0xF8;
		row[x * 4 + 1] = (c >> 3) & 0xF8;
		row[x * 4 + 2] = (c << 2) & 0xF8;
		row[x * 4 + 3] = (c & 1) * 0xFF;
	}
}

static void _convertRowBGRA5(png_bytep row, const png_byte* pixelData, unsigned width) {
	unsigned x;
	for (x = 0; x < width; ++x) {
		uint16_t c = ((uint16_t*) pixelData)[x];
		row[x * 4] = (c << 2) & 0xF8;
		row[x * 4 + 1] = (c >> 3) & 0xF8;
		row[x * 4 + 2] = (c >> 8) & 0xF8;
		row[x * 4 + 3] = (c & 1) * 0xFF;
	}
}

static void _convertRowRGB565(png_bytep row, const png_byte* pixelData, unsigned width) {
	unsigned x;
	for (x = 0; x < width; ++x) {
		uint16_t c = ((uint16_t*) pixelData)[x];
		row[x * 3] = (c >> 8) & 0xF8;
		row[x * 3 + 1] = (c >> 3) & 0xFC;
		row[x * 3 + 2] = (c << 3) & 0xF8;
	}
}

static void _convertRowBGR565(png_bytep row, const png_byte* pixelData, unsigned width) {
	unsigned x;
	for (x = 0; x < width; ++x) {
		uint16_t c = ((uint16_t*) pixelData)[x];
		row[x * 3] = (c << 3) & 0xF8;
		row[x * 3 + 1] = (c >> 3) & 0xFC;
		row[x * 3 + 2] = (c >> 8) & 0xF8;
	}
}

static void _convertRowBGR8(png_bytep row, const png_byte* pixelData, unsigned width) {
	unsigned x;
	for (x = 0; x < width; ++x) {
#ifdef __BIG_ENDIAN__
		row[x * 3] = pixelData[x * 3 + 2];
		row[x * 3 + 1] = pixelData[x * 3 + 1];
		row[x * 3 + 2] = pixelData[x * 3];
#else
		row[x * 3] = pixelData[x * 3];
		row[x * 3 + 1] = pixelData[x * 3 + 1];
		row[x * 3 + 2] = pixelData[x * 3 + 2];
#endif
	}
}

static void _convertRowRGB8(png_bytep row, const png_byte* pixelData, unsigned width) {
	unsigned x;
	for (x = 0; x < width; ++x) {
#ifdef __BIG_ENDIAN__
		row[x * 3] = pixelData[x * 3];
		row[x * 3 + 1] = pixelData[x * 3 + 1];
		row[x * 3 + 2] = pixelData[x * 3 + 2];
#else
		row[x * 3] = pixelData[x * 3 + 2];
		row[x * 3 + 1] = pixelData[x * 3 + 1];
		row[x * 3 + 2] = pixelData[x * 3];
#endif
	}
}

bool PNGWritePixels(png_structp png, unsigned width, unsigned height, unsigned stride, const void* pixels, enum mColorFormat fmt) {
	int depth;
	if (fmt == mCOLOR_L8) {
		depth = 1;
	} else if (mColorFormatHasAlpha(fmt)) {
		depth = 4;
	} else {
		depth = 3;
	}
	png_bytep row = malloc(sizeof(png_byte) * width * depth);
	if (!row) {
		return false;
	}
	const png_byte* pixelData = pixels;
	if (setjmp(png_jmpbuf(png))) {
		free(row);
		return false;
	}
	const png_byte* pixelRow = pixelData;
	stride *= mColorFormatBytes(fmt);
	unsigned i;
	for (i = 0; i < height; ++i, pixelRow += stride) {
		switch (fmt) {
		case mCOLOR_XBGR8:
			_convertRowXBGR8(row, pixelRow, width);
			break;
		case mCOLOR_XRGB8:
			_convertRowXRGB8(row, pixelRow, width);
			break;
		case mCOLOR_BGRX8:
			_convertRowBGRX8(row, pixelRow, width);
			break;
		case mCOLOR_RGBX8:
			_convertRowRGBX8(row, pixelRow, width);
			break;
		case mCOLOR_ABGR8:
			_convertRowABGR8(row, pixelRow, width);
			break;
		case mCOLOR_ARGB8:
			_convertRowARGB8(row, pixelRow, width);
			break;
		case mCOLOR_BGRA8:
			_convertRowBGRA8(row, pixelRow, width);
			break;
		case mCOLOR_RGBA8:
			_convertRowRGBA8(row, pixelRow, width);
			break;
		case mCOLOR_RGB5:
			_convertRowRGB5(row, pixelRow, width);
			break;
		case mCOLOR_BGR5:
			_convertRowBGR5(row, pixelRow, width);
			break;
		case mCOLOR_ARGB5:
			_convertRowARGB5(row, pixelRow, width);
			break;
		case mCOLOR_ABGR5:
			_convertRowABGR5(row, pixelRow, width);
			break;
		case mCOLOR_RGBA5:
			_convertRowRGBA5(row, pixelRow, width);
			break;
		case mCOLOR_BGRA5:
			_convertRowBGRA5(row, pixelRow, width);
			break;
		case mCOLOR_RGB565:
			_convertRowRGB565(row, pixelRow, width);
			break;
		case mCOLOR_BGR565:
			_convertRowBGR565(row, pixelRow, width);
			break;
		case mCOLOR_BGR8:
			_convertRowBGR8(row, pixelRow, width);
			break;
		case mCOLOR_RGB8:
			_convertRowRGB8(row, pixelRow, width);
			break;
		case mCOLOR_L8:
		case mCOLOR_PAL8:
			memcpy(row, pixelRow, width);
			break;
		case mCOLOR_ANY:
			// Invalid value
			longjmp(png_jmpbuf(png), 1);
		}
		png_write_row(png, row);
	}
	free(row);
	return true;
}

bool PNGWritePixelsPalette(png_structp png, unsigned width, unsigned height, unsigned stride, const void* pixels) {
	UNUSED(width);
	const png_byte* pixelData = pixels;
	if (setjmp(png_jmpbuf(png))) {
		return false;
	}
	unsigned i;
	for (i = 0; i < height; ++i) {
		png_write_row(png, &pixelData[stride * i]);
	}
	return true;
}

bool PNGWriteCustomChunk(png_structp png, const char* name, size_t size, void* data) {
	char realName[5];
	strncpy(realName, name, 4);
	realName[0] = tolower((int) realName[0]);
	realName[1] = tolower((int) realName[1]);
	realName[4] = '\0';
	if (setjmp(png_jmpbuf(png))) {
		return false;
	}
	png_write_chunk(png, (png_bytep) realName, data, size);
	return true;
}

void PNGWriteClose(png_structp png, png_infop info) {
	if (!setjmp(png_jmpbuf(png))) {
		png_write_end(png, info);
	}
	png_destroy_write_struct(&png, &info);
}

bool isPNG(struct VFile* source) {
	png_byte header[PNG_HEADER_BYTES];
	source->seek(source, 0, SEEK_SET);
	if (source->read(source, header, PNG_HEADER_BYTES) < PNG_HEADER_BYTES) {
		return false;
	}
	return !png_sig_cmp(header, 0, PNG_HEADER_BYTES);
}

png_structp PNGReadOpen(struct VFile* source, unsigned offset) {
	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	if (!png) {
		return 0;
	}
	if (setjmp(png_jmpbuf(png))) {
		png_destroy_read_struct(&png, 0, 0);
		return 0;
	}
	png_set_read_fn(png, source, _pngRead);
	png_set_sig_bytes(png, offset);
	return png;
}

bool PNGInstallChunkHandler(png_structp png, void* context, ChunkHandler handler, const char* chunkName) {
	if (setjmp(png_jmpbuf(png))) {
		return false;
	}
	png_set_read_user_chunk_fn(png, context, handler);
	int len = strlen(chunkName);
	int chunks = 0;
	char* chunkList = strdup(chunkName);
	int i;
	for (i = 4; i <= len; i += 5) {
		chunkList[i] = '\0';
		++chunks;
	}
	png_set_keep_unknown_chunks(png, PNG_HANDLE_CHUNK_ALWAYS, (png_bytep) chunkList, chunks);
	free(chunkList);
	return true;
}

bool PNGReadHeader(png_structp png, png_infop info) {
	if (setjmp(png_jmpbuf(png))) {
		return false;
	}
	png_read_info(png, info);
	return true;
}

bool PNGIgnorePixels(png_structp png, png_infop info) {
	if (setjmp(png_jmpbuf(png))) {
		return false;
	}

	unsigned height = png_get_image_height(png, info);
	unsigned i;
	for (i = 0; i < height; ++i) {
		png_read_row(png, 0, 0);
	}
	return true;
}

bool PNGReadPixels(png_structp png, png_infop info, void* pixels, unsigned width, unsigned height, unsigned stride) {
	if (png_get_channels(png, info) != 3) {
		return false;
	}

	if (setjmp(png_jmpbuf(png))) {
		return false;
	}

	if (png_get_bit_depth(png, info) == 16) {
#ifdef PNG_READ_SCALE_16_TO_8_SUPPORTED
		png_set_scale_16(png);
#else
		png_set_strip_16(png);
#endif
	}

	uint8_t* pixelData = pixels;
	unsigned pngHeight = png_get_image_height(png, info);
	if (height < pngHeight) {
		pngHeight = height;
	}

	unsigned pngWidth = png_get_image_width(png, info);
	if (width < pngWidth) {
		pngWidth = width;
	}

	unsigned i;
	png_bytep row = malloc(png_get_rowbytes(png, info));
	for (i = 0; i < pngHeight; ++i) {
		png_read_row(png, row, 0);
		unsigned x;
		for (x = 0; x < pngWidth; ++x) {
#ifdef COLOR_16_BIT
			uint16_t c = row[x * 3 + 2] >> 3;
#ifdef COLOR_5_6_5
			c |= (row[x * 3 + 1] << 3) & 0x7E0;
			c |= (row[x * 3] << 8) & 0xF800;
#else
			c |= (row[x * 3 + 1] << 2) & 0x3E0;
			c |= (row[x * 3] << 7) & 0x7C00;
#endif
			((uint16_t*) pixelData)[stride * i + x] = c;
#else
#if __BIG_ENDIAN__
			pixelData[stride * i * 4 + x * 4 + 3] = row[x * 3];
			pixelData[stride * i * 4 + x * 4 + 2] = row[x * 3 + 1];
			pixelData[stride * i * 4 + x * 4 + 1] = row[x * 3 + 2];
			pixelData[stride * i * 4 + x * 4] = 0xFF;
#else
			pixelData[stride * i * 4 + x * 4] = row[x * 3];
			pixelData[stride * i * 4 + x * 4 + 1] = row[x * 3 + 1];
			pixelData[stride * i * 4 + x * 4 + 2] = row[x * 3 + 2];
			pixelData[stride * i * 4 + x * 4 + 3] = 0xFF;
#endif
#endif
		}
	}
	free(row);
	return true;
}

bool PNGReadPixelsA(png_structp png, png_infop info, void* pixels, unsigned width, unsigned height, unsigned stride) {
	if (png_get_channels(png, info) != 4) {
		return false;
	}

	if (setjmp(png_jmpbuf(png))) {
		return false;
	}

	if (png_get_bit_depth(png, info) == 16) {
#ifdef PNG_READ_SCALE_16_TO_8_SUPPORTED
		png_set_scale_16(png);
#else
		png_set_strip_16(png);
#endif
	}

	uint8_t* pixelData = pixels;
	unsigned pngHeight = png_get_image_height(png, info);
	if (height < pngHeight) {
		pngHeight = height;
	}

	unsigned pngWidth = png_get_image_width(png, info);
	if (width < pngWidth) {
		pngWidth = width;
	}

	unsigned i;
	png_bytep row = malloc(png_get_rowbytes(png, info));
	for (i = 0; i < pngHeight; ++i) {
		png_read_row(png, row, 0);
		unsigned x;
		for (x = 0; x < pngWidth; ++x) {
#ifdef COLOR_16_BIT
			uint16_t c = row[x * 4 + 2] >> 3;
#ifdef COLOR_5_6_5
			c |= (row[x * 4 + 1] << 3) & 0x7E0;
			c |= (row[x * 4] << 8) & 0xF800;
#else
			c |= (row[x * 4 + 1] << 2) & 0x3E0;
			c |= (row[x * 4] << 7) & 0x7C00;
#endif
			((uint16_t*) pixelData)[stride * i + x] = c;
#else
#if __BIG_ENDIAN__
			pixelData[stride * i * 4 + x * 4 + 3] = row[x * 4];
			pixelData[stride * i * 4 + x * 4 + 2] = row[x * 4 + 1];
			pixelData[stride * i * 4 + x * 4 + 1] = row[x * 4 + 2];
			pixelData[stride * i * 4 + x * 4] = row[x * 4 + 3];
#else
			pixelData[stride * i * 4 + x * 4] = row[x * 4];
			pixelData[stride * i * 4 + x * 4 + 1] = row[x * 4 + 1];
			pixelData[stride * i * 4 + x * 4 + 2] = row[x * 4 + 2];
			pixelData[stride * i * 4 + x * 4 + 3] = row[x * 4 + 3];
#endif
#endif
		}
	}
	free(row);
	return true;
}

bool PNGReadPixels8(png_structp png, png_infop info, void* pixels, unsigned width, unsigned height, unsigned stride) {
	if (png_get_channels(png, info) != 1) {
		return false;
	}

	if (setjmp(png_jmpbuf(png))) {
		return false;
	}

	if (png_get_bit_depth(png, info) == 16) {
#ifdef PNG_READ_SCALE_16_TO_8_SUPPORTED
		png_set_scale_16(png);
#else
		png_set_strip_16(png);
#endif
	}

	uint8_t* pixelData = pixels;
	unsigned pngHeight = png_get_image_height(png, info);
	if (height < pngHeight) {
		pngHeight = height;
	}

	unsigned pngWidth = png_get_image_width(png, info);
	if (width < pngWidth) {
		pngWidth = width;
	}

	unsigned i;
	for (i = 0; i < pngHeight; ++i) {
		png_read_row(png, &pixelData[stride * i], 0);
	}
	return true;
}


bool PNGReadFooter(png_structp png, png_infop end) {
	if (setjmp(png_jmpbuf(png))) {
		return false;
	}
	png_read_end(png, end);
	return true;
}

void PNGReadClose(png_structp png, png_infop info, png_infop end) {
	png_destroy_read_struct(&png, &info, &end);
}

#endif
