#include "AudioDevice.h"

extern "C" {
#include "gba.h"
#include "gba-audio.h"
}

using namespace QGBA;

AudioDevice::AudioDevice(GBAAudio* audio, QObject* parent)
	: QIODevice(parent)
	, m_audio(audio)
{
	setOpenMode(ReadOnly);
}

void AudioDevice::setFormat(const QAudioFormat& format) {
	// TODO: merge where the fudge rate exists
	float fudgeRate = 16853760.0f / GBA_ARM7TDMI_FREQUENCY;
	m_ratio = format.sampleRate() / (float) (m_audio->sampleRate * fudgeRate);
}

qint64 AudioDevice::readData(char* data, qint64 maxSize) {
	if (maxSize > 0xFFFFFFFF) {
		maxSize = 0xFFFFFFFF;
	}

	return GBAAudioResampleNN(m_audio, m_ratio, &m_drift, reinterpret_cast<GBAStereoSample*>(data), maxSize / sizeof(GBAStereoSample)) * sizeof(GBAStereoSample);
}

qint64 AudioDevice::writeData(const char*, qint64) {
	return 0;
}

AudioDevice::Thread::Thread(QObject* parent)
	: QThread(parent)
{
	// Nothing to do
}

void AudioDevice::Thread::setInput(GBAAudio* input) {
	m_input = input;
}

void AudioDevice::Thread::run() {
	QAudioFormat format;
	format.setSampleRate(44100);
	format.setChannelCount(2);
	format.setSampleSize(16);
	format.setCodec("audio/pcm");
	format.setByteOrder(QAudioFormat::LittleEndian);
	format.setSampleType(QAudioFormat::SignedInt);

	AudioDevice device(m_input);
	QAudioOutput audioOutput(format);
	audioOutput.setBufferSize(1024);
	device.setFormat(audioOutput.format());

	audioOutput.start(&device);
	exec();
	audioOutput.stop();
}
