/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "sdl-audio.h"

#include "gba/gba.h"
#include "gba/supervisor/thread.h"

#include "third-party/blip_buf/blip_buf.h"

#define BUFFER_SIZE (GBA_AUDIO_SAMPLES >> 2)

static void _GBSDLAudioCallback(void* context, Uint8* data, int len);

bool GBSDLInitAudio(struct GBSDLAudio* context, struct GBAThread* threadContext) {
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
		GBALog(0, GBA_LOG_ERROR, "Could not initialize SDL sound system: %s", SDL_GetError());
		return false;
	}

	context->desiredSpec.freq = context->sampleRate;
	context->desiredSpec.format = AUDIO_S16SYS;
	context->desiredSpec.channels = 2;
	context->desiredSpec.samples = context->samples;
	context->desiredSpec.callback = _GBSDLAudioCallback;
	context->desiredSpec.userdata = context;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	context->deviceId = SDL_OpenAudioDevice(0, 0, &context->desiredSpec, &context->obtainedSpec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
	if (context->deviceId == 0) {
#else
	if (SDL_OpenAudio(&context->desiredSpec, &context->obtainedSpec) < 0) {
#endif
		GBALog(0, GBA_LOG_ERROR, "Could not open SDL sound system");
		return false;
	}
	context->samples = context->obtainedSpec.samples;
	context->psg = 0;
	context->thread = 0;

	if (threadContext) {
		context->thread = threadContext;
		float ratio = GBAAudioCalculateRatio(0x8000, threadContext->fpsTarget, 44100);
		threadContext->audioBuffers = context->samples / ratio;
		if (context->samples > threadContext->audioBuffers) {
			threadContext->audioBuffers = context->samples * 2;
		}
		context->sync = &threadContext->sync;

#if SDL_VERSION_ATLEAST(2, 0, 0)
		SDL_PauseAudioDevice(context->deviceId, 0);
#else
		SDL_PauseAudio(0);
#endif
	}

	return true;
}

void GBSDLDeinitAudio(struct GBSDLAudio* context) {
	UNUSED(context);
	context->psg = 0;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_PauseAudioDevice(context->deviceId, 1);
	SDL_CloseAudioDevice(context->deviceId);
#else
	SDL_PauseAudio(1);
	SDL_CloseAudio();
#endif
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void GBSDLPauseAudio(struct GBSDLAudio* context) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_PauseAudioDevice(context->deviceId, 1);
#else
	UNUSED(context);
	SDL_PauseAudio(1);
#endif
}

void GBSDLResumeAudio(struct GBSDLAudio* context) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_PauseAudioDevice(context->deviceId, 0);
#else
	UNUSED(context);
	SDL_PauseAudio(0);
#endif
}

static void _GBSDLAudioCallback(void* context, Uint8* data, int len) {
	struct GBSDLAudio* audioContext = context;
	if (!context || (!audioContext->psg && !audioContext->thread)) {
		memset(data, 0, len);
		return;
	}
	struct GBAudio* psg = audioContext->psg;
	if (!psg) {
		psg = &audioContext->thread->gba->audio.psg;
	}
	double fauxClock = 1;
	if (audioContext->thread) {
		fauxClock = GBAAudioCalculateRatio(1, audioContext->thread->fpsTarget, 1);
		mCoreSyncLockAudio(&audioContext->thread->sync);
	}
	blip_set_rates(psg->left, psg->clockRate, audioContext->obtainedSpec.freq * fauxClock);
	blip_set_rates(psg->right, psg->clockRate, audioContext->obtainedSpec.freq * fauxClock);
	len /= 2 * audioContext->obtainedSpec.channels;
	int available = blip_samples_avail(psg->left);
	if (available > len) {
		available = len;
	}
	blip_read_samples(psg->left, (short*) data, available, audioContext->obtainedSpec.channels == 2);
	if (audioContext->obtainedSpec.channels == 2) {
		blip_read_samples(psg->right, ((short*) data) + 1, available, 1);
	}

	if (audioContext->sync) {
		mCoreSyncConsumeAudio(audioContext->sync);
	}
	if (available < len) {
		memset(((short*) data) + audioContext->obtainedSpec.channels * available, 0, (len - available) * audioContext->obtainedSpec.channels * sizeof(short));
	}
}
