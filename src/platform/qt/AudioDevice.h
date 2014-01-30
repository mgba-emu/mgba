#ifndef QGBA_AUDIO_DEVICE
#define QGBA_AUDIO_DEVICE

#include <QAudioFormat>
#include <QAudioOutput>
#include <QIODevice>
#include <QThread>

struct GBAAudio;

namespace QGBA {



class AudioThread : public QThread {
Q_OBJECT

public:
	AudioThread(QObject* parent = 0);

	void setInput(GBAAudio* input);

public slots:
	void shutdown();

protected:
	void run();

private:
	GBAAudio* m_input;
	QAudioOutput* m_audioOutput;
};

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
