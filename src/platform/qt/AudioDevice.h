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
		Thread(AudioDevice* device, QObject* parent = 0);

		void setOutput(QAudioOutput* output);

	protected:
		void run();

	private:
		QAudioOutput* m_audio;
		AudioDevice* m_device;
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
