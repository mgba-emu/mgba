/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ffmpeg-encoder.h"

#include <mgba/core/core.h>
#include <mgba/gba/interface.h>
#include <mgba/internal/gba/gba.h>
#include <mgba-util/math.h>

#include <libavcodec/version.h>
#include <libavcodec/avcodec.h>
#if LIBAVCODEC_VERSION_MAJOR >= 59
#include <libavcodec/bsf.h>
#endif

#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>

#include <libavutil/version.h>
#if LIBAVUTIL_VERSION_MAJOR >= 53
#include <libavutil/buffer.h>
#endif
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>

#ifdef USE_LIBAVRESAMPLE
#include <libavresample/avresample.h>
#else
#include <libswresample/swresample.h>
#endif
#include <libswscale/swscale.h>

static void _ffmpegPostVideoFrame(struct mAVStream*, const color_t* pixels, size_t stride);
static void _ffmpegPostAudioFrame(struct mAVStream*, int16_t left, int16_t right);
static void _ffmpegSetVideoDimensions(struct mAVStream*, unsigned width, unsigned height);
static void _ffmpegSetAudioRate(struct mAVStream*, unsigned rate);

static bool _ffmpegWriteAudioFrame(struct FFmpegEncoder* encoder, struct AVFrame* audioFrame);
static bool _ffmpegWriteVideoFrame(struct FFmpegEncoder* encoder, struct AVFrame* videoFrame);

static void _ffmpegOpenResampleContext(struct FFmpegEncoder* encoder);

enum {
	PREFERRED_SAMPLE_RATE = 0x10000
};

void FFmpegEncoderInit(struct FFmpegEncoder* encoder) {
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100)
	av_register_all();
#endif

	encoder->d.videoDimensionsChanged = _ffmpegSetVideoDimensions;
	encoder->d.audioRateChanged = _ffmpegSetAudioRate;
	encoder->d.postVideoFrame = _ffmpegPostVideoFrame;
	encoder->d.postAudioFrame = _ffmpegPostAudioFrame;
	encoder->d.postAudioBuffer = NULL;

	encoder->audioCodec = NULL;
	encoder->videoCodec = NULL;
	encoder->containerFormat = NULL;
	FFmpegEncoderSetAudio(encoder, "flac", 0);
	FFmpegEncoderSetVideo(encoder, "libx264", 0, 0);
	FFmpegEncoderSetContainer(encoder, "matroska");
	FFmpegEncoderSetDimensions(encoder, GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
	encoder->iwidth = GBA_VIDEO_HORIZONTAL_PIXELS;
	encoder->iheight = GBA_VIDEO_VERTICAL_PIXELS;
	encoder->isampleRate = PREFERRED_SAMPLE_RATE;
	encoder->frameskip = 1;
	encoder->skipResidue = 0;
	encoder->loop = false;
	encoder->ipixFormat =
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	    AV_PIX_FMT_RGB565;
#else
	    AV_PIX_FMT_BGR555;
#endif
#else
#ifndef USE_LIBAV
	    AV_PIX_FMT_0BGR32;
#else
	    AV_PIX_FMT_BGR32;
#endif
#endif
	encoder->resampleContext = NULL;
	encoder->absf = NULL;
	encoder->context = NULL;
	encoder->scaleContext = NULL;
	encoder->audio = NULL;
	encoder->audioStream = NULL;
	encoder->audioFrame = NULL;
	encoder->audioBuffer = NULL;
	encoder->video = NULL;
	encoder->videoStream = NULL;
	encoder->videoFrame = NULL;
	encoder->graph = NULL;
	encoder->source = NULL;
	encoder->sink = NULL;
	encoder->sinkFrame = NULL;
	FFmpegEncoderSetInputFrameRate(encoder, VIDEO_TOTAL_LENGTH, GBA_ARM7TDMI_FREQUENCY);

	int i;
	for (i = 0; i < FFMPEG_FILTERS_MAX; ++i) {
		encoder->filters[i] = NULL;
	}
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

	const AVCodec* codec = avcodec_find_encoder_by_name(acodec);
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
	encoder->sampleRate = encoder->isampleRate;
	if (codec->supported_samplerates) {
		bool gotSampleRate = false;
		int highestSampleRate = 0;
		for (i = 0; codec->supported_samplerates[i]; ++i) {
			if (codec->supported_samplerates[i] > highestSampleRate) {
				highestSampleRate = codec->supported_samplerates[i];
			}
			if (codec->supported_samplerates[i] < encoder->isampleRate) {
				continue;
			}
			if (!gotSampleRate || encoder->sampleRate > codec->supported_samplerates[i]) {
				encoder->sampleRate = codec->supported_samplerates[i];
				gotSampleRate = true;
			}
		}
		if (!gotSampleRate) {
			// There are no available sample rates that are higher than the input sample rate
			// Let's use the highest available instead
			encoder->sampleRate = highestSampleRate;
		}
	} else if (codec->id == AV_CODEC_ID_FLAC) {
		// HACK: FLAC doesn't support > 65535Hz unless it's divisible by 10
		if (encoder->sampleRate >= 65535) {
			encoder->sampleRate -= encoder->isampleRate % 10;
		}
	} else if (codec->id == AV_CODEC_ID_VORBIS) {
		// HACK: FLAC doesn't support > 48000Hz but doesn't tell us
		if (encoder->sampleRate > 48000) {
			encoder->sampleRate = 48000;
		}
	} else if (codec->id == AV_CODEC_ID_AAC) {
		// HACK: AAC doesn't support 32768Hz (it rounds to 32000), but libfaac doesn't tell us that
		encoder->sampleRate = 48000;
	}
	encoder->audioCodec = acodec;
	encoder->audioBitrate = abr;
	return true;
}

