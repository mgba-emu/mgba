#ifndef FFMPEG_ENCODER
#define FFMPEG_ENCODER

#include "gba-thread.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

struct FFmpegEncoder {
	struct GBAAVStream d;
	AVFormatContext* context;

	AVCodecContext* audio;
	uint16_t* audioBuffer;
	size_t audioBufferSize;
	AVFrame* audioFrame;
	size_t currentAudioSample;
	int64_t currentAudioFrame;
	AVStream* audioStream;

	AVCodecContext* video;
	AVFrame* videoFrame;
	int64_t currentVideoFrame;
	AVStream* videoStream;
};

bool FFmpegEncoderCreate(struct FFmpegEncoder*);
void FFmpegEncoderDestroy(struct FFmpegEncoder*);

#endif
