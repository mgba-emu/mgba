/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "imagemagick-gif-encoder.h"

#include "gba-video.h"

static void _magickPostVideoFrame(struct GBAAVStream*, struct GBAVideoRenderer* renderer);
static void _magickPostAudioFrame(struct GBAAVStream*, int32_t left, int32_t right);

void ImageMagickGIFEncoderInit(struct ImageMagickGIFEncoder* encoder) {
	encoder->wand = 0;

	encoder->d.postVideoFrame = _magickPostVideoFrame;
	encoder->d.postAudioFrame = _magickPostAudioFrame;

	encoder->frameskip = 2;
}

bool ImageMagickGIFEncoderOpen(struct ImageMagickGIFEncoder* encoder, const char* outfile) {
	MagickWandGenesis();
	encoder->wand = NewMagickWand();
	encoder->outfile = strdup(outfile);
	encoder->frame = malloc(VIDEO_HORIZONTAL_PIXELS * VIDEO_VERTICAL_PIXELS * 4);
	encoder->currentFrame = 0;
	return true;
}
void ImageMagickGIFEncoderClose(struct ImageMagickGIFEncoder* encoder) {
	if (!encoder->wand) {
		return;
	}
	MagickWriteImages(encoder->wand, encoder->outfile, MagickTrue);
	free(encoder->outfile);
	free(encoder->frame);
	DestroyMagickWand(encoder->wand);
	encoder->wand = 0;
	MagickWandTerminus();
}

bool ImageMagickGIFEncoderIsOpen(struct ImageMagickGIFEncoder* encoder) {
	return !!encoder->wand;
}

static void _magickPostVideoFrame(struct GBAAVStream* stream, struct GBAVideoRenderer* renderer) {
	struct ImageMagickGIFEncoder* encoder = (struct ImageMagickGIFEncoder*) stream;

	if (encoder->currentFrame % (encoder->frameskip + 1)) {
		++encoder->currentFrame;
		return;
	}

	uint8_t* pixels;
	unsigned stride;
	renderer->getPixels(renderer, &stride, (void**) &pixels);
	size_t row;
	for (row = 0; row < VIDEO_VERTICAL_PIXELS; ++row) {
		memcpy(&encoder->frame[row * VIDEO_HORIZONTAL_PIXELS], &pixels[row * 4 *stride], VIDEO_HORIZONTAL_PIXELS * 4);
	}

	MagickConstituteImage(encoder->wand, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, "RGBP", CharPixel, encoder->frame);
	uint64_t ts = encoder->currentFrame;
	uint64_t nts = encoder->currentFrame + encoder->frameskip + 1;
	ts *= VIDEO_TOTAL_LENGTH * 100;
	nts *= VIDEO_TOTAL_LENGTH * 100;
	ts /= GBA_ARM7TDMI_FREQUENCY;
	nts /= GBA_ARM7TDMI_FREQUENCY;
	MagickSetImageDelay(encoder->wand, nts - ts);
	++encoder->currentFrame;
}

static void _magickPostAudioFrame(struct GBAAVStream* stream, int32_t left, int32_t right) {
	UNUSED(stream);
	UNUSED(left);
	UNUSED(right);
	// This is a video-only format...
}
