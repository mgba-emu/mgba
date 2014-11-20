#include "AudioProcessor.h"

#include "AudioDevice.h"

#ifdef BUILD_SDL
#include "AudioProcessorSDL.h"
#else
#include "AudioProcessorQt.h"
#endif

#include <QAudioOutput>

extern "C" {
#include "gba-thread.h"
}

using namespace QGBA;

AudioProcessor* AudioProcessor::create() {
#ifdef BUILD_SDL
	return new AudioProcessorSDL();
#else
	return new AudioProcessorQt();
#endif	
}

AudioProcessor::AudioProcessor(QObject* parent)
	: QObject(parent)
{
}

void AudioProcessor::setInput(GBAThread* input) {
	m_context = input;
}
