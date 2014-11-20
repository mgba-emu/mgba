#ifndef QGBA_AUDIO_PROCESSOR_QT
#define QGBA_AUDIO_PROCESSOR_QT
#include "AudioProcessor.h"

namespace QGBA {

class AudioDevice;

class AudioProcessorQt : public AudioProcessor {
Q_OBJECT

public:
	AudioProcessorQt(QObject* parent = nullptr);

	virtual void setInput(GBAThread* input);

public slots:
	virtual void start();
	virtual void pause();

	virtual void setBufferSamples(int samples);
	virtual void inputParametersChanged();

private:
	QAudioOutput* m_audioOutput;
	AudioDevice* m_device;
};

}

#endif