bool FFmpegEncoderSetVideo(struct FFmpegEncoder* encoder, const char* vcodec, int vbr, int frameskip) {
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
		{ AV_PIX_FMT_RGB32, 4},
		{ AV_PIX_FMT_BGR32, 4},
		{ AV_PIX_FMT_YUV444P, 5 },
		{ AV_PIX_FMT_YUV422P, 6 },
		{ AV_PIX_FMT_YUV420P, 7 },
		{ AV_PIX_FMT_PAL8, 8 },
	};

	if (!vcodec) {
		encoder->videoCodec = 0;
		return true;
	}

	const AVCodec* codec = avcodec_find_encoder_by_name(vcodec);
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
	if (vbr < 0 && !av_opt_find((void*) &codec->priv_class, "crf", NULL, 0, 0)) {
		return false;
	}
	encoder->videoCodec = vcodec;
	encoder->videoBitrate = vbr;
	encoder->frameskip = frameskip + 1;
	return true;
}

bool FFmpegEncoderSetContainer(struct FFmpegEncoder* encoder, const char* container) {
	const AVOutputFormat* oformat = av_guess_format(container, 0, 0);
	if (!oformat) {
		return false;
	}
	encoder->containerFormat = container;
	return true;
}

void FFmpegEncoderSetDimensions(struct FFmpegEncoder* encoder, int width, int height) {
	encoder->width = width > 0 ? width : GBA_VIDEO_HORIZONTAL_PIXELS;
	encoder->height = height > 0 ? height : GBA_VIDEO_VERTICAL_PIXELS;
}

void FFmpegEncoderSetLooping(struct FFmpegEncoder* encoder, bool loop) {
	encoder->loop = loop;
}

bool FFmpegEncoderVerifyContainer(struct FFmpegEncoder* encoder) {
	const AVOutputFormat* oformat = av_guess_format(encoder->containerFormat, 0, 0);
	const AVCodec* acodec = avcodec_find_encoder_by_name(encoder->audioCodec);
	const AVCodec* vcodec = avcodec_find_encoder_by_name(encoder->videoCodec);
	if ((encoder->audioCodec && !acodec) || (encoder->videoCodec && !vcodec) || !oformat || (!acodec && !vcodec)) {
		return false;
	}
	if (encoder->audioCodec && !avformat_query_codec(oformat, acodec->id, FF_COMPLIANCE_EXPERIMENTAL)) {
		return false;
	}
	if (encoder->videoCodec && !avformat_query_codec(oformat, vcodec->id, FF_COMPLIANCE_EXPERIMENTAL)) {
		return false;
	}
	return true;
}

