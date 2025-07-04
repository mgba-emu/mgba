/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "sdl-audio.h"

#include <mgba/core/core.h>
#include <mgba/core/thread.h>

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

	mAudioBufferInit(&context->buffer, context->samples, context->obtainedSpec.channels);
	mAudioResamplerInit(&context->resampler, mINTERPOLATOR_SINC);
	mAudioResamplerSetDestination(&context->resampler, &context->buffer, context->obtainedSpec.freq);

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
	mAudioBufferDeinit(&context->buffer);
	mAudioResamplerDeinit(&context->resampler);
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
	struct mAudioBuffer* buffer = NULL;
	unsigned sampleRate = 32768;
	if (audioContext->core) {
		buffer = audioContext->core->getAudioBuffer(audioContext->core);
		sampleRate = audioContext->core->audioSampleRate(audioContext->core);
	}
	double fauxClock = 1;
	if (audioContext->sync) {
		if (audioContext->sync->fpsTarget > 0 && audioContext->core) {
			fauxClock = mCoreCalculateFramerateRatio(audioContext->core, audioContext->sync->fpsTarget);
		}
		mCoreSyncLockAudio(audioContext->sync);
		audioContext->sync->audioHighWater = audioContext->samples + audioContext->resampler.highWaterMark + audioContext->resampler.lowWaterMark + (audioContext->samples >> 6);
		audioContext->sync->audioHighWater *= sampleRate / (fauxClock * audioContext->obtainedSpec.freq);
	}
	mAudioResamplerSetSource(&audioContext->resampler, buffer, sampleRate / fauxClock, true);
	mAudioResamplerProcess(&audioContext->resampler);
	if (audioContext->sync) {
		mCoreSyncConsumeAudio(audioContext->sync);
	}
	len /= 2 * audioContext->obtainedSpec.channels;
	int available = mAudioBufferRead(&audioContext->buffer, (int16_t*) data, len);

	if (available < len) {
		memset(((short*) data) + audioContext->obtainedSpec.channels * available, 0, (len - available) * audioContext->obtainedSpec.channels * sizeof(short));
	}
}
