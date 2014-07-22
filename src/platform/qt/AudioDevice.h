#ifndef QGBA_AUDIO_DEVICE
#define QGBA_AUDIO_DEVICE
#include <QAudioFormat>
#include <QIODevice>

struct GBAThread;

namespace QGBA {

class AudioDevice : public QIODevice {
Q_OBJECT

public:
	AudioDevice(QObject* parent = nullptr);

	void setInput(GBAThread* input);
	void setFormat(const QAudioFormat& format);

protected:
	virtual qint64 readData(char* data, qint64 maxSize) override;
	virtual qint64 writeData(const char* data, qint64 maxSize) override;

private:
	GBAThread* m_context;
	float m_drift;
	float m_ratio;
};

}

#endif