bool FFmpegEncoderOpen(struct FFmpegEncoder* encoder, const char* outfile) {
	const AVCodec* acodec = avcodec_find_encoder_by_name(encoder->audioCodec);
	const AVCodec* vcodec = avcodec_find_encoder_by_name(encoder->videoCodec);
	if ((encoder->audioCodec && !acodec) || (encoder->videoCodec && !vcodec) || !FFmpegEncoderVerifyContainer(encoder)) {
		return false;
	}

	if (encoder->context) {
		return false;
	}

	encoder->currentAudioSample = 0;
	encoder->currentAudioFrame = 0;
	encoder->currentVideoFrame = 0;
	encoder->skipResidue = 0;

	const AVOutputFormat* oformat = av_guess_format(encoder->containerFormat, 0, 0);
#ifndef USE_LIBAV
	avformat_alloc_output_context2(&encoder->context, (AVOutputFormat*) oformat, 0, outfile);
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
		int res = avcodec_open2(encoder->audio, acodec, &opts);
		av_dict_free(&opts);
		if (res < 0) {
			FFmpegEncoderClose(encoder);
			return false;
		}
		encoder->audioFrame = av_frame_alloc();
		if (!encoder->audio->frame_size) {
			encoder->audio->frame_size = 1;
		}
		encoder->audioFrame->nb_samples = encoder->audio->frame_size;
		encoder->audioFrame->format = encoder->audio->sample_fmt;
		encoder->audioFrame->pts = 0;
		encoder->audioFrame->channel_layout = AV_CH_LAYOUT_STEREO;
		_ffmpegOpenResampleContext(encoder);
		av_frame_get_buffer(encoder->audioFrame, 0);

		if (encoder->audio->codec->id == AV_CODEC_ID_AAC &&
		    (strcasecmp(encoder->containerFormat, "mp4") == 0||
		        strcasecmp(encoder->containerFormat, "m4v") == 0 ||
		        strcasecmp(encoder->containerFormat, "mov") == 0)) {
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

	if (vcodec) {
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
		encoder->video->time_base = (AVRational) { encoder->frameCycles * encoder->frameskip, encoder->cycles };
		encoder->video->framerate = (AVRational) { encoder->cycles, encoder->frameCycles * encoder->frameskip };
		encoder->videoStream->time_base = encoder->video->time_base;
		encoder->videoStream->avg_frame_rate = encoder->video->framerate;
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

		if (encoder->video->codec->id == AV_CODEC_ID_H264 &&
		    (strcasecmp(encoder->containerFormat, "mp4") == 0 ||
		        strcasecmp(encoder->containerFormat, "m4v") == 0 ||
		        strcasecmp(encoder->containerFormat, "mov") == 0)) {
			// QuickTime and a few other things require YUV420
			encoder->video->pix_fmt = AV_PIX_FMT_YUV420P;
		}
		if (encoder->video->codec->id == AV_CODEC_ID_FFV1) {
#if LIBAVCODEC_VERSION_MAJOR >= 57
			av_opt_set(encoder->video->priv_data, "coder", "range_tab", 0);
			av_opt_set_int(encoder->video->priv_data, "context", 1, 0);
#endif
			encoder->video->gop_size = 128;
			encoder->video->level = 3;
		}

		if (encoder->video->codec->id == AV_CODEC_ID_PNG) {
			encoder->video->compression_level = 8;
		}
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(58, 48, 100)
		if (encoder->video->codec->id == AV_CODEC_ID_ZMBV) {
			encoder->video->compression_level = 5;
			encoder->video->pix_fmt = AV_PIX_FMT_BGR0;
		}
#endif
		if (strcmp(vcodec->name, "libx264") == 0 || strcmp(vcodec->name, "libx264rgb") == 0) {
			// Try to adaptively figure out when you can use a slower encoder
			if (encoder->width * encoder->height > 1000000) {
				av_opt_set(encoder->video->priv_data, "preset", "superfast", 0);
			} else if (encoder->width * encoder->height > 500000) {
				av_opt_set(encoder->video->priv_data, "preset", "veryfast", 0);
			} else {
				av_opt_set(encoder->video->priv_data, "preset", "faster", 0);
			}
			av_opt_set(encoder->video->priv_data, "tune", "zerolatency", 0);
			if (encoder->videoBitrate == 0) {
				av_opt_set(encoder->video->priv_data, "qp", "0", 0);
				if (strcmp(vcodec->name, "libx264") == 0) {
					encoder->video->pix_fmt = AV_PIX_FMT_YUV444P;
				}
			} else if (encoder->videoBitrate < 0) {
				av_opt_set_int(encoder->video->priv_data, "crf", -encoder->videoBitrate, 0);
			}
		} else if (encoder->videoBitrate < 0) {
			if (strcmp(vcodec->name, "libvpx") == 0 || strcmp(vcodec->name, "libvpx-vp9") == 0 || strcmp(vcodec->name, "libx265") == 0) {
				av_opt_set_int(encoder->video->priv_data, "crf", -encoder->videoBitrate, 0);
			} else {
				FFmpegEncoderClose(encoder);
				return false;
			}
		}
		if (strncmp(vcodec->name, "libvpx", 6) == 0) {
			av_opt_set_int(encoder->video->priv_data, "cpu-used", 2, 0);
			av_opt_set(encoder->video->priv_data, "deadline", "realtime", 0);
		}
		if (strcmp(vcodec->name, "libvpx-vp9") == 0 && encoder->videoBitrate == 0) {
			av_opt_set_int(encoder->video->priv_data, "lossless", 1, 0);
			av_opt_set_int(encoder->video->priv_data, "crf", 0, 0);
			encoder->video->gop_size = 120;
			encoder->video->pix_fmt = AV_PIX_FMT_GBRP;
		}
		if (strcmp(vcodec->name, "libwebp_anim") == 0 && encoder->videoBitrate == 0) {
			av_opt_set(encoder->video->priv_data, "lossless", "1", 0);
			encoder->video->pix_fmt = AV_PIX_FMT_RGB32;
		}

		if (encoder->pixFormat == AV_PIX_FMT_PAL8) {
			encoder->graph = avfilter_graph_alloc();

			const struct AVFilter* source = avfilter_get_by_name("buffer");
			const struct AVFilter* sink = avfilter_get_by_name("buffersink");
			const struct AVFilter* split = avfilter_get_by_name("split");
			const struct AVFilter* palettegen = avfilter_get_by_name("palettegen");
			const struct AVFilter* paletteuse = avfilter_get_by_name("paletteuse");

			if (!source || !sink || !split || !palettegen || !paletteuse || !encoder->graph) {
				FFmpegEncoderClose(encoder);
				return false;
			}

			char args[256];
			snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d",
			         encoder->video->width, encoder->video->height, encoder->ipixFormat,
			         encoder->video->time_base.num, encoder->video->time_base.den);

			int res = 0;
			res |= avfilter_graph_create_filter(&encoder->source, source, NULL, args, NULL, encoder->graph);
			res |= avfilter_graph_create_filter(&encoder->sink, sink, NULL, NULL, NULL, encoder->graph);
			res |= avfilter_graph_create_filter(&encoder->filters[0], split, NULL, NULL, NULL, encoder->graph);
			res |= avfilter_graph_create_filter(&encoder->filters[1], palettegen, NULL, "reserve_transparent=off", NULL, encoder->graph);
			res |= avfilter_graph_create_filter(&encoder->filters[2], paletteuse, NULL, "dither=none", NULL, encoder->graph);
			if (res < 0) {
				FFmpegEncoderClose(encoder);
				return false;
			}

			res = 0;
			res |= avfilter_link(encoder->source, 0, encoder->filters[0], 0);
			res |= avfilter_link(encoder->filters[0], 0, encoder->filters[1], 0);
			res |= avfilter_link(encoder->filters[0], 1, encoder->filters[2], 0);
			res |= avfilter_link(encoder->filters[1], 0, encoder->filters[2], 1);
			res |= avfilter_link(encoder->filters[2], 0, encoder->sink, 0);
			if (res < 0 || avfilter_graph_config(encoder->graph, NULL) < 0) {
				FFmpegEncoderClose(encoder);
				return false;
			}

			encoder->sinkFrame = av_frame_alloc();
		}
		AVDictionary* opts = 0;
		av_dict_set(&opts, "strict", "-2", 0);
		int res = avcodec_open2(encoder->video, vcodec, &opts);
		av_dict_free(&opts);
		if (res < 0) {
			FFmpegEncoderClose(encoder);
			return false;
		}
		encoder->videoFrame = av_frame_alloc();
		encoder->videoFrame->format = encoder->video->pix_fmt != AV_PIX_FMT_PAL8 ? encoder->video->pix_fmt : encoder->ipixFormat;
		encoder->videoFrame->width = encoder->video->width;
		encoder->videoFrame->height = encoder->video->height;
		encoder->videoFrame->pts = 0;
		_ffmpegSetVideoDimensions(&encoder->d, encoder->iwidth, encoder->iheight);
		av_frame_get_buffer(encoder->videoFrame, 32);
#ifdef FFMPEG_USE_CODECPAR
		avcodec_parameters_from_context(encoder->videoStream->codecpar, encoder->video);
#endif
	}

	if (strcmp(encoder->containerFormat, "gif") == 0) {
		av_opt_set(encoder->context->priv_data, "loop", encoder->loop ? "0" : "-1", 0);
	} else if (strcmp(encoder->containerFormat, "apng") == 0) {
		av_opt_set(encoder->context->priv_data, "plays", encoder->loop ? "0" : "1", 0);
	} else if (strcmp(encoder->containerFormat, "webp") == 0) {
		av_opt_set(encoder->context->priv_data, "loop", encoder->loop ? "0" : "1", 0);
	}

	AVDictionary* opts = 0;
	av_dict_set(&opts, "strict", "-2", 0);
	bool res = avio_open(&encoder->context->pb, outfile, AVIO_FLAG_WRITE) < 0 || avformat_write_header(encoder->context, &opts) < 0;
	av_dict_free(&opts);
	if (res) {
		FFmpegEncoderClose(encoder);
		return false;
	}
	return true;
}

