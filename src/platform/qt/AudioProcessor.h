#ifndef QGBA_AUDIO_PROCESSOR
#define QGBA_AUDIO_PROCESSOR
#include <QObject>

struct GBAThread;

class QAudioOutput;

namespace QGBA {

class AudioDevice;

class AudioProcessor : public QObject {
Q_OBJECT

public:
	AudioProcessor(QObject* parent = nullptr);

	void setInput(GBAThread* input);

public slots:
	void start();
	void pause();

	void setBufferSamples(int samples);
	void inputParametersChanged();

private:
	GBAThread* m_context;
	QAudioOutput* m_audioOutput;
	AudioDevice* m_device;
};

}

#endif
