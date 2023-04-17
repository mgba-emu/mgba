/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef PNG_IO_H
#define PNG_IO_H

#include <mgba-util/common.h>

CXX_GUARD_START

#ifdef USE_PNG

#include <mgba-util/image.h>

// png.h defines its own version of restrict which conflicts with mGBA's.
#ifdef restrict
#undef restrict
#endif
#include <png.h>

struct VFile;

enum {
	PNG_HEADER_BYTES = 8
};

png_structp PNGWriteOpen(struct VFile* source);
png_infop PNGWriteHeader(png_structp png, unsigned width, unsigned height, enum mColorFormat);
png_infop PNGWriteHeaderPalette(png_structp png, unsigned width, unsigned height, const uint32_t* palette, unsigned entries);
bool PNGWritePixels(png_structp png, unsigned width, unsigned height, unsigned stride, const void* pixels, enum mColorFormat);
bool PNGWritePixelsPalette(png_structp png, unsigned width, unsigned height, unsigned stride, const void* pixels);
bool PNGWriteCustomChunk(png_structp png, const char* name, size_t size, void* data);
void PNGWriteClose(png_structp png, png_infop info);

typedef int (*ChunkHandler)(png_structp, png_unknown_chunkp);

bool isPNG(struct VFile* source);
png_structp PNGReadOpen(struct VFile* source, unsigned offset);
bool PNGInstallChunkHandler(png_structp png, void* context, ChunkHandler handler, const char* chunkName);
bool PNGReadHeader(png_structp png, png_infop info);
bool PNGReadPixels(png_structp png, png_infop info, void* pixels, unsigned width, unsigned height, unsigned stride);
bool PNGReadPixelsA(png_structp png, png_infop info, void* pixels, unsigned width, unsigned height, unsigned stride);
bool PNGReadPixels8(png_structp png, png_infop info, void* pixels, unsigned width, unsigned height, unsigned stride);
bool PNGIgnorePixels(png_structp png, png_infop info);
bool PNGReadFooter(png_structp png, png_infop end);
void PNGReadClose(png_structp png, png_infop info, png_infop end);

#endif

CXX_GUARD_END

#endif