void FFmpegEncoderClose(struct FFmpegEncoder* encoder) {
	if (encoder->audio) {
		while (true) {
			if (!_ffmpegWriteAudioFrame(encoder, NULL)) {
				break;
			}
		}
	}
	if (encoder->video) {
		if (encoder->graph) {
			if (av_buffersrc_add_frame(encoder->source, NULL) >= 0) {
				while (true) {
					int res = av_buffersink_get_frame(encoder->sink, encoder->sinkFrame);
					if (res < 0) {
						break;
					}
					_ffmpegWriteVideoFrame(encoder, encoder->sinkFrame);
					av_frame_unref(encoder->sinkFrame);
				}
			}
		}
		while (true) {
			if (!_ffmpegWriteVideoFrame(encoder, NULL)) {
				break;
			}
		}
	}

	if (encoder->context && encoder->context->pb) {
		av_write_trailer(encoder->context);
		avio_close(encoder->context->pb);
	}

	if (encoder->audioBuffer) {
		av_free(encoder->audioBuffer);
		encoder->audioBuffer = NULL;
	}

	if (encoder->audioFrame) {
		av_frame_free(&encoder->audioFrame);
	}
	if (encoder->audio) {
#ifdef FFMPEG_USE_CODECPAR
		avcodec_free_context(&encoder->audio);
#else
		avcodec_close(encoder->audio);
		encoder->audio = NULL;
#endif
	}

	if (encoder->resampleContext) {
#ifdef USE_LIBAVRESAMPLE
		avresample_close(encoder->resampleContext);
		encoder->resampleContext = NULL;
#else
		swr_free(&encoder->resampleContext);
#endif
	}

	if (encoder->absf) {
#ifdef FFMPEG_USE_NEW_BSF
		av_bsf_free(&encoder->absf);
#else
		av_bitstream_filter_close(encoder->absf);
		encoder->absf = NULL;
#endif
	}

	if (encoder->videoFrame) {
		av_frame_free(&encoder->videoFrame);
	}

	if (encoder->sinkFrame) {
		av_frame_free(&encoder->sinkFrame);
		encoder->sinkFrame = NULL;
	}

	if (encoder->video) {
#ifdef FFMPEG_USE_CODECPAR
		avcodec_free_context(&encoder->video);
#else
		avcodec_close(encoder->video);
		encoder->video = NULL;
#endif
	}

	if (encoder->scaleContext) {
		sws_freeContext(encoder->scaleContext);
		encoder->scaleContext = NULL;
	}

	if (encoder->graph) {
		avfilter_graph_free(&encoder->graph);
		encoder->graph = NULL;
		encoder->source = NULL;
		encoder->sink = NULL;

		int i;
		for (i = 0; i < FFMPEG_FILTERS_MAX; ++i) {
			encoder->filters[i] = NULL;
		}
	}

	if (encoder->context) {
		avformat_free_context(encoder->context);
		encoder->context = NULL;
	}
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

	encoder->currentAudioSample = 0;
#ifdef USE_LIBAVRESAMPLE
	avresample_convert(encoder->resampleContext, 0, 0, 0,
	                   (uint8_t**) &encoder->audioBuffer, 0, encoder->audioBufferSize / 4);

	if (avresample_available(encoder->resampleContext) < encoder->audioFrame->nb_samples) {
		return;
	}
	av_frame_make_writable(encoder->audioFrame);
	int samples = avresample_read(encoder->resampleContext, encoder->audioFrame->data, encoder->audioFrame->nb_samples);
#else
	av_frame_make_writable(encoder->audioFrame);
	if (swr_get_out_samples(encoder->resampleContext, 1) < encoder->audioFrame->nb_samples) {
		swr_convert(encoder->resampleContext, NULL, 0, (const uint8_t**) &encoder->audioBuffer, encoder->audioBufferSize / 4);
		return;
	}
	int samples = swr_convert(encoder->resampleContext, encoder->audioFrame->data, encoder->audioFrame->nb_samples,
	                          (const uint8_t**) &encoder->audioBuffer, encoder->audioBufferSize / 4);
#endif

	encoder->audioFrame->pts = encoder->currentAudioFrame;
	encoder->currentAudioFrame += samples;

	_ffmpegWriteAudioFrame(encoder, encoder->audioFrame);
}

