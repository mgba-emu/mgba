#ifndef FFMPEG_ENCODER
#define FFMPEG_ENCODER

#include "gba-thread.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

struct FFmpegEncoder {
	struct GBAAVStream d;
	AVFormatContext* context;

	unsigned audioBitrate;
	const char* audioCodec;

	unsigned videoBitrate;
	const char* videoCodec;

	AVCodecContext* audio;
	uint16_t* audioBuffer;
	size_t audioBufferSize;
	AVFrame* audioFrame;
	size_t currentAudioSample;
	int64_t currentAudioFrame;
	AVStream* audioStream;

	AVCodecContext* video;
	enum AVPixelFormat pixFormat;
	AVFrame* videoFrame;
	int64_t currentVideoFrame;
	AVStream* videoStream;
};

void FFmpegEncoderInit(struct FFmpegEncoder*);
bool FFmpegEncoderSetAudio(struct FFmpegEncoder*, const char* acodec, unsigned abr);
bool FFmpegEncoderSetVideo(struct FFmpegEncoder*, const char* vcodec, unsigned vbr);
bool FFmpegEncoderOpen(struct FFmpegEncoder*, const char* outfile);
void FFmpegEncoderClose(struct FFmpegEncoder*);

#endif
