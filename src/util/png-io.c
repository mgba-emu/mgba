#include "util/png-io.h"

#include "vfs.h"

static void _pngWrite(png_structp png, png_bytep buffer, png_size_t size) {
	struct VFile* vf = png_get_io_ptr(png);
	size_t written = vf->write(vf, buffer, size);
	if (written != size) {
		png_error(png, "Could not write PNG");
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
	png_write_chunk(png, (png_const_bytep) realName, data, size);
	return true;
}

void PNGWriteClose(png_structp png, png_infop info) {
	png_write_end(png, info);
	png_destroy_write_struct(&png, &info);
}
