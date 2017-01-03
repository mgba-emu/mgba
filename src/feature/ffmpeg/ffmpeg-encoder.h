/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef FFMPEG_ENCODER
#define FFMPEG_ENCODER

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/internal/gba/gba.h>

#include <libavformat/avformat.h>
#include <libavcodec/version.h>

// Version 57.16 in FFmpeg
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 37, 100)
#define FFMPEG_USE_PACKETS
#endif

// Version 57.15 in libav
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 35, 0)
#define FFMPEG_USE_NEW_BSF
#endif

// Version 57.14 in libav
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 48, 0)
#define FFMPEG_USE_CODECPAR
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 8, 0)
#define FFMPEG_USE_PACKET_UNREF
#endif

struct FFmpegEncoder {
	struct mAVStream d;
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
	int64_t nextAudioPts; // TODO (0.6): Remove
	struct AVAudioResampleContext* resampleContext;
#ifdef FFMPEG_USE_NEW_BSF
	struct AVBSFContext* absf; // Needed for AAC in MP4
#else
	struct AVBitStreamFilterContext* absf; // Needed for AAC in MP4
#endif
	struct AVStream* audioStream;

	struct AVCodecContext* video;
	enum AVPixelFormat pixFormat;
	struct AVFrame* videoFrame;
	int width;
	int height;
	int iwidth;
	int iheight;
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

CXX_GUARD_END

#endif
