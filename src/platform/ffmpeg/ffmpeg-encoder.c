#include "ffmpeg-encoder.h"

#include "gba-video.h"

#include <libavutil/imgutils.h>
#include <libavutil/opt.h>

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
	encoder->currentAudioSample = 0;
	encoder->currentAudioFrame = 0;
	encoder->currentVideoFrame = 0;
	encoder->context = 0;
}

bool FFmpegEncoderSetAudio(struct FFmpegEncoder* encoder, const char* acodec, unsigned abr) {
	AVCodec* codec = avcodec_find_encoder_by_name(acodec);
	if (!codec) {
		return false;
	}

	if (!codec->sample_fmts) {
		return false;
	}
	size_t i;
	encoder->sampleFormat = AV_SAMPLE_FMT_NONE;
	for (i = 0; codec->sample_fmts[i] != AV_SAMPLE_FMT_NONE; ++i) {
		if (codec->sample_fmts[i] == AV_SAMPLE_FMT_S16 || codec->sample_fmts[i] == AV_SAMPLE_FMT_S16P) {
			encoder->sampleFormat = codec->sample_fmts[i];
			break;
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
	static struct {
		enum AVPixelFormat format;
		int priority;
	} priorities[] = {
		{ AV_PIX_FMT_RGB24, 0 },
		{ AV_PIX_FMT_BGR0, 1 },
		{ AV_PIX_FMT_YUV422P, 2 },
		{ AV_PIX_FMT_YUV444P, 3 },
		{ AV_PIX_FMT_YUV420P, 4 }
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

bool FFmpegEncoderVerifyContainer(struct FFmpegEncoder* encoder) {
	AVOutputFormat* oformat = av_guess_format(encoder->containerFormat, 0, 0);
	AVCodec* acodec = avcodec_find_encoder_by_name(encoder->audioCodec);
	AVCodec* vcodec = avcodec_find_encoder_by_name(encoder->videoCodec);
	if (!acodec || !vcodec || !oformat) {
		return false;
	}
	if (!avformat_query_codec(oformat, acodec->id, FF_COMPLIANCE_EXPERIMENTAL)) {
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
	if (!acodec || !vcodec || !FFmpegEncoderVerifyContainer(encoder)) {
		return false;
	}

	avformat_alloc_output_context2(&encoder->context, 0, 0, outfile);

	encoder->context->oformat = av_guess_format(encoder->containerFormat, 0, 0);

	encoder->audioStream = avformat_new_stream(encoder->context, acodec);
	encoder->audio = encoder->audioStream->codec;
	encoder->audio->bit_rate = encoder->audioBitrate;
	encoder->audio->channels = 2;
	encoder->audio->channel_layout = AV_CH_LAYOUT_STEREO;
	encoder->audio->sample_rate = encoder->sampleRate;
	encoder->audio->sample_fmt = encoder->sampleFormat;
	avcodec_open2(encoder->audio, acodec, 0);
	encoder->audioFrame = av_frame_alloc();
	encoder->audioFrame->nb_samples = encoder->audio->frame_size;
	encoder->audioFrame->format = encoder->audio->sample_fmt;
	encoder->audioFrame->pts = 0;
	if (encoder->sampleRate != PREFERRED_SAMPLE_RATE) {
		encoder->resampleContext = avresample_alloc_context();
		av_opt_set_int(encoder->resampleContext, "in_channel_layout", AV_CH_LAYOUT_STEREO, 0);
		av_opt_set_int(encoder->resampleContext, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
		av_opt_set_int(encoder->resampleContext, "in_sample_rate", PREFERRED_SAMPLE_RATE, 0);
		av_opt_set_int(encoder->resampleContext, "out_sample_rate", encoder->sampleRate, 0);
		av_opt_set_int(encoder->resampleContext, "in_sample_fmt", encoder->sampleFormat, 0);
		av_opt_set_int(encoder->resampleContext, "out_sample_fmt", encoder->sampleFormat, 0);
		avresample_open(encoder->resampleContext);
		encoder->audioBufferSize = (encoder->audioFrame->nb_samples * PREFERRED_SAMPLE_RATE / encoder->sampleRate) * 4;
		encoder->audioBuffer = av_malloc(encoder->audioBufferSize);
	} else {
		encoder->resampleContext = 0;
		encoder->audioBufferSize = 0;
		encoder->audioBuffer = 0;
	}
	encoder->postaudioBufferSize = av_samples_get_buffer_size(0, encoder->audio->channels, encoder->audio->frame_size, encoder->audio->sample_fmt, 0);
	encoder->postaudioBuffer = av_malloc(encoder->postaudioBufferSize);
	avcodec_fill_audio_frame(encoder->audioFrame, encoder->audio->channels, encoder->audio->sample_fmt, (const uint8_t*) encoder->postaudioBuffer, encoder->postaudioBufferSize, 0);

	encoder->videoStream = avformat_new_stream(encoder->context, vcodec);
	encoder->video = encoder->videoStream->codec;
	encoder->video->bit_rate = encoder->videoBitrate;
	encoder->video->width = VIDEO_HORIZONTAL_PIXELS;
	encoder->video->height = VIDEO_VERTICAL_PIXELS;
	encoder->video->time_base = (AVRational) { VIDEO_TOTAL_LENGTH, GBA_ARM7TDMI_FREQUENCY };
	encoder->video->pix_fmt = encoder->pixFormat;
	encoder->video->gop_size = 15;
	encoder->video->max_b_frames = 0;
	avcodec_open2(encoder->video, vcodec, 0);
	encoder->videoFrame = av_frame_alloc();
	encoder->videoFrame->format = encoder->video->pix_fmt;
	encoder->videoFrame->width = encoder->video->width;
	encoder->videoFrame->height = encoder->video->height;
	encoder->videoFrame->pts = 0;
	av_image_alloc(encoder->videoFrame->data, encoder->videoFrame->linesize, encoder->video->width, encoder->video->height, encoder->video->pix_fmt, 32);

	if (encoder->context->oformat->flags & AVFMT_GLOBALHEADER) {
		encoder->audio->flags |= CODEC_FLAG_GLOBAL_HEADER;
		encoder->video->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

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

	av_free(encoder->postaudioBuffer);
	if (encoder->audioBuffer) {
		av_free(encoder->audioBuffer);
	}
	av_frame_free(&encoder->audioFrame);
	avcodec_close(encoder->audio);

	av_frame_free(&encoder->videoFrame);
	avcodec_close(encoder->video);

	if (encoder->resampleContext) {
		avresample_close(encoder->resampleContext);
	}

	avformat_free_context(encoder->context);
	encoder->context = 0;

	encoder->currentAudioSample = 0;
	encoder->currentAudioFrame = 0;
	encoder->currentVideoFrame = 0;
}

void _ffmpegPostAudioFrame(struct GBAAVStream* stream, int32_t left, int32_t right) {
	struct FFmpegEncoder* encoder = (struct FFmpegEncoder*) stream;
	if (!encoder->context) {
		return;
	}

	av_frame_make_writable(encoder->audioFrame);
	uint16_t* buffers[2];
	int stride;
	bool planar = av_sample_fmt_is_planar(encoder->audio->sample_fmt);
	if (encoder->resampleContext) {
		buffers[0] = (uint16_t*) encoder->audioBuffer;
		if (planar) {
			stride = 1;
			buffers[1] = &buffers[0][encoder->audioBufferSize / 4];
		} else {
			stride = 2;
			buffers[1] = &buffers[0][1];
		}
	} else {
		buffers[0] = (uint16_t*) encoder->postaudioBuffer;
		if (planar) {
			stride = 1;
			buffers[1] = &buffers[0][encoder->postaudioBufferSize / 4];
		} else {
			stride = 2;
			buffers[1] = &buffers[0][1];
		}
	}
	buffers[0][encoder->currentAudioSample * stride] = left;
	buffers[1][encoder->currentAudioSample * stride] = right;

	++encoder->currentAudioFrame;
	++encoder->currentAudioSample;

	if (encoder->resampleContext) {
		if ((encoder->currentAudioSample * 4) < encoder->audioBufferSize) {
			return;
		}
		encoder->currentAudioSample = 0;

		avresample_convert(encoder->resampleContext,
			0, 0, encoder->postaudioBufferSize / 4,
			(uint8_t**) buffers, 0, encoder->audioBufferSize / 4);
		if ((ssize_t) avresample_available(encoder->resampleContext) < (ssize_t) encoder->postaudioBufferSize / 4) {
			return;
		}
		avresample_read(encoder->resampleContext, encoder->audioFrame->data, encoder->postaudioBufferSize / 4);
	} else {
		if ((encoder->currentAudioSample * 4) < encoder->postaudioBufferSize) {
			return;
		}
		encoder->currentAudioSample = 0;
	}

	AVRational timeBase = { 1, PREFERRED_SAMPLE_RATE };
	encoder->audioFrame->pts = av_rescale_q(encoder->currentAudioFrame, timeBase, encoder->audioStream->time_base);

	AVPacket packet;
	av_init_packet(&packet);
	packet.data = 0;
	packet.size = 0;
	int gotData;
	avcodec_encode_audio2(encoder->audio, &packet, encoder->audioFrame, &gotData);
	if (gotData) {
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
	uint32_t* pixels;
	unsigned stride;
	renderer->getPixels(renderer, &stride, (void**) &pixels);

	AVPacket packet;

	av_init_packet(&packet);
	packet.data = 0;
	packet.size = 0;
	av_frame_make_writable(encoder->videoFrame);
	encoder->videoFrame->pts = av_rescale_q(encoder->currentVideoFrame, encoder->video->time_base, encoder->videoStream->time_base);
	++encoder->currentVideoFrame;

	unsigned x, y;
	if (encoder->videoFrame->format == AV_PIX_FMT_BGR0) {
		for (y = 0; y < VIDEO_VERTICAL_PIXELS; ++y) {
			for (x = 0; x < VIDEO_HORIZONTAL_PIXELS; ++x) {
				uint32_t pixel = pixels[stride * y + x];
				encoder->videoFrame->data[0][y * encoder->videoFrame->linesize[0] + x * 4] = pixel >> 16;
				encoder->videoFrame->data[0][y * encoder->videoFrame->linesize[0] + x * 4 + 1] = pixel >> 8;
				encoder->videoFrame->data[0][y * encoder->videoFrame->linesize[0] + x * 4 + 2] = pixel;
			}
		}
	} else if (encoder->videoFrame->format == AV_PIX_FMT_RGB24) {
		for (y = 0; y < VIDEO_VERTICAL_PIXELS; ++y) {
			for (x = 0; x < VIDEO_HORIZONTAL_PIXELS; ++x) {
				uint32_t pixel = pixels[stride * y + x];
				encoder->videoFrame->data[0][y * encoder->videoFrame->linesize[0] + x * 3] = pixel;
				encoder->videoFrame->data[0][y * encoder->videoFrame->linesize[0] + x * 3 + 1] = pixel >> 8;
				encoder->videoFrame->data[0][y * encoder->videoFrame->linesize[0] + x * 3 + 2] = pixel >> 16;
			}
		}
	}

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
