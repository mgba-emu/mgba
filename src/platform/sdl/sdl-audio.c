/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "sdl-audio.h"

#include <mgba/core/core.h>
#include <mgba/core/thread.h>
#include <mgba/internal/gba/audio.h>
#include <mgba/internal/gba/gba.h>

#include <mgba/core/blip_buf.h>

#define BUFFER_SIZE (GBA_AUDIO_SAMPLES >> 2)

mLOG_DEFINE_CATEGORY(SDL_AUDIO, "SDL Audio", "platform.sdl.audio");

static void _mSDLAudioCallback(void* context, Uint8* data, int len);

bool mSDLInitAudio(struct mSDLAudio* context, struct mCoreThread* threadContext) {
#if defined(_WIN32) && SDL_VERSION_ATLEAST(2, 0, 8)
	if (!getenv("SDL_AUDIODRIVER")) {
		_putenv_s("SDL_AUDIODRIVER", "directsound");
	}
#endif
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
		mLOG(SDL_AUDIO, ERROR, "Could not initialize SDL sound system: %s", SDL_GetError());
		return false;
	}

	context->desiredSpec.freq = context->sampleRate;
	context->desiredSpec.format = AUDIO_S16SYS;
	context->desiredSpec.channels = 2;
	context->desiredSpec.samples = context->samples;
	context->desiredSpec.callback = _mSDLAudioCallback;
	context->desiredSpec.userdata = context;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	context->deviceId = SDL_OpenAudioDevice(0, 0, &context->desiredSpec, &context->obtainedSpec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
	if (context->deviceId == 0) {
#else
	if (SDL_OpenAudio(&context->desiredSpec, &context->obtainedSpec) < 0) {
#endif
		mLOG(SDL_AUDIO, ERROR, "Could not open SDL sound system");
		return false;
	}
	context->core = 0;

	if (threadContext) {
		context->core = threadContext->core;
		context->sync = &threadContext->impl->sync;

#if SDL_VERSION_ATLEAST(2, 0, 0)
		SDL_PauseAudioDevice(context->deviceId, 0);
#else
		SDL_PauseAudio(0);
#endif
	}

	return true;
}

void mSDLDeinitAudio(struct mSDLAudio* context) {
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

void mSDLPauseAudio(struct mSDLAudio* context) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_PauseAudioDevice(context->deviceId, 1);
#else
	UNUSED(context);
	SDL_PauseAudio(1);
#endif
}

void mSDLResumeAudio(struct mSDLAudio* context) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_PauseAudioDevice(context->deviceId, 0);
#else
	UNUSED(context);
	SDL_PauseAudio(0);
#endif
}

static void _mSDLAudioCallback(void* context, Uint8* data, int len) {
	struct mSDLAudio* audioContext = context;
	if (!context || !audioContext->core) {
		memset(data, 0, len);
		return;
	}
	blip_t* left = NULL;
	blip_t* right = NULL;
	int32_t clockRate = GBA_ARM7TDMI_FREQUENCY;
	if (audioContext->core) {
		left = audioContext->core->getAudioChannel(audioContext->core, 0);
		right = audioContext->core->getAudioChannel(audioContext->core, 1);
		clockRate = audioContext->core->frequency(audioContext->core);
	}
	double fauxClock = 1;
	if (audioContext->sync) {
		if (audioContext->sync->fpsTarget > 0) {
			fauxClock = GBAAudioCalculateRatio(1, audioContext->sync->fpsTarget, 1);
		}
		mCoreSyncLockAudio(audioContext->sync);
	}
	blip_set_rates(left, clockRate, audioContext->obtainedSpec.freq * fauxClock);
	blip_set_rates(right, clockRate, audioContext->obtainedSpec.freq * fauxClock);
	len /= 2 * audioContext->obtainedSpec.channels;
	int available = blip_samples_avail(left);
	if (available > len) {
		available = len;
	}
	blip_read_samples(left, (short*) data, available, audioContext->obtainedSpec.channels == 2);
	if (audioContext->obtainedSpec.channels == 2) {
		blip_read_samples(right, ((short*) data) + 1, available, 1);
	}

	if (audioContext->sync) {
		mCoreSyncConsumeAudio(audioContext->sync);
	}
	if (available < len) {
		memset(((short*) data) + audioContext->obtainedSpec.channels * available, 0, (len - available) * audioContext->obtainedSpec.channels * sizeof(short));
	}
}
