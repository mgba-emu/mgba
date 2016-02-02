/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SDL_AUDIO_H
#define SDL_AUDIO_H

#include "util/common.h"

#include <SDL.h>

struct GBSDLAudio {
	// Input
	size_t samples;
	unsigned sampleRate;

	// State
	SDL_AudioSpec desiredSpec;
	SDL_AudioSpec obtainedSpec;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_AudioDeviceID deviceId;
#endif

	struct GBAudio* psg;
	struct GBAThread* thread;
	struct mCoreSync* sync;
};

bool GBSDLInitAudio(struct GBSDLAudio* context, struct GBAThread*);
void GBSDLDeinitAudio(struct GBSDLAudio* context);
void GBSDLPauseAudio(struct GBSDLAudio* context);
void GBSDLResumeAudio(struct GBSDLAudio* context);

#endif
