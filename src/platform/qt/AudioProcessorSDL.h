#ifndef QGBA_AUDIO_PROCESSOR_SDL
#define QGBA_AUDIO_PROCESSOR_SDL
#include "AudioProcessor.h"

#ifdef BUILD_SDL

extern "C" {
#include "platform/sdl/sdl-audio.h"
}

namespace QGBA {

class AudioProcessorSDL : public AudioProcessor {
Q_OBJECT

public:
	AudioProcessorSDL(QObject* parent = nullptr);
	~AudioProcessorSDL();

public slots:
	virtual void start();
	virtual void pause();

	virtual void setBufferSamples(int samples);
	virtual void inputParametersChanged();

private:
	GBASDLAudio m_audio;
};

}

#endif

#endif
