#include "sdl-audio.h"

#include "gba.h"
#include "gba-thread.h"

#define BUFFER_SIZE (GBA_AUDIO_SAMPLES >> 2)
#define FPS_TARGET 60.f

static void _GBASDLAudioCallback(void* context, Uint8* data, int len);

bool GBASDLInitAudio(struct GBASDLAudio* context) {
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
		GBALog(0, GBA_LOG_ERROR, "Could not initialize SDL sound system");
		return false;
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
		return false;
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

static void _GBASDLAudioCallback(void* context, Uint8* data, int len) {
	struct GBASDLAudio* audioContext = context;
	if (!context || !audioContext->audio) {
		memset(data, 0, len);
		return;
	}
	float ratio = 280896.0f * FPS_TARGET / GBA_ARM7TDMI_FREQUENCY;
	audioContext->ratio = audioContext->obtainedSpec.freq / ratio / (float) audioContext->audio->sampleRate;
	struct GBAStereoSample* ssamples = (struct GBAStereoSample*) data;
	len /= 2 * audioContext->obtainedSpec.channels;
	if (audioContext->obtainedSpec.channels == 2) {
		GBAAudioResampleNN(audioContext->audio, audioContext->ratio, &audioContext->drift, ssamples, len);
	}
}
