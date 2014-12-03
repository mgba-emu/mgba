/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "sdl-audio.h"

#include "gba.h"
#include "gba-thread.h"

#define BUFFER_SIZE (GBA_AUDIO_SAMPLES >> 2)

static void _GBASDLAudioCallback(void* context, Uint8* data, int len);

bool GBASDLInitAudio(struct GBASDLAudio* context, struct GBAThread* threadContext) {
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
		GBALog(0, GBA_LOG_ERROR, "Could not initialize SDL sound system");
		return false;
	}

	context->desiredSpec.freq = 44100;
	context->desiredSpec.format = AUDIO_S16SYS;
	context->desiredSpec.channels = 2;
	context->desiredSpec.samples = context->samples;
	context->desiredSpec.callback = _GBASDLAudioCallback;
	context->desiredSpec.userdata = context;
	context->drift = 0.f;
	if (SDL_OpenAudio(&context->desiredSpec, &context->obtainedSpec) < 0) {
		GBALog(0, GBA_LOG_ERROR, "Could not open SDL sound system");
		return false;
	}
	context->thread = threadContext;
	context->samples = context->obtainedSpec.samples;
	if (context->samples > threadContext->audioBuffers) {
		threadContext->audioBuffers = context->samples * 2;
	}

	SDL_PauseAudio(0);
	return true;
}

void GBASDLDeinitAudio(struct GBASDLAudio* context) {
	UNUSED(context);
	SDL_PauseAudio(1);
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void GBASDLPauseAudio(struct GBASDLAudio* context) {
	UNUSED(context);
	SDL_PauseAudio(1);
}

void GBASDLResumeAudio(struct GBASDLAudio* context) {
	UNUSED(context);
	SDL_PauseAudio(0);
}

static void _GBASDLAudioCallback(void* context, Uint8* data, int len) {
	struct GBASDLAudio* audioContext = context;
	if (!context || !audioContext->thread || !audioContext->thread->gba) {
		memset(data, 0, len);
		return;
	}
	audioContext->ratio = GBAAudioCalculateRatio(&audioContext->thread->gba->audio, audioContext->thread->fpsTarget, audioContext->obtainedSpec.freq);
	if (audioContext->ratio == INFINITY) {
		memset(data, 0, len);
		return;
	}
	struct GBAStereoSample* ssamples = (struct GBAStereoSample*) data;
	len /= 2 * audioContext->obtainedSpec.channels;
	if (audioContext->obtainedSpec.channels == 2) {
		GBAAudioResampleNN(&audioContext->thread->gba->audio, audioContext->ratio, &audioContext->drift, ssamples, len);
	}
}
