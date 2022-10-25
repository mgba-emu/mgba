/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef FFMPEG_ENCODER
#define FFMPEG_ENCODER

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/interface.h>

#include "feature/ffmpeg/ffmpeg-common.h"

#define FFMPEG_FILTERS_MAX 4

struct FFmpegEncoder {
	struct mAVStream d;
	struct AVFormatContext* context;

	unsigned audioBitrate;
	const char* audioCodec;

	int videoBitrate;
	const char* videoCodec;

	const char* containerFormat;

	struct AVCodecContext* audio;
	enum AVSampleFormat sampleFormat;
	int sampleRate;
	uint16_t* audioBuffer;
	size_t audioBufferSize;
	AVFrame* audioFrame;
	size_t currentAudioSample;
	int64_t currentAudioFrame;
#ifdef USE_LIBAVRESAMPLE
	struct AVAudioResampleContext* resampleContext;
#else
	struct SwrContext* resampleContext;
#endif
#ifdef FFMPEG_USE_NEW_BSF
	struct AVBSFContext* absf; // Needed for AAC in MP4
#else
	struct AVBitStreamFilterContext* absf; // Needed for AAC in MP4
#endif
	struct AVStream* audioStream;

	struct AVCodecContext* video;
	enum AVPixelFormat pixFormat;
	enum AVPixelFormat ipixFormat;
	AVFrame* videoFrame;
	int width;
	int height;
	int iwidth;
	int iheight;
	int isampleRate;
	int frameCycles;
	int cycles;
	int frameskip;
	int skipResidue;
	bool loop;
	int64_t currentVideoFrame;
	struct SwsContext* scaleContext;
	struct AVStream* videoStream;

	struct AVFilterGraph* graph;
	struct AVFilterContext* source;
	struct AVFilterContext* sink;
	struct AVFilterContext* filters[FFMPEG_FILTERS_MAX];
	struct AVFrame* sinkFrame;
};

void FFmpegEncoderInit(struct FFmpegEncoder*);
bool FFmpegEncoderSetAudio(struct FFmpegEncoder*, const char* acodec, unsigned abr);
bool FFmpegEncoderSetVideo(struct FFmpegEncoder*, const char* vcodec, int vbr, int frameskip);
bool FFmpegEncoderSetContainer(struct FFmpegEncoder*, const char* container);
void FFmpegEncoderSetDimensions(struct FFmpegEncoder*, int width, int height);
void FFmpegEncoderSetInputFrameRate(struct FFmpegEncoder*, int numerator, int denominator);
void FFmpegEncoderSetInputSampleRate(struct FFmpegEncoder*, int sampleRate);
void FFmpegEncoderSetLooping(struct FFmpegEncoder*, bool loop);
bool FFmpegEncoderVerifyContainer(struct FFmpegEncoder*);
bool FFmpegEncoderOpen(struct FFmpegEncoder*, const char* outfile);
void FFmpegEncoderClose(struct FFmpegEncoder*);
bool FFmpegEncoderIsOpen(struct FFmpegEncoder*);

CXX_GUARD_END

#endif
