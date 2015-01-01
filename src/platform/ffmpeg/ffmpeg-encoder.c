/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ffmpeg-encoder.h"

#include "gba-video.h"

#include <libavcodec/version.h>
#include <libavcodec/avcodec.h>

#include <libavutil/version.h>
#if LIBAVUTIL_VERSION_MAJOR >= 53
#include <libavutil/buffer.h>
#endif
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>

#include <libavresample/avresample.h>
#include <libswscale/swscale.h>

static void _ffmpegPostVideoFrame(struct GBAAVStream*, struct GBAVideoRenderer* renderer);
static void _ffmpegPostAudioFrame(struct GBAAVStream*, int32_t left, int32_t right);

enum {
	PREFERRED_SAMPLE_RATE = 0x8000
};

void FFmpegEncoderInit(struct FFmpegEncoder* encoder) {
	av_register_all();

	encoder->d.postVideoFrame = _ffmpegPostVideoFrame;
	encoder->d.postAudioFrame = _ffmpegPostAudioFrame;

	encoder->audioCodec = 0;
	encoder->videoCodec = 0;
	encoder->containerFormat = 0;
	FFmpegEncoderSetAudio(encoder, "flac", 0);
	FFmpegEncoderSetVideo(encoder, "png", 0);
	FFmpegEncoderSetContainer(encoder, "matroska");
	FFmpegEncoderSetDimensions(encoder, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
	encoder->resampleContext = 0;
	encoder->absf = 0;
	encoder->context = 0;
}

bool FFmpegEncoderSetAudio(struct FFmpegEncoder* encoder, const char* acodec, unsigned abr) {
	static const struct {
		int format;
		int priority;
	} priorities[] = {
		{ AV_SAMPLE_FMT_S16, 0 },
		{ AV_SAMPLE_FMT_S16P, 1 },
		{ AV_SAMPLE_FMT_S32, 2 },
		{ AV_SAMPLE_FMT_S32P, 2 },
		{ AV_SAMPLE_FMT_FLT, 3 },
		{ AV_SAMPLE_FMT_FLTP, 3 },
		{ AV_SAMPLE_FMT_DBL, 4 },
		{ AV_SAMPLE_FMT_DBLP, 4 }
	};

	if (!acodec) {
		encoder->audioCodec = 0;
		return true;
	}

	AVCodec* codec = avcodec_find_encoder_by_name(acodec);
	if (!codec) {
		return false;
	}

	if (!codec->sample_fmts) {
		return false;
	}
	size_t i;
	size_t j;
	int priority = INT_MAX;
	encoder->sampleFormat = AV_SAMPLE_FMT_NONE;
	for (i = 0; codec->sample_fmts[i] != AV_SAMPLE_FMT_NONE; ++i) {
		for (j = 0; j < sizeof(priorities) / sizeof(*priorities); ++j) {
			if (codec->sample_fmts[i] == priorities[j].format && priority > priorities[j].priority) {
				priority = priorities[j].priority;
				encoder->sampleFormat = codec->sample_fmts[i];
			}
		}
	}
	if (encoder->sampleFormat == AV_SAMPLE_FMT_NONE) {
		return false;
	}
	encoder->sampleRate = PREFERRED_SAMPLE_RATE;
	if (codec->supported_samplerates) {
		for (i = 0; codec->supported_samplerates[i]; ++i) {
			if (codec->supported_samplerates[i] < PREFERRED_SAMPLE_RATE) {
				continue;
			}
			if (encoder->sampleRate == PREFERRED_SAMPLE_RATE || encoder->sampleRate > codec->supported_samplerates[i]) {
				encoder->sampleRate = codec->supported_samplerates[i];
			}
		}
	} else if (codec->id == AV_CODEC_ID_AAC) {
		// HACK: AAC doesn't support 32768Hz (it rounds to 32000), but libfaac doesn't tell us that
		encoder->sampleRate = 44100;
	}
	encoder->audioCodec = acodec;
	encoder->audioBitrate = abr;
	return true;
}

bool FFmpegEncoderSetVideo(struct FFmpegEncoder* encoder, const char* vcodec, unsigned vbr) {
	static const struct {
		enum AVPixelFormat format;
		int priority;
	} priorities[] = {
		{ AV_PIX_FMT_RGB555, 0 },
		{ AV_PIX_FMT_BGR555, 0 },
		{ AV_PIX_FMT_RGB565, 1 },
		{ AV_PIX_FMT_BGR565, 1 },
		{ AV_PIX_FMT_RGB24, 2 },
		{ AV_PIX_FMT_BGR24, 2 },
#ifndef USE_LIBAV
		{ AV_PIX_FMT_BGR0, 3 },
		{ AV_PIX_FMT_RGB0, 3 },
		{ AV_PIX_FMT_0BGR, 3 },
		{ AV_PIX_FMT_0RGB, 3 },
#endif
		{ AV_PIX_FMT_YUV422P, 4 },
		{ AV_PIX_FMT_YUV444P, 5 },
		{ AV_PIX_FMT_YUV420P, 6 }
	};
	AVCodec* codec = avcodec_find_encoder_by_name(vcodec);
	if (!codec) {
		return false;
	}

	size_t i;
	size_t j;
	int priority = INT_MAX;
	encoder->pixFormat = AV_PIX_FMT_NONE;
	for (i = 0; codec->pix_fmts[i] != AV_PIX_FMT_NONE; ++i) {
		for (j = 0; j < sizeof(priorities) / sizeof(*priorities); ++j) {
			if (codec->pix_fmts[i] == priorities[j].format && priority > priorities[j].priority) {
				priority = priorities[j].priority;
				encoder->pixFormat = codec->pix_fmts[i];
			}
		}
	}
	if (encoder->pixFormat == AV_PIX_FMT_NONE) {
		return false;
	}
	encoder->videoCodec = vcodec;
	encoder->videoBitrate = vbr;
	return true;
}

bool FFmpegEncoderSetContainer(struct FFmpegEncoder* encoder, const char* container) {
	AVOutputFormat* oformat = av_guess_format(container, 0, 0);
	if (!oformat) {
		return false;
	}
	encoder->containerFormat = container;
	return true;
}

void FFmpegEncoderSetDimensions(struct FFmpegEncoder* encoder, int width, int height) {
	encoder->width = width > 0 ? width : VIDEO_HORIZONTAL_PIXELS;
	encoder->height = height > 0 ? height : VIDEO_VERTICAL_PIXELS;
}

bool FFmpegEncoderVerifyContainer(struct FFmpegEncoder* encoder) {
	AVOutputFormat* oformat = av_guess_format(encoder->containerFormat, 0, 0);
	AVCodec* acodec = avcodec_find_encoder_by_name(encoder->audioCodec);
	AVCodec* vcodec = avcodec_find_encoder_by_name(encoder->videoCodec);
	if ((encoder->audioCodec && !acodec) || !vcodec || !oformat) {
		return false;
	}
	if (encoder->audioCodec && !avformat_query_codec(oformat, acodec->id, FF_COMPLIANCE_EXPERIMENTAL)) {
		return false;
	}
	if (!avformat_query_codec(oformat, vcodec->id, FF_COMPLIANCE_EXPERIMENTAL)) {
		return false;
	}
	return true;
}

bool FFmpegEncoderOpen(struct FFmpegEncoder* encoder, const char* outfile) {
	AVCodec* acodec = avcodec_find_encoder_by_name(encoder->audioCodec);
	AVCodec* vcodec = avcodec_find_encoder_by_name(encoder->videoCodec);
	if ((encoder->audioCodec && !acodec) || !vcodec || !FFmpegEncoderVerifyContainer(encoder)) {
		return false;
	}

	encoder->currentAudioSample = 0;
	encoder->currentAudioFrame = 0;
	encoder->currentVideoFrame = 0;
	encoder->nextAudioPts = 0;

	AVOutputFormat* oformat = av_guess_format(encoder->containerFormat, 0, 0);
#ifndef USE_LIBAV
	avformat_alloc_output_context2(&encoder->context, oformat, 0, outfile);
#else
	encoder->context = avformat_alloc_context();
	strncpy(encoder->context->filename, outfile, sizeof(encoder->context->filename));
	encoder->context->oformat = oformat;
#endif

	if (acodec) {
		encoder->audioStream = avformat_new_stream(encoder->context, acodec);
		encoder->audio = encoder->audioStream->codec;
		encoder->audio->bit_rate = encoder->audioBitrate;
		encoder->audio->channels = 2;
		encoder->audio->channel_layout = AV_CH_LAYOUT_STEREO;
		encoder->audio->sample_rate = encoder->sampleRate;
		encoder->audio->sample_fmt = encoder->sampleFormat;
		AVDictionary* opts = 0;
		av_dict_set(&opts, "strict", "-2", 0);
		if (encoder->context->oformat->flags & AVFMT_GLOBALHEADER) {
			encoder->audio->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}
		avcodec_open2(encoder->audio, acodec, &opts);
		av_dict_free(&opts);
#if LIBAVCODEC_VERSION_MAJOR >= 55
		encoder->audioFrame = av_frame_alloc();
#else
		encoder->audioFrame = avcodec_alloc_frame();
#endif
		if (!encoder->audio->frame_size) {
			encoder->audio->frame_size = 1;
		}
		encoder->audioFrame->nb_samples = encoder->audio->frame_size;
		encoder->audioFrame->format = encoder->audio->sample_fmt;
		encoder->audioFrame->pts = 0;
		encoder->resampleContext = avresample_alloc_context();
		av_opt_set_int(encoder->resampleContext, "in_channel_layout", AV_CH_LAYOUT_STEREO, 0);
		av_opt_set_int(encoder->resampleContext, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
		av_opt_set_int(encoder->resampleContext, "in_sample_rate", PREFERRED_SAMPLE_RATE, 0);
		av_opt_set_int(encoder->resampleContext, "out_sample_rate", encoder->sampleRate, 0);
		av_opt_set_int(encoder->resampleContext, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
		av_opt_set_int(encoder->resampleContext, "out_sample_fmt", encoder->sampleFormat, 0);
		avresample_open(encoder->resampleContext);
		encoder->audioBufferSize = (encoder->audioFrame->nb_samples * PREFERRED_SAMPLE_RATE / encoder->sampleRate) * 4;
		encoder->audioBuffer = av_malloc(encoder->audioBufferSize);
		encoder->postaudioBufferSize = av_samples_get_buffer_size(0, encoder->audio->channels, encoder->audio->frame_size, encoder->audio->sample_fmt, 0);
		encoder->postaudioBuffer = av_malloc(encoder->postaudioBufferSize);
		avcodec_fill_audio_frame(encoder->audioFrame, encoder->audio->channels, encoder->audio->sample_fmt, (const uint8_t*) encoder->postaudioBuffer, encoder->postaudioBufferSize, 0);

		if (encoder->audio->codec->id == AV_CODEC_ID_AAC &&
			(strcasecmp(encoder->containerFormat, "mp4") ||
			strcasecmp(encoder->containerFormat, "m4v") ||
			strcasecmp(encoder->containerFormat, "mov"))) {
			// MP4 container doesn't support the raw ADTS AAC format that the encoder spits out
			encoder->absf = av_bitstream_filter_init("aac_adtstoasc");
		}
	}

	encoder->videoStream = avformat_new_stream(encoder->context, vcodec);
	encoder->video = encoder->videoStream->codec;
	encoder->video->bit_rate = encoder->videoBitrate;
	encoder->video->width = encoder->width;
	encoder->video->height = encoder->height;
	encoder->video->time_base = (AVRational) { VIDEO_TOTAL_LENGTH, GBA_ARM7TDMI_FREQUENCY };
	encoder->video->pix_fmt = encoder->pixFormat;
	encoder->video->gop_size = 60;
	encoder->video->max_b_frames = 3;
	if (encoder->context->oformat->flags & AVFMT_GLOBALHEADER) {
		encoder->video->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}
	if (strcmp(vcodec->name, "libx264") == 0) {
		// Try to adaptively figure out when you can use a slower encoder
		if (encoder->width * encoder->height > 1000000) {
			av_opt_set(encoder->video->priv_data, "preset", "superfast", 0);
		} else if (encoder->width * encoder->height > 500000) {
			av_opt_set(encoder->video->priv_data, "preset", "veryfast", 0);
		} else {
			av_opt_set(encoder->video->priv_data, "preset", "faster", 0);
		}
		av_opt_set(encoder->video->priv_data, "tune", "zerolatency", 0);
	}
	avcodec_open2(encoder->video, vcodec, 0);
#if LIBAVCODEC_VERSION_MAJOR >= 55
	encoder->videoFrame = av_frame_alloc();
#else
	encoder->videoFrame = avcodec_alloc_frame();
#endif
	encoder->videoFrame->format = encoder->video->pix_fmt;
	encoder->videoFrame->width = encoder->video->width;
	encoder->videoFrame->height = encoder->video->height;
	encoder->videoFrame->pts = 0;
	encoder->scaleContext = sws_getContext(VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS,
#ifndef USE_LIBAV
		AV_PIX_FMT_0BGR32,
#else
		AV_PIX_FMT_BGR32,
#endif
		encoder->videoFrame->width, encoder->videoFrame->height, encoder->video->pix_fmt,
		SWS_POINT, 0, 0, 0);
	av_image_alloc(encoder->videoFrame->data, encoder->videoFrame->linesize, encoder->video->width, encoder->video->height, encoder->video->pix_fmt, 32);

	avio_open(&encoder->context->pb, outfile, AVIO_FLAG_WRITE);
	avformat_write_header(encoder->context, 0);

	return true;
}

void FFmpegEncoderClose(struct FFmpegEncoder* encoder) {
	if (!encoder->context) {
		return;
	}
	av_write_trailer(encoder->context);
	avio_close(encoder->context->pb);

	if (encoder->audioCodec) {
		av_free(encoder->postaudioBuffer);
		if (encoder->audioBuffer) {
			av_free(encoder->audioBuffer);
		}
#if LIBAVCODEC_VERSION_MAJOR >= 55
		av_frame_free(&encoder->audioFrame);
#else
		avcodec_free_frame(&encoder->audioFrame);
#endif
		avcodec_close(encoder->audio);

		if (encoder->resampleContext) {
			avresample_close(encoder->resampleContext);
		}

		if (encoder->absf) {
			av_bitstream_filter_close(encoder->absf);
			encoder->absf = 0;
		}
	}

#if LIBAVCODEC_VERSION_MAJOR >= 55
	av_frame_free(&encoder->videoFrame);
#else
	avcodec_free_frame(&encoder->videoFrame);
#endif
	avcodec_close(encoder->video);

	sws_freeContext(encoder->scaleContext);

	avformat_free_context(encoder->context);
	encoder->context = 0;
}

bool FFmpegEncoderIsOpen(struct FFmpegEncoder* encoder) {
	return !!encoder->context;
}

void _ffmpegPostAudioFrame(struct GBAAVStream* stream, int32_t left, int32_t right) {
	struct FFmpegEncoder* encoder = (struct FFmpegEncoder*) stream;
	if (!encoder->context || !encoder->audioCodec) {
		return;
	}

	encoder->audioBuffer[encoder->currentAudioSample * 2] = left;
	encoder->audioBuffer[encoder->currentAudioSample * 2 + 1] = right;

	++encoder->currentAudioFrame;
	++encoder->currentAudioSample;

	if ((encoder->currentAudioSample * 4) < encoder->audioBufferSize) {
		return;
	}
	encoder->currentAudioSample = 0;

	int channelSize = 2 * av_get_bytes_per_sample(encoder->audio->sample_fmt);
	avresample_convert(encoder->resampleContext,
		0, 0, 0,
		(uint8_t**) &encoder->audioBuffer, 0, encoder->audioBufferSize / 4);
	if (avresample_available(encoder->resampleContext) < encoder->audioFrame->nb_samples) {
		return;
	}
#if LIBAVCODEC_VERSION_MAJOR >= 55
	av_frame_make_writable(encoder->audioFrame);
#endif
	avresample_read(encoder->resampleContext, encoder->audioFrame->data, encoder->postaudioBufferSize / channelSize);

	AVRational timeBase = { 1, PREFERRED_SAMPLE_RATE };
	encoder->audioFrame->pts = encoder->nextAudioPts;
	encoder->nextAudioPts = av_rescale_q(encoder->currentAudioFrame, timeBase, encoder->audioStream->time_base);

	AVPacket packet;
	av_init_packet(&packet);
	packet.data = 0;
	packet.size = 0;
	int gotData;
	avcodec_encode_audio2(encoder->audio, &packet, encoder->audioFrame, &gotData);
	if (gotData) {
		if (encoder->absf) {
			AVPacket tempPacket = packet;
			int success = av_bitstream_filter_filter(encoder->absf, encoder->audio, 0,
				&tempPacket.data, &tempPacket.size,
				packet.data, packet.size, 0);
			if (success > 0) {
#if LIBAVUTIL_VERSION_MAJOR >= 53
				tempPacket.buf = av_buffer_create(tempPacket.data, tempPacket.size, av_buffer_default_free, 0, 0);
#endif
				av_free_packet(&packet);
			}
			packet = tempPacket;
		}
		packet.stream_index = encoder->audioStream->index;
		av_interleaved_write_frame(encoder->context, &packet);
	}
	av_free_packet(&packet);
}

void _ffmpegPostVideoFrame(struct GBAAVStream* stream, struct GBAVideoRenderer* renderer) {
	struct FFmpegEncoder* encoder = (struct FFmpegEncoder*) stream;
	if (!encoder->context) {
		return;
	}
	uint8_t* pixels;
	unsigned stride;
	renderer->getPixels(renderer, &stride, (void**) &pixels);
	stride *= 4;

	AVPacket packet;

	av_init_packet(&packet);
	packet.data = 0;
	packet.size = 0;
#if LIBAVCODEC_VERSION_MAJOR >= 55
	av_frame_make_writable(encoder->videoFrame);
#endif
	encoder->videoFrame->pts = av_rescale_q(encoder->currentVideoFrame, encoder->video->time_base, encoder->videoStream->time_base);
	++encoder->currentVideoFrame;

	sws_scale(encoder->scaleContext, (const uint8_t* const*) &pixels, (const int*) &stride, 0, VIDEO_VERTICAL_PIXELS, encoder->videoFrame->data, encoder->videoFrame->linesize);

	int gotData;
	avcodec_encode_video2(encoder->video, &packet, encoder->videoFrame, &gotData);
	if (gotData) {
		if (encoder->videoStream->codec->coded_frame->key_frame) {
			packet.flags |= AV_PKT_FLAG_KEY;
		}
		packet.stream_index = encoder->videoStream->index;
		av_interleaved_write_frame(encoder->context, &packet);
	}
	av_free_packet(&packet);
}
