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
	AudioThread(QObject* parent = nullptr);

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
	AudioDevice(GBAAudio* audio, QObject* parent = nullptr);

	void setFormat(const QAudioFormat& format);

protected:
	virtual qint64 readData(char* data, qint64 maxSize) override;
	virtual qint64 writeData(const char* data, qint64 maxSize) override;

private:
	GBAAudio* m_audio;
	float m_drift;
	float m_ratio;
};

}

#endif
