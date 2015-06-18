/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SDL_AUDIO_H
#define SDL_AUDIO_H

#include "util/common.h"

#include <SDL.h>

#include "gba/audio.h"

struct GBASDLAudio {
	// Input
	size_t samples;

	// State
	SDL_AudioSpec desiredSpec;
	SDL_AudioSpec obtainedSpec;
#if RESAMPLE_LIBRARY != RESAMPLE_BLIP_BUF
	float ratio;
#endif
#if RESAMPLE_LIBRARY == RESAMPLE_NN
	float drift;
#endif
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_AudioDeviceID deviceId;
#endif

	struct GBAThread* thread;
};

bool GBASDLInitAudio(struct GBASDLAudio* context, struct GBAThread* threadContext);
void GBASDLDeinitAudio(struct GBASDLAudio* context);
void GBASDLPauseAudio(struct GBASDLAudio* context);
void GBASDLResumeAudio(struct GBASDLAudio* context);

#endif
