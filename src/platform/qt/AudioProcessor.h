#ifndef QGBA_AUDIO_PROCESSOR
#define QGBA_AUDIO_PROCESSOR
#include <QObject>

struct GBAThread;

namespace QGBA {

class AudioProcessor : public QObject {
Q_OBJECT

public:
	static AudioProcessor* create();
	AudioProcessor(QObject* parent = nullptr);

	virtual void setInput(GBAThread* input);

public slots:
	virtual void start() = 0;
	virtual void pause() = 0;

	virtual void setBufferSamples(int samples) = 0;
	virtual void inputParametersChanged() = 0;

protected:
	GBAThread* input() { return m_context; }
private:
	GBAThread* m_context;
};

}

#endif
