#include "ffmpeg-encoder.h"

#include "gba-video.h"

#include <libavutil/imgutils.h>

static void _ffmpegPostVideoFrame(struct GBAAVStream*, struct GBAVideoRenderer* renderer);
static void _ffmpegPostAudioFrame(struct GBAAVStream*, int32_t left, int32_t right);

void FFmpegEncoderInit(struct FFmpegEncoder* encoder) {
	av_register_all();

	encoder->d.postVideoFrame = _ffmpegPostVideoFrame;
	encoder->d.postAudioFrame = _ffmpegPostAudioFrame;

	FFmpegEncoderSetAudio(encoder, "flac", 0);
	FFmpegEncoderSetVideo(encoder, "png", 0);
	encoder->currentAudioSample = 0;
	encoder->currentAudioFrame = 0;
	encoder->currentVideoFrame = 0;
}

bool FFmpegEncoderSetAudio(struct FFmpegEncoder* encoder, const char* acodec, unsigned abr) {
	if (!avcodec_find_encoder_by_name(acodec)) {
		return false;
	}
	encoder->audioCodec = acodec;
	encoder->audioBitrate = abr;
	return true;
}

bool FFmpegEncoderSetVideo(struct FFmpegEncoder* encoder, const char* vcodec, unsigned vbr) {
	AVCodec* codec = avcodec_find_encoder_by_name(vcodec);
	if (!codec) {
		return false;
	}

	size_t i;
	encoder->pixFormat = AV_PIX_FMT_NONE;
	for (i = 0; codec->pix_fmts[i] != AV_PIX_FMT_NONE; ++i) {
		if (codec->pix_fmts[i] == AV_PIX_FMT_RGB24) {
			encoder->pixFormat = AV_PIX_FMT_RGB24;
			break;
		}
		if (codec->pix_fmts[i] == AV_PIX_FMT_BGR0) {
			encoder->pixFormat = AV_PIX_FMT_BGR0;
		}
	}
	if (encoder->pixFormat == AV_PIX_FMT_NONE) {
		return false;
	}
	encoder->videoCodec = vcodec;
	encoder->videoBitrate = vbr;
	return true;
}

bool FFmpegEncoderOpen(struct FFmpegEncoder* encoder, const char* outfile) {
	AVCodec* acodec = avcodec_find_encoder_by_name(encoder->audioCodec);
	AVCodec* vcodec = avcodec_find_encoder_by_name(encoder->videoCodec);
	if (!acodec || !vcodec) {
		return false;
	}

	avformat_alloc_output_context2(&encoder->context, 0, 0, outfile);

	encoder->audioStream = avformat_new_stream(encoder->context, acodec);
	encoder->audio = encoder->audioStream->codec;
	encoder->audio->bit_rate = encoder->audioBitrate;
	encoder->audio->sample_rate = 0x8000;
	encoder->audio->channels = 2;
	encoder->audio->channel_layout = AV_CH_LAYOUT_STEREO;
	encoder->audio->sample_fmt = AV_SAMPLE_FMT_S16;
	avcodec_open2(encoder->audio, acodec, 0);
	encoder->audioFrame = av_frame_alloc();
	encoder->audioFrame->nb_samples = encoder->audio->frame_size;
	encoder->audioFrame->format = encoder->audio->sample_fmt;
	encoder->audioFrame->pts = 0;
	encoder->audioBufferSize = av_samples_get_buffer_size(0, encoder->audio->channels, encoder->audio->frame_size, encoder->audio->sample_fmt, 0);
	encoder->audioBuffer = av_malloc(encoder->audioBufferSize);
	avcodec_fill_audio_frame(encoder->audioFrame, encoder->audio->channels, encoder->audio->sample_fmt, (const uint8_t*) encoder->audioBuffer, encoder->audioBufferSize, 0);

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
	av_write_trailer(encoder->context);
	avio_close(encoder->context->pb);

	av_free(encoder->audioBuffer);
	av_frame_free(&encoder->audioFrame);
	avcodec_close(encoder->audio);

	av_frame_free(&encoder->videoFrame);
	avcodec_close(encoder->video);
	avformat_free_context(encoder->context);
}

void _ffmpegPostAudioFrame(struct GBAAVStream* stream, int32_t left, int32_t right) {
	struct FFmpegEncoder* encoder = (struct FFmpegEncoder*) stream;

	av_frame_make_writable(encoder->audioFrame);
	encoder->audioBuffer[encoder->currentAudioSample * 2] = left;
	encoder->audioBuffer[encoder->currentAudioSample * 2 + 1] = right;
	encoder->audioFrame->pts = av_rescale_q(encoder->currentAudioFrame, encoder->audio->time_base, encoder->audioStream->time_base);
	++encoder->currentAudioFrame;
	++encoder->currentAudioSample;

	if ((encoder->currentAudioSample * 4) < encoder->audioBufferSize) {
		return;
	}
	encoder->currentAudioSample = 0;

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
