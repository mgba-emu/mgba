#ifndef QGBA_AUDIO_DEVICE
#define QGBA_AUDIO_DEVICE

#include <QAudioFormat>
#include <QAudioOutput>
#include <QIODevice>
#include <QThread>

struct GBAAudio;

namespace QGBA {

class AudioDevice : public QIODevice {
Q_OBJECT

public:
	AudioDevice(GBAAudio* audio, QObject* parent = 0);

	void setFormat(const QAudioFormat& format);

	class Thread : public QThread {
	public:
		Thread(QObject* parent = 0);

		void setInput(GBAAudio* input);

	protected:
		void run();

	private:
		GBAAudio* m_input;
	};

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
