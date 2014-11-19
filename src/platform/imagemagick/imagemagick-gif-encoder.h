#ifndef IMAGEMAGICK_GIF_ENCODER
#define IMAGEMAGICK_GIF_ENCODER

#include "gba-thread.h"

#include <wand/MagickWand.h>

struct ImageMagickGIFEncoder {
	struct GBAAVStream d;
	MagickWand* wand;
	char* outfile;
	uint32_t* frame;

	unsigned currentFrame;
	int frameskip;
};

void ImageMagickGIFEncoderInit(struct ImageMagickGIFEncoder*);
bool ImageMagickGIFEncoderOpen(struct ImageMagickGIFEncoder*, const char* outfile);
void ImageMagickGIFEncoderClose(struct ImageMagickGIFEncoder*);
bool ImageMagickGIFEncoderIsOpen(struct ImageMagickGIFEncoder*);

#endif
