#include "sdl-audio.h"

#include "gba.h"
#include "gba-thread.h"

#define BUFFER_SIZE (GBA_AUDIO_SAMPLES >> 2)

struct StereoSample {
	Sint16 left;
	Sint16 right;
};

static void _GBASDLAudioCallback(void* context, Uint8* data, int len);

int GBASDLInitAudio(struct GBASDLAudio* context) {
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
		GBALog(0, GBA_LOG_ERROR, "Could not initialize SDL sound system");
		return 0;
	}

	context->desiredSpec.freq = 44100;
	context->desiredSpec.format = AUDIO_S16SYS;
	context->desiredSpec.channels = 2;
	context->desiredSpec.samples = GBA_AUDIO_SAMPLES;
	context->desiredSpec.callback = _GBASDLAudioCallback;
	context->desiredSpec.userdata = context;
	context->audio = 0;
	context->drift = 0.f;
	if (SDL_OpenAudio(&context->desiredSpec, &context->obtainedSpec) < 0) {
		GBALog(0, GBA_LOG_ERROR, "Could not open SDL sound system");
		return 0;
	}
	SDL_PauseAudio(0);
	return 1;
}

void GBASDLDeinitAudio(struct GBASDLAudio* context) {
	(void)(context);
	SDL_PauseAudio(1);
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

static void _pulldownResample(struct GBASDLAudio* context, struct StereoSample* output, ssize_t samples) {
	int32_t left[BUFFER_SIZE];
	int32_t right[BUFFER_SIZE];

	// toRead is in GBA samples
	int toRead = samples / context->ratio;
	while (samples > 0) {
		int currentRead = BUFFER_SIZE >> 2;
		if (currentRead > toRead) {
			currentRead = toRead;
		}
		unsigned read = GBAAudioCopy(context->audio, left, right, currentRead);
		toRead -= read;
		unsigned i;
		for (i = 0; i < read; ++i) {
			context->drift += context->ratio;
			while (context->drift >= 0) {
				output->left = left[i];
				output->right = right[i];
				++output;
				--samples;
#ifndef NDEBUG
				if (samples < 0) {
					abort();
				}
#endif
				context->drift -= 1.f;
			}
		}
		if (read < BUFFER_SIZE >> 2) {
			memset(output, 0, toRead);
			return;
		}
	}
}

static void _GBASDLAudioCallback(void* context, Uint8* data, int len) {
	struct GBASDLAudio* audioContext = context;
	if (!context || !audioContext->audio) {
		memset(data, 0, len);
		return;
	}
	audioContext->ratio = audioContext->obtainedSpec.freq / (float) audioContext->audio->sampleRate;
	struct StereoSample* ssamples = (struct StereoSample*) data;
	len /= 2 * audioContext->obtainedSpec.channels;
	if (audioContext->obtainedSpec.channels == 2) {
		_pulldownResample(audioContext, ssamples, len);
	}
}
