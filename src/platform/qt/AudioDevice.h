#ifndef QGBA_AUDIO_DEVICE
#define QGBA_AUDIO_DEVICE

#include <QAudioFormat>
#include <QIODevice>

struct GBAAudio;

namespace QGBA {

class AudioDevice : public QIODevice {
Q_OBJECT

public:
	AudioDevice(GBAAudio* audio, QObject* parent = 0);

	void setFormat(const QAudioFormat& format);

protected:
	virtual qint64 readData(char* data, qint64 maxSize);
	virtual qint64 writeData(const char* data, qint64 maxSize);

private:
	GBAAudio* m_audio;
	float m_drift;
	float m_ratio;
};

}

#endif
