/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ffmpeg-encoder.h"

#include <mgba/core/core.h>
#include <mgba/internal/gba/video.h>

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

static void _ffmpegPostVideoFrame(struct mAVStream*, const color_t* pixels, size_t stride);
static void _ffmpegPostAudioFrame(struct mAVStream*, int16_t left, int16_t right);
static void _ffmpegSetVideoDimensions(struct mAVStream*, unsigned width, unsigned height);

enum {
	PREFERRED_SAMPLE_RATE = 0x8000
};

void FFmpegEncoderInit(struct FFmpegEncoder* encoder) {
	av_register_all();

	encoder->d.videoDimensionsChanged = _ffmpegSetVideoDimensions;
	encoder->d.postVideoFrame = _ffmpegPostVideoFrame;
	encoder->d.postAudioFrame = _ffmpegPostAudioFrame;
	encoder->d.postAudioBuffer = 0;

	encoder->audioCodec = 0;
	encoder->videoCodec = 0;
	encoder->containerFormat = 0;
	FFmpegEncoderSetAudio(encoder, "flac", 0);
	FFmpegEncoderSetVideo(encoder, "png", 0);
	FFmpegEncoderSetContainer(encoder, "matroska");
	FFmpegEncoderSetDimensions(encoder, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
	encoder->iwidth = VIDEO_HORIZONTAL_PIXELS;
	encoder->iheight = VIDEO_VERTICAL_PIXELS;
	encoder->resampleContext = 0;
	encoder->absf = 0;
	encoder->context = 0;
	encoder->scaleContext = NULL;
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
	strncpy(encoder->context->filename, outfile, sizeof(encoder->context->filename) - 1);
	encoder->context->filename[sizeof(encoder->context->filename) - 1] = '\0';
	encoder->context->oformat = oformat;
#endif

	if (acodec) {
#ifdef FFMPEG_USE_CODECPAR
		encoder->audioStream = avformat_new_stream(encoder->context, NULL);
		encoder->audio = avcodec_alloc_context3(acodec);
#else
		encoder->audioStream = avformat_new_stream(encoder->context, acodec);
		encoder->audio = encoder->audioStream->codec;
#endif
		encoder->audio->bit_rate = encoder->audioBitrate;
		encoder->audio->channels = 2;
		encoder->audio->channel_layout = AV_CH_LAYOUT_STEREO;
		encoder->audio->sample_rate = encoder->sampleRate;
		encoder->audio->sample_fmt = encoder->sampleFormat;
		AVDictionary* opts = 0;
		av_dict_set(&opts, "strict", "-2", 0);
		if (encoder->context->oformat->flags & AVFMT_GLOBALHEADER) {
#ifdef AV_CODEC_FLAG_GLOBAL_HEADER
			encoder->audio->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
#else
			encoder->audio->flags |= CODEC_FLAG_GLOBAL_HEADER;
#endif
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
#ifdef FFMPEG_USE_NEW_BSF
			av_bsf_alloc(av_bsf_get_by_name("aac_adtstoasc"), &encoder->absf);
			avcodec_parameters_from_context(encoder->absf->par_in, encoder->audio);
			av_bsf_init(encoder->absf);
#else
			encoder->absf = av_bitstream_filter_init("aac_adtstoasc");
#endif
		}
#ifdef FFMPEG_USE_CODECPAR
		avcodec_parameters_from_context(encoder->audioStream->codecpar, encoder->audio);
#endif
	}

#ifdef FFMPEG_USE_CODECPAR
	encoder->videoStream = avformat_new_stream(encoder->context, NULL);
	encoder->video = avcodec_alloc_context3(vcodec);
#else
	encoder->videoStream = avformat_new_stream(encoder->context, vcodec);
	encoder->video = encoder->videoStream->codec;
#endif
	encoder->video->bit_rate = encoder->videoBitrate;
	encoder->video->width = encoder->width;
	encoder->video->height = encoder->height;
	encoder->video->time_base = (AVRational) { VIDEO_TOTAL_LENGTH, GBA_ARM7TDMI_FREQUENCY };
	encoder->video->pix_fmt = encoder->pixFormat;
	encoder->video->gop_size = 60;
	encoder->video->max_b_frames = 3;
	if (encoder->context->oformat->flags & AVFMT_GLOBALHEADER) {
#ifdef AV_CODEC_FLAG_GLOBAL_HEADER
		encoder->video->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
#else
		encoder->video->flags |= CODEC_FLAG_GLOBAL_HEADER;
#endif
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

	if (encoder->video->codec->id == AV_CODEC_ID_H264 &&
	    (strcasecmp(encoder->containerFormat, "mp4") ||
	        strcasecmp(encoder->containerFormat, "m4v") ||
	        strcasecmp(encoder->containerFormat, "mov"))) {
		// QuickTime and a few other things require YUV420
		encoder->video->pix_fmt = AV_PIX_FMT_YUV420P;
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
	_ffmpegSetVideoDimensions(&encoder->d, encoder->iwidth, encoder->iheight);
	av_image_alloc(encoder->videoFrame->data, encoder->videoFrame->linesize, encoder->video->width, encoder->video->height, encoder->video->pix_fmt, 32);
#ifdef FFMPEG_USE_CODECPAR
	avcodec_parameters_from_context(encoder->videoStream->codecpar, encoder->video);
#endif

	if (avio_open(&encoder->context->pb, outfile, AVIO_FLAG_WRITE) < 0) {
		return false;
	}
	return avformat_write_header(encoder->context, 0) >= 0;
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
#ifdef FFMPEG_USE_NEW_BSF
			av_bsf_free(&encoder->absf);
#else
			av_bitstream_filter_close(encoder->absf);
			encoder->absf = 0;
#endif
		}
	}

#if LIBAVCODEC_VERSION_MAJOR >= 55
	av_frame_free(&encoder->videoFrame);
#else
	avcodec_free_frame(&encoder->videoFrame);
#endif
	avcodec_close(encoder->video);

	sws_freeContext(encoder->scaleContext);
	encoder->scaleContext = NULL;

	avformat_free_context(encoder->context);
	encoder->context = 0;
}

bool FFmpegEncoderIsOpen(struct FFmpegEncoder* encoder) {
	return !!encoder->context;
}

void _ffmpegPostAudioFrame(struct mAVStream* stream, int16_t left, int16_t right) {
	struct FFmpegEncoder* encoder = (struct FFmpegEncoder*) stream;
	if (!encoder->context || !encoder->audioCodec) {
		return;
	}

	if (encoder->absf && !left) {
		// XXX: AVBSF doesn't like silence. Figure out why.
		left = 1;
	}

	encoder->audioBuffer[encoder->currentAudioSample * 2] = left;
	encoder->audioBuffer[encoder->currentAudioSample * 2 + 1] = right;

	++encoder->currentAudioSample;

	if (encoder->currentAudioSample * 4 < encoder->audioBufferSize) {
		return;
	}

	int channelSize = 2 * av_get_bytes_per_sample(encoder->audio->sample_fmt);
	avresample_convert(encoder->resampleContext, 0, 0, 0,
	                   (uint8_t**) &encoder->audioBuffer, 0, encoder->audioBufferSize / 4);

	encoder->currentAudioSample = 0;
	if (avresample_available(encoder->resampleContext) < encoder->audioFrame->nb_samples) {
		return;
	}
#if LIBAVCODEC_VERSION_MAJOR >= 55
	av_frame_make_writable(encoder->audioFrame);
#endif
	int samples = avresample_read(encoder->resampleContext, encoder->audioFrame->data, encoder->postaudioBufferSize / channelSize);

	encoder->audioFrame->pts = av_rescale_q(encoder->currentAudioFrame, encoder->audio->time_base, encoder->audioStream->time_base);
	encoder->currentAudioFrame += samples;

	AVPacket packet;
	av_init_packet(&packet);
	packet.data = 0;
	packet.size = 0;
	packet.pts = encoder->audioFrame->pts;

	int gotData;
#ifdef FFMPEG_USE_PACKETS
	avcodec_send_frame(encoder->audio, encoder->audioFrame);
	gotData = avcodec_receive_packet(encoder->audio, &packet);
	gotData = (gotData == 0) && packet.size;
#else
	avcodec_encode_audio2(encoder->audio, &packet, encoder->audioFrame, &gotData);
#endif
	if (gotData) {
		if (encoder->absf) {
			AVPacket tempPacket;

#ifdef FFMPEG_USE_NEW_BSF
			int success = av_bsf_send_packet(encoder->absf, &packet);
			if (success >= 0) {
				success = av_bsf_receive_packet(encoder->absf, &tempPacket);
			}
#else
			int success = av_bitstream_filter_filter(encoder->absf, encoder->audio, 0,
			    &tempPacket.data, &tempPacket.size,
			    packet.data, packet.size, 0);
#endif

			if (success >= 0) {
#if LIBAVUTIL_VERSION_MAJOR >= 53
				tempPacket.buf = av_buffer_create(tempPacket.data, tempPacket.size, av_buffer_default_free, 0, 0);
#endif

#ifdef FFMPEG_USE_PACKET_UNREF
				av_packet_move_ref(&packet, &tempPacket);
#else
				av_free_packet(&packet);
				packet = tempPacket;
#endif

				packet.stream_index = encoder->audioStream->index;
				av_interleaved_write_frame(encoder->context, &packet);
			}
		} else {
			packet.stream_index = encoder->audioStream->index;
			av_interleaved_write_frame(encoder->context, &packet);
		}
	}
#ifdef FFMPEG_USE_PACKET_UNREF
	av_packet_unref(&packet);
#else
	av_free_packet(&packet);
#endif
}

void _ffmpegPostVideoFrame(struct mAVStream* stream, const color_t* pixels, size_t stride) {
	struct FFmpegEncoder* encoder = (struct FFmpegEncoder*) stream;
	if (!encoder->context) {
		return;
	}
	stride *= BYTES_PER_PIXEL;

	AVPacket packet;

	av_init_packet(&packet);
	packet.data = 0;
	packet.size = 0;
#if LIBAVCODEC_VERSION_MAJOR >= 55
	av_frame_make_writable(encoder->videoFrame);
#endif
	encoder->videoFrame->pts = av_rescale_q(encoder->currentVideoFrame, encoder->video->time_base, encoder->videoStream->time_base);
	packet.pts = encoder->videoFrame->pts;
	++encoder->currentVideoFrame;

	sws_scale(encoder->scaleContext, (const uint8_t* const*) &pixels, (const int*) &stride, 0, encoder->iheight, encoder->videoFrame->data, encoder->videoFrame->linesize);

	int gotData;
#ifdef FFMPEG_USE_PACKETS
	avcodec_send_frame(encoder->video, encoder->videoFrame);
	gotData = avcodec_receive_packet(encoder->video, &packet) == 0;
#else
	avcodec_encode_video2(encoder->video, &packet, encoder->videoFrame, &gotData);
#endif
	if (gotData) {
#ifndef FFMPEG_USE_PACKET_UNREF
		if (encoder->video->coded_frame->key_frame) {
			packet.flags |= AV_PKT_FLAG_KEY;
		}
#endif
		packet.stream_index = encoder->videoStream->index;
		av_interleaved_write_frame(encoder->context, &packet);
	}
#ifdef FFMPEG_USE_PACKET_UNREF
	av_packet_unref(&packet);
#else
	av_free_packet(&packet);
#endif
}

static void _ffmpegSetVideoDimensions(struct mAVStream* stream, unsigned width, unsigned height) {
	struct FFmpegEncoder* encoder = (struct FFmpegEncoder*) stream;
	encoder->iwidth = width;
	encoder->iheight = height;
	if (encoder->scaleContext) {
		sws_freeContext(encoder->scaleContext);
	}
	encoder->scaleContext = sws_getContext(encoder->iwidth, encoder->iheight,
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	    AV_PIX_FMT_RGB565,
#else
	    AV_PIX_FMT_BGR555,
#endif
#else
#ifndef USE_LIBAV
	    AV_PIX_FMT_0BGR32,
#else
	    AV_PIX_FMT_BGR32,
#endif
#endif
	    encoder->videoFrame->width, encoder->videoFrame->height, encoder->video->pix_fmt,
	    SWS_POINT, 0, 0, 0);
}