bool _ffmpegWriteAudioFrame(struct FFmpegEncoder* encoder, struct AVFrame* audioFrame) {
	AVPacket* packet;
#ifdef FFMPEG_USE_PACKET_UNREF
	packet = av_packet_alloc();
#else
	packet = av_malloc(sizeof(*packet));
	av_init_packet(packet);
#endif
	packet->data = 0;
	packet->size = 0;

	int gotData;
#ifdef FFMPEG_USE_PACKETS
	avcodec_send_frame(encoder->audio, audioFrame);
	gotData = avcodec_receive_packet(encoder->audio, packet);
	gotData = (gotData == 0) && packet->size;
#else
	avcodec_encode_audio2(encoder->audio, packet, audioFrame, &gotData);
#endif
	packet->pts = av_rescale_q(packet->pts, encoder->audio->time_base, encoder->audioStream->time_base);
	packet->dts = packet->pts;

	if (gotData) {
		if (encoder->absf) {
			AVPacket* tempPacket;
#ifdef FFMPEG_USE_PACKETS
			tempPacket = av_packet_alloc();
#else
			tempPacket = av_malloc(sizeof(*tempPacket));
			av_init_packet(tempPacket);
#endif

#ifdef FFMPEG_USE_NEW_BSF
			int success = av_bsf_send_packet(encoder->absf, packet);
			if (success >= 0) {
				success = av_bsf_receive_packet(encoder->absf, tempPacket);
			}
#else
			int success = av_bitstream_filter_filter(encoder->absf, encoder->audio, 0,
			    &tempPacket->data, &tempPacket->size,
			    packet->data, packet->size, 0);
#endif

			if (success >= 0) {
#if LIBAVUTIL_VERSION_MAJOR >= 53
				tempPacket->buf = av_buffer_create(tempPacket->data, tempPacket->size, av_buffer_default_free, 0, 0);
#endif

#ifdef FFMPEG_USE_PACKET_UNREF
				av_packet_move_ref(packet, tempPacket);
				av_packet_free(&tempPacket);
#else
				av_free_packet(packet);
				av_freep(&packet);
				packet = tempPacket;
#endif

				packet->stream_index = encoder->audioStream->index;
				av_interleaved_write_frame(encoder->context, packet);
			}
		} else {
			packet->stream_index = encoder->audioStream->index;
			av_interleaved_write_frame(encoder->context, packet);
		}
	}
#ifdef FFMPEG_USE_PACKET_UNREF
	av_packet_unref(packet);
	av_packet_free(&packet);
#else
	av_free_packet(packet);
	av_freep(&packet);
#endif
	return gotData;
}

