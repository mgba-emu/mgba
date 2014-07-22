#include "AudioProcessor.h"

#include "AudioDevice.h"

#include <QAudioOutput>

extern "C" {
#include "gba-thread.h"
}

using namespace QGBA;

AudioProcessor::AudioProcessor(QObject* parent)
	: QObject(parent)
	, m_audioOutput(nullptr)
	, m_device(nullptr)
{
}

void AudioProcessor::setInput(GBAThread* input) {
	m_context = input;
	if (m_device) {
		m_device->setInput(input);
		if (m_audioOutput) {
			m_device->setFormat(m_audioOutput->format());
		}
	}
}

void AudioProcessor::start() {
	if (!m_device) {
		m_device = new AudioDevice(this);
	}

	if (!m_audioOutput) {
		QAudioFormat format;
		format.setSampleRate(44100);
		format.setChannelCount(2);
		format.setSampleSize(16);
		format.setCodec("audio/pcm");
		format.setByteOrder(QAudioFormat::LittleEndian);
		format.setSampleType(QAudioFormat::SignedInt);

		m_audioOutput = new QAudioOutput(format, this);
	}

	m_device->setInput(m_context);
	m_device->setFormat(m_audioOutput->format());
	m_audioOutput->setBufferSize(m_context->audioBuffers * 4);

	m_audioOutput->start(m_device);
}

void AudioProcessor::pause() {
	if (m_audioOutput) {
		m_audioOutput->stop();
	}
}

void AudioProcessor::setBufferSamples(int samples) {
	QAudioFormat format = m_audioOutput->format();
	m_audioOutput->setBufferSize(samples * format.channelCount() * format.sampleSize() / 8);
}
