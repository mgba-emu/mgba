/* Copyright (c) 2013-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef FFMPEG_DECODER
#define FFMPEG_DECODER

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/interface.h>

#include "feature/ffmpeg/ffmpeg-common.h"

#define FFMPEG_DECODER_BUFSIZE 4096

struct FFmpegDecoder {
	struct mAVStream* out;
	struct AVFormatContext* context;

	int audioStream;
	AVFrame* audioFrame;
	struct AVCodecContext* audio;

	int videoStream;
	AVFrame* videoFrame;
	struct AVCodecContext* video;
	struct SwsContext* scaleContext;

	int width;
	int height;
	uint8_t* pixels;
};

void FFmpegDecoderInit(struct FFmpegDecoder*);
bool FFmpegDecoderOpen(struct FFmpegDecoder*, const char* infile);
void FFmpegDecoderClose(struct FFmpegDecoder*);
bool FFmpegDecoderIsOpen(struct FFmpegDecoder*);
bool FFmpegDecoderRead(struct FFmpegDecoder*);

CXX_GUARD_END

#endif