void _ffmpegPostVideoFrame(struct mAVStream* stream, const color_t* pixels, size_t stride) {
	struct FFmpegEncoder* encoder = (struct FFmpegEncoder*) stream;
	if (!encoder->context || !encoder->videoCodec) {
		return;
	}
	encoder->skipResidue = (encoder->skipResidue + 1) % encoder->frameskip;
	if (encoder->skipResidue) {
		return;
	}
	stride *= BYTES_PER_PIXEL;

	av_frame_make_writable(encoder->videoFrame);
	if (encoder->video->codec->id == AV_CODEC_ID_WEBP) {
		// TODO: Figure out why WebP is rescaling internally (should video frames not be rescaled externally?)
		encoder->videoFrame->pts = encoder->currentVideoFrame;
	} else {
		encoder->videoFrame->pts = av_rescale_q(encoder->currentVideoFrame, encoder->video->time_base, encoder->videoStream->time_base);
	}
	++encoder->currentVideoFrame;

	sws_scale(encoder->scaleContext, (const uint8_t* const*) &pixels, (const int*) &stride, 0, encoder->iheight, encoder->videoFrame->data, encoder->videoFrame->linesize);

	if (encoder->graph) {
		if (av_buffersrc_write_frame(encoder->source, encoder->videoFrame) < 0) {
			return;
		}
		while (true) {
			int res = av_buffersink_get_frame(encoder->sink, encoder->sinkFrame);
			if (res < 0) {
				break;
			}
			_ffmpegWriteVideoFrame(encoder, encoder->sinkFrame);
			av_frame_unref(encoder->sinkFrame);
		}
	} else {
		_ffmpegWriteVideoFrame(encoder, encoder->videoFrame);
	}
}

