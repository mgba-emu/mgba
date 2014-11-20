#include "AudioProcessorSDL.h"

extern "C" {
#include "gba-thread.h"
}

using namespace QGBA;

AudioProcessorSDL::AudioProcessorSDL(QObject* parent)
	: AudioProcessor(parent)
	, m_audio()
{
}

AudioProcessorSDL::~AudioProcessorSDL() {
	GBASDLDeinitAudio(&m_audio);
}

void AudioProcessorSDL::start() {
	if (m_audio.thread) {
		GBASDLResumeAudio(&m_audio);
	} else {
		m_audio.samples = input()->audioBuffers;
		GBASDLInitAudio(&m_audio, input());
	}
}

void AudioProcessorSDL::pause() {
	GBASDLPauseAudio(&m_audio);
}

void AudioProcessorSDL::setBufferSamples(int samples) {
	if (m_audio.thread) {
		GBASDLDeinitAudio(&m_audio);
		m_audio.samples = samples;
		GBASDLInitAudio(&m_audio, input());
	}
}

void AudioProcessorSDL::inputParametersChanged() {
}
