/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "sdl-audio.h"

#include "gba/gba.h"
#include "gba/supervisor/thread.h"

#if RESAMPLE_LIBRARY == RESAMPLE_BLIP_BUF
#include "third-party/blip_buf/blip_buf.h"
#endif

#define BUFFER_SIZE (GBA_AUDIO_SAMPLES >> 2)

static void _GBASDLAudioCallback(void* context, Uint8* data, int len);

bool GBASDLInitAudio(struct GBASDLAudio* context, struct GBAThread* threadContext) {
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
		GBALog(0, GBA_LOG_ERROR, "Could not initialize SDL sound system: %s", SDL_GetError());
		return false;
	}

	context->desiredSpec.freq = 44100;
	context->desiredSpec.format = AUDIO_S16SYS;
	context->desiredSpec.channels = 2;
	context->desiredSpec.samples = context->samples;
	context->desiredSpec.callback = _GBASDLAudioCallback;
	context->desiredSpec.userdata = context;
#if RESAMPLE_LIBRARY == RESAMPLE_NN
	context->drift = 0.f;
#endif

#if SDL_VERSION_ATLEAST(2, 0, 0)
	context->deviceId = SDL_OpenAudioDevice(0, 0, &context->desiredSpec, &context->obtainedSpec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
	if (context->deviceId == 0) {
#else
	if (SDL_OpenAudio(&context->desiredSpec, &context->obtainedSpec) < 0) {
#endif
		GBALog(0, GBA_LOG_ERROR, "Could not open SDL sound system");
		return false;
	}
	context->thread = threadContext;
	context->samples = context->obtainedSpec.samples;
	float ratio = GBAAudioCalculateRatio(0x8000, threadContext->fpsTarget, 44100);
	threadContext->audioBuffers = context->samples / ratio;
	if (context->samples > threadContext->audioBuffers) {
		threadContext->audioBuffers = context->samples * 2;
	}

#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_PauseAudioDevice(context->deviceId, 0);
#else
	SDL_PauseAudio(0);
#endif
	return true;
}

void GBASDLDeinitAudio(struct GBASDLAudio* context) {
	UNUSED(context);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_PauseAudioDevice(context->deviceId, 1);
	SDL_CloseAudioDevice(context->deviceId);
#else
	SDL_PauseAudio(1);
	SDL_CloseAudio();
#endif
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void GBASDLPauseAudio(struct GBASDLAudio* context) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_PauseAudioDevice(context->deviceId, 1);
#else
	UNUSED(context);
	SDL_PauseAudio(1);
#endif
}

void GBASDLResumeAudio(struct GBASDLAudio* context) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_PauseAudioDevice(context->deviceId, 0);
#else
	UNUSED(context);
	SDL_PauseAudio(0);
#endif
}

static void _GBASDLAudioCallback(void* context, Uint8* data, int len) {
	struct GBASDLAudio* audioContext = context;
	if (!context || !audioContext->thread || !audioContext->thread->gba) {
		memset(data, 0, len);
		return;
	}
#if RESAMPLE_LIBRARY == RESAMPLE_NN
	audioContext->ratio = GBAAudioCalculateRatio(audioContext->thread->gba->audio.sampleRate, audioContext->thread->fpsTarget, audioContext->obtainedSpec.freq);
	if (audioContext->ratio == INFINITY) {
		memset(data, 0, len);
		return;
	}
	struct GBAStereoSample* ssamples = (struct GBAStereoSample*) data;
	len /= 2 * audioContext->obtainedSpec.channels;
	if (audioContext->obtainedSpec.channels == 2) {
		GBAAudioResampleNN(&audioContext->thread->gba->audio, audioContext->ratio, &audioContext->drift, ssamples, len);
	}
#elif RESAMPLE_LIBRARY == RESAMPLE_BLIP_BUF
	double fauxClock = GBAAudioCalculateRatio(1, audioContext->thread->fpsTarget, 1);
	GBASyncLockAudio(&audioContext->thread->sync);
	blip_set_rates(audioContext->thread->gba->audio.left, GBA_ARM7TDMI_FREQUENCY, audioContext->obtainedSpec.freq * fauxClock);
	blip_set_rates(audioContext->thread->gba->audio.right, GBA_ARM7TDMI_FREQUENCY, audioContext->obtainedSpec.freq * fauxClock);
	len /= 2 * audioContext->obtainedSpec.channels;
	int available = blip_samples_avail(audioContext->thread->gba->audio.left);
	if (available > len) {
		available = len;
	}
	blip_read_samples(audioContext->thread->gba->audio.left, (short*) data, available, audioContext->obtainedSpec.channels == 2);
	if (audioContext->obtainedSpec.channels == 2) {
		blip_read_samples(audioContext->thread->gba->audio.right, ((short*) data) + 1, available, 1);
	}
	GBASyncConsumeAudio(&audioContext->thread->sync);
	if (available < len) {
		memset(((short*) data) + audioContext->obtainedSpec.channels * available, 0, (len - available) * audioContext->obtainedSpec.channels * sizeof(short));
	}
#endif
}
