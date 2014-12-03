/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef FFMPEG_ENCODER
#define FFMPEG_ENCODER

#include "gba-thread.h"

#include <libavformat/avformat.h>

struct FFmpegEncoder {
	struct GBAAVStream d;
	struct AVFormatContext* context;

	unsigned audioBitrate;
	const char* audioCodec;

	unsigned videoBitrate;
	const char* videoCodec;

	const char* containerFormat;

	struct AVCodecContext* audio;
	enum AVSampleFormat sampleFormat;
	int sampleRate;
	uint16_t* audioBuffer;
	size_t audioBufferSize;
	uint16_t* postaudioBuffer;
	size_t postaudioBufferSize;
	AVFrame* audioFrame;
	size_t currentAudioSample;
	int64_t currentAudioFrame;
	int64_t nextAudioPts;
	struct AVAudioResampleContext* resampleContext;
	struct AVBitStreamFilterContext* absf; // Needed for AAC in MP4
	struct AVStream* audioStream;

	struct AVCodecContext* video;
	enum AVPixelFormat pixFormat;
	struct AVFrame* videoFrame;
	int width;
	int height;
	int64_t currentVideoFrame;
	struct SwsContext* scaleContext;
	struct AVStream* videoStream;
};

void FFmpegEncoderInit(struct FFmpegEncoder*);
bool FFmpegEncoderSetAudio(struct FFmpegEncoder*, const char* acodec, unsigned abr);
bool FFmpegEncoderSetVideo(struct FFmpegEncoder*, const char* vcodec, unsigned vbr);
bool FFmpegEncoderSetContainer(struct FFmpegEncoder*, const char* container);
void FFmpegEncoderSetDimensions(struct FFmpegEncoder*, int width, int height);
bool FFmpegEncoderVerifyContainer(struct FFmpegEncoder*);
bool FFmpegEncoderOpen(struct FFmpegEncoder*, const char* outfile);
void FFmpegEncoderClose(struct FFmpegEncoder*);
bool FFmpegEncoderIsOpen(struct FFmpegEncoder*);

#endif
