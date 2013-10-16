#ifndef SDL_AUDIO_H
#define SDL_AUDIO_H

#include <SDL.h>

struct StereoSample {
	Sint16 left;
	Sint16 right;
};

struct GBASDLAudio {
	SDL_AudioSpec desiredSpec;
	SDL_AudioSpec obtainedSpec;
	float drift;
	struct GBAAudio* audio;
	struct StereoSample currentSample;
};

int GBASDLInitAudio(struct GBASDLAudio* context);
void GBASDLDeinitAudio(struct GBASDLAudio* context);

#endif
