/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef IMAGEMAGICK_GIF_ENCODER
#define IMAGEMAGICK_GIF_ENCODER

#include "gba-thread.h"

#define MAGICKCORE_HDRI_ENABLE 0
#define MAGICKCORE_QUANTUM_DEPTH 8

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