bool _ffmpegWriteVideoFrame(struct FFmpegEncoder* encoder, struct AVFrame* videoFrame) {
	AVPacket* packet;

#ifdef FFMPEG_USE_PACKET_UNREF
	packet = av_packet_alloc();
#else
	packet = av_malloc(sizeof(*packet));
	av_init_packet(packet);
#endif
	packet->data = 0;
	packet->size = 0;

	int gotData;
#ifdef FFMPEG_USE_PACKETS
	avcodec_send_frame(encoder->video, videoFrame);
	gotData = avcodec_receive_packet(encoder->video, packet) == 0;
#else
	avcodec_encode_video2(encoder->video, packet, videoFrame, &gotData);
#endif
	if (gotData) {
#ifndef FFMPEG_USE_PACKET_UNREF
		if (encoder->video->coded_frame->key_frame) {
			packet->flags |= AV_PKT_FLAG_KEY;
		}
#endif
		packet->stream_index = encoder->videoStream->index;
		av_interleaved_write_frame(encoder->context, packet);
	}
#ifdef FFMPEG_USE_PACKET_UNREF
	av_packet_unref(packet);
	av_packet_free(&packet);
#else
	av_free_packet(packet);
	av_freep(&packet);
#endif

	return gotData;
}

static void _ffmpegSetVideoDimensions(struct mAVStream* stream, unsigned width, unsigned height) {
	struct FFmpegEncoder* encoder = (struct FFmpegEncoder*) stream;
	if (!encoder->context || !encoder->videoCodec) {
		return;
	}
	encoder->iwidth = width;
	encoder->iheight = height;
	if (encoder->scaleContext) {
		sws_freeContext(encoder->scaleContext);
	}
	encoder->scaleContext = sws_getContext(encoder->iwidth, encoder->iheight, encoder->ipixFormat,
	    encoder->videoFrame->width, encoder->videoFrame->height, encoder->videoFrame->format,
	    SWS_POINT, 0, 0, 0);
}

