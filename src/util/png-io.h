#ifndef PNG_IO_H
#define PNG_IO_H

#include "common.h"

#include <png.h>

struct VFile;

png_structp PNGWriteOpen(struct VFile* source);
png_infop PNGWriteHeader(png_structp png, unsigned width, unsigned height);
bool PNGWritePixels(png_structp png, unsigned width, unsigned height, unsigned stride, void* pixels);
bool PNGWriteCustomChunk(png_structp png, const char* name, size_t size, void* data);
void PNGWriteClose(png_structp png, png_infop info);

#endif
