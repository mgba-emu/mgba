/* Copyright (c) 2013-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ffmpeg-decoder.h"

#include <libswscale/swscale.h>

void FFmpegDecoderInit(struct FFmpegDecoder* decoder) {
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100)
	av_register_all();
#endif

	memset(decoder, 0, sizeof(*decoder));
	decoder->audioStream = -1;
	decoder->videoStream = -1;
}

bool FFmpegDecoderOpen(struct FFmpegDecoder* decoder, const char* infile) {
	if (FFmpegDecoderIsOpen(decoder)) {
		return false;
	}

	if (avformat_open_input(&decoder->context, infile, NULL, NULL) < 0) {
		return false;
	}

	if (avformat_find_stream_info(decoder->context, NULL) < 0) {
		FFmpegDecoderClose(decoder);
		return false;
	}

	unsigned i;
	for (i = 0; i < decoder->context->nb_streams; ++i) {
#ifdef FFMPEG_USE_CODECPAR
		enum AVMediaType type = decoder->context->streams[i]->codecpar->codec_type;
#else
		enum AVMediaType type = decoder->context->streams[i]->codec->codec_type;
#endif
		struct AVCodec* codec;
		struct AVCodecContext* context = NULL;
		if (type == AVMEDIA_TYPE_VIDEO && decoder->videoStream < 0) {
			decoder->video = avcodec_alloc_context3(NULL);
			if (!decoder->video) {
				FFmpegDecoderClose(decoder);
				return false;
			}
			context = decoder->video;
		}

		if (type == AVMEDIA_TYPE_AUDIO && decoder->audioStream < 0) {
			decoder->audio = avcodec_alloc_context3(NULL);
			if (!decoder->audio) {
				FFmpegDecoderClose(decoder);
				return false;
			}
			context = decoder->audio;
		}
		if (!context) {
			continue;
		}

#ifdef FFMPEG_USE_CODECPAR
		if (avcodec_parameters_to_context(context, decoder->context->streams[i]->codecpar) < 0) {
			FFmpegDecoderClose(decoder);
			return false;
		}
#endif
		codec = avcodec_find_decoder(context->codec_id);
		if (!codec) {
			FFmpegDecoderClose(decoder);
			return false;			
		}
		if (avcodec_open2(context, codec, NULL) < 0) {
			FFmpegDecoderClose(decoder);
			return false;
		}

		if (type == AVMEDIA_TYPE_VIDEO) {
			decoder->videoStream = i;
			decoder->width = context->coded_width;
			decoder->height = context->coded_height;
			if (decoder->out->videoDimensionsChanged) {
				decoder->out->videoDimensionsChanged(decoder->out, decoder->width, decoder->height);
			}
#if LIBAVCODEC_VERSION_MAJOR >= 55
			decoder->videoFrame = av_frame_alloc();
#else
			decoder->videoFrame = avcodec_alloc_frame();
#endif
			decoder->pixels = malloc(decoder->width * decoder->height * BYTES_PER_PIXEL);
		}

		if (type == AVMEDIA_TYPE_AUDIO) {
			decoder->audioStream = i;
#if LIBAVCODEC_VERSION_MAJOR >= 55
			decoder->audioFrame = av_frame_alloc();
#else
			decoder->audioFrame = avcodec_alloc_frame();
#endif
		}
	}
	return true;
}

void FFmpegDecoderClose(struct FFmpegDecoder* decoder) {
	if (decoder->audioFrame) {
#if LIBAVCODEC_VERSION_MAJOR >= 55
		av_frame_free(&decoder->audioFrame);
#else
		avcodec_free_frame(&decoder->audioFrame);
#endif
	}

	if (decoder->audio) {
#ifdef FFMPEG_USE_CODECPAR
		avcodec_free_context(&decoder->audio);
#else
		avcodec_close(decoder->audio);
		decoder->audio = NULL;
#endif
	}

	if (decoder->scaleContext) {
		sws_freeContext(decoder->scaleContext);
		decoder->scaleContext = NULL;
	}

	if (decoder->videoFrame) {
#if LIBAVCODEC_VERSION_MAJOR >= 55
		av_frame_free(&decoder->videoFrame);
#else
		avcodec_free_frame(&decoder->videoFrame);
#endif
	}

	if (decoder->pixels) {
		free(decoder->pixels);
		decoder->pixels = NULL;
	}

	if (decoder->video) {
#ifdef FFMPEG_USE_CODECPAR
		avcodec_free_context(&decoder->video);
#else
		avcodec_close(decoder->video);
		decoder->video = NULL;
#endif
	}

	if (decoder->context) {
		avformat_close_input(&decoder->context);
	}
}

bool FFmpegDecoderIsOpen(struct FFmpegDecoder* decoder) {
	return !!decoder->context;
}

bool FFmpegDecoderRead(struct FFmpegDecoder* decoder) {
	bool readPacket = false;
	while (!readPacket) {
		AVPacket packet;
		if (av_read_frame(decoder->context, &packet) < 0) {
			break;
		}

		readPacket = true;
		if (packet.stream_index == decoder->audioStream) {
			// TODO
		} else if (packet.stream_index == decoder->videoStream) {
#ifdef FFMPEG_USE_CODECPAR
			if (avcodec_send_packet(decoder->video, &packet) < 0) {
				// TODO
			}
			if (avcodec_receive_frame(decoder->video, decoder->videoFrame) < 0) {
				readPacket = false;
			}
#else
			int gotData;
			if (avcodec_decode_video2(decoder->video, decoder->videoFrame, &gotData, &packet) < 0 || !gotData) {
				readPacket = false;
			}
#endif
			if (readPacket && decoder->out->postVideoFrame) {
				if (!decoder->scaleContext) {
					decoder->scaleContext = sws_getContext(decoder->width, decoder->height, decoder->videoFrame->format,
					    decoder->width, decoder->height, AV_PIX_FMT_BGR32,
					    SWS_POINT, 0, 0, 0);
				}
				int stride = decoder->width * BYTES_PER_PIXEL;
				sws_scale(decoder->scaleContext, (const uint8_t* const*) decoder->videoFrame->data, decoder->videoFrame->linesize, 0, decoder->videoFrame->height, &decoder->pixels, &stride);
				decoder->out->postVideoFrame(decoder->out, (const color_t*) decoder->pixels, decoder->width);
			}
		}
#ifdef FFMPEG_USE_PACKET_UNREF
		av_packet_unref(&packet);
#else
		av_free_packet(&packet);
#endif
	}
	return readPacket;
}