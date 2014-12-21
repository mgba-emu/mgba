/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ffmpeg-resample.h"

#include "gba-audio.h"

#include <libavresample/avresample.h>
#include <libavutil/opt.h>

struct AVAudioResampleContext* GBAAudioOpenLAVR(struct GBAAudio* audio, unsigned outputRate) {
	AVAudioResampleContext *avr = avresample_alloc_context();
	av_opt_set_int(avr, "in_channel_layout", AV_CH_LAYOUT_STEREO, 0);
	av_opt_set_int(avr, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
	av_opt_set_int(avr, "in_sample_rate", audio->sampleRate, 0);
	av_opt_set_int(avr, "out_sample_rate", outputRate, 0);
	av_opt_set_int(avr, "in_sample_fmt", AV_SAMPLE_FMT_S16P, 0);
	av_opt_set_int(avr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
	if (avresample_open(avr)) {
		avresample_free(&avr);
		return 0;
	}
	return avr;
}

unsigned GBAAudioResampleLAVR(struct GBAAudio* audio, struct AVAudioResampleContext* avr, struct GBAStereoSample* output, unsigned nSamples) {
	int16_t left[GBA_AUDIO_SAMPLES];
	int16_t right[GBA_AUDIO_SAMPLES];
	int16_t* samples[2] = { left, right };

	size_t totalRead = 0;
	size_t available = avresample_available(avr);
	if (available) {
		totalRead = avresample_read(avr, (uint8_t**) &output, nSamples);
		nSamples -= totalRead;
		output += totalRead;
	}
	while (nSamples) {
		unsigned read = GBAAudioCopy(audio, left, right, GBA_AUDIO_SAMPLES);

		size_t currentRead = avresample_convert(avr, (uint8_t**) &output, nSamples * sizeof(struct GBAStereoSample), nSamples, (uint8_t**) samples, sizeof(left), read);
		nSamples -= currentRead;
		output += currentRead;
		totalRead += currentRead;
		if (read < GBA_AUDIO_SAMPLES && nSamples) {
			memset(output, 0, nSamples * sizeof(struct GBAStereoSample));
			break;
		}
	}
	return totalRead;
}
