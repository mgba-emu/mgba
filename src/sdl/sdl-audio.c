#include "sdl-audio.h"

#include "gba.h"
#include "gba-thread.h"

static void _GBASDLAudioCallback(void* context, Uint8* data, int len);

int GBASDLInitAudio(struct GBASDLAudio* context) {
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
		GBALog(0, GBA_LOG_ERROR, "Could not initialize SDL sound system");
		return 0;
	}

	context->desiredSpec.freq = 44100;
	context->desiredSpec.format = AUDIO_S16SYS;
	context->desiredSpec.channels = 2;
	context->desiredSpec.samples = GBA_AUDIO_SAMPLES >> 2;
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
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

static void _pulldownResample(struct GBASDLAudio* context) {
	int32_t value;
	if (CircleBufferRead32(&context->audio->left, &value)) {
		context->currentSample.left = value << 7;
	} else {
		context->currentSample.left = 0;
	}
	if (CircleBufferRead32(&context->audio->right, &value)) {
		context->currentSample.right = value << 7;
	} else {
		context->currentSample.right = 0;
	}
}

static void _GBASDLAudioCallback(void* context, Uint8* data, int len) {
	struct GBASDLAudio* audioContext = context;
	int i;
	if (!context || !audioContext->audio) {
		for (i = 0; i < len; ++i) {
			data[i] = 0;
		}
		return;
	}
	struct StereoSample* ssamples = (struct StereoSample*) data;
	len /= 2 * audioContext->obtainedSpec.channels;
	if (audioContext->obtainedSpec.channels == 2) {
		pthread_mutex_lock(&audioContext->audio->bufferMutex);
		for (i = 0; i < len; ++i) {
			audioContext->drift += audioContext->audio->sampleRate / (float) audioContext->obtainedSpec.freq;
			while (audioContext->drift >= 0) {
				_pulldownResample(audioContext);
				audioContext->drift -= 1.f;
			}
			ssamples[i] = audioContext->currentSample;
		}
		GBASyncConsumeAudio(audioContext->audio->p->sync);
		pthread_mutex_unlock(&audioContext->audio->bufferMutex);
	}
}
