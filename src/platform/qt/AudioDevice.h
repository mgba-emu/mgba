#ifndef QGBA_AUDIO_DEVICE
#define QGBA_AUDIO_DEVICE

#include <QAudioFormat>
#include <QAudioOutput>
#include <QIODevice>
#include <QThread>

struct GBAThread;

namespace QGBA {

class AudioDevice : public QIODevice {
Q_OBJECT

public:
	AudioDevice(GBAThread* threadContext, QObject* parent = nullptr);

	void setFormat(const QAudioFormat& format);

protected:
	virtual qint64 readData(char* data, qint64 maxSize) override;
	virtual qint64 writeData(const char* data, qint64 maxSize) override;

private:
	GBAThread* m_context;
	float m_drift;
	float m_ratio;
};

class AudioThread : public QThread {
Q_OBJECT

public:
	AudioThread(QObject* parent = nullptr);

	void setInput(GBAThread* input);

public slots:
	void shutdown();
	void pause();
	void resume();

protected:
	void run();

private:
	GBAThread* m_input;
	QAudioOutput* m_audioOutput;
	AudioDevice* m_device;
};

}

#endif
