/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef IMAGEMAGICK_GIF_ENCODER
#define IMAGEMAGICK_GIF_ENCODER

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/interface.h>

#if MAGICKWAND_VERSION_MAJOR >= 7
#include <MagickWand/MagickWand.h>
#else
#include <wand/MagickWand.h>
#endif

struct ImageMagickGIFEncoder {
	struct mAVStream d;
	MagickWand* wand;
	char* outfile;
	uint32_t* frame;

	unsigned currentFrame;
	int frameskip;
	int delayMs;

	unsigned iwidth;
	unsigned iheight;
};

void ImageMagickGIFEncoderInit(struct ImageMagickGIFEncoder*);
void ImageMagickGIFEncoderSetParams(struct ImageMagickGIFEncoder* encoder, int frameskip, int delayMs);
bool ImageMagickGIFEncoderOpen(struct ImageMagickGIFEncoder*, const char* outfile);
bool ImageMagickGIFEncoderClose(struct ImageMagickGIFEncoder*);
bool ImageMagickGIFEncoderIsOpen(struct ImageMagickGIFEncoder*);

CXX_GUARD_END

#endif