static void _ffmpegSetAudioRate(struct mAVStream* stream, unsigned rate) {
	struct FFmpegEncoder* encoder = (struct FFmpegEncoder*) stream;
	FFmpegEncoderSetInputSampleRate(encoder, rate);
}

void FFmpegEncoderSetInputFrameRate(struct FFmpegEncoder* encoder, int numerator, int denominator) {
	reduceFraction(&numerator, &denominator);
	encoder->frameCycles = numerator;
	encoder->cycles = denominator;
	if (encoder->video) {
		encoder->video->framerate = (AVRational) { denominator, numerator * encoder->frameskip };
	}
}

void FFmpegEncoderSetInputSampleRate(struct FFmpegEncoder* encoder, int sampleRate) {
	encoder->isampleRate = sampleRate;
	if (encoder->resampleContext) {	
		av_freep(&encoder->audioBuffer);
#ifdef USE_LIBAVRESAMPLE
		avresample_close(encoder->resampleContext);
#else
		swr_free(&encoder->resampleContext);
#endif
		_ffmpegOpenResampleContext(encoder);
	}
}

void _ffmpegOpenResampleContext(struct FFmpegEncoder* encoder) {
	encoder->audioBufferSize = av_rescale_q(encoder->audioFrame->nb_samples, (AVRational) { 1, encoder->sampleRate }, (AVRational) { 1, encoder->isampleRate }) * 4;
	encoder->audioBuffer = av_malloc(encoder->audioBufferSize);
#ifdef USE_LIBAVRESAMPLE
	encoder->resampleContext = avresample_alloc_context();
	av_opt_set_int(encoder->resampleContext, "in_channel_layout", AV_CH_LAYOUT_STEREO, 0);
	av_opt_set_int(encoder->resampleContext, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
	av_opt_set_int(encoder->resampleContext, "in_sample_rate", encoder->isampleRate, 0);
	av_opt_set_int(encoder->resampleContext, "out_sample_rate", encoder->sampleRate, 0);
	av_opt_set_int(encoder->resampleContext, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
	av_opt_set_int(encoder->resampleContext, "out_sample_fmt", encoder->sampleFormat, 0);
	avresample_open(encoder->resampleContext);
#else
	encoder->resampleContext = swr_alloc_set_opts(NULL, AV_CH_LAYOUT_STEREO, encoder->sampleFormat, encoder->sampleRate,
	                                              AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, encoder->isampleRate, 0, NULL);
	swr_init(encoder->resampleContext);
#endif
}
