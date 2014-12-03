/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/png-io.h"

#ifdef USE_PNG

#include "vfs.h"

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

png_infop PNGWriteHeader(png_structp png, unsigned width, unsigned height) {
	png_infop info = png_create_info_struct(png);
	if (!info) {
		return 0;
	}
	png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	png_write_info(png, info);
	return info;
}

bool PNGWritePixels(png_structp png, unsigned width, unsigned height, unsigned stride, void* pixels) {
	png_bytep row = malloc(sizeof(png_bytep) * width * 3);
	if (!row) {
		return false;
	}
	png_bytep pixelData = pixels;
	if (setjmp(png_jmpbuf(png))) {
		free(row);
		return false;
	}
	unsigned i;
	for (i = 0; i < height; ++i) {
		unsigned x;
		for (x = 0; x < width; ++x) {
			row[x * 3] = pixelData[stride * i * 4 + x * 4];
			row[x * 3 + 1] = pixelData[stride * i * 4 + x * 4 + 1];
			row[x * 3 + 2] = pixelData[stride * i * 4 + x * 4 + 2];
		}
		png_write_row(png, row);
	}
	free(row);
	return true;
}

bool PNGWriteCustomChunk(png_structp png, const char* name, size_t size, void* data) {
	char realName[5];
	strncpy(realName, name, 4);
	realName[0] = tolower(realName[0]);
	realName[1] = tolower(realName[1]);
	realName[4] = '\0';
	if (setjmp(png_jmpbuf(png))) {
		return false;
	}
	png_write_chunk(png, (const png_bytep) realName, data, size);
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
	source->read(source, header, PNG_HEADER_BYTES);
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
	png_set_keep_unknown_chunks(png, PNG_HANDLE_CHUNK_ALWAYS, (const png_bytep) chunkName, 1);
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
	if (setjmp(png_jmpbuf(png))) {
		return false;
	}

	uint8_t* pixelData = pixels;
	unsigned pngHeight = png_get_image_height(png, info);
	if (height > pngHeight) {
		height = pngHeight;
	}

	unsigned pngWidth = png_get_image_width(png, info);
	if (width > pngWidth) {
		width = pngWidth;
	}

	unsigned i;
	png_bytep row = malloc(png_get_rowbytes(png, info));
	for (i = 0; i < height; ++i) {
		png_read_row(png, row, 0);
		unsigned x;
		for (x = 0; x < width; ++x) {
			pixelData[stride * i * 4 + x * 4] = row[x * 3];
			pixelData[stride * i * 4 + x * 4 + 1] = row[x * 3 + 1];
			pixelData[stride * i * 4 + x * 4 + 2] = row[x * 3 + 2];
		}
	}
	free(row);
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
