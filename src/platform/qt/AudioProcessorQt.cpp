/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AudioProcessorQt.h"

#include "AudioDevice.h"
#include "LogController.h"

#include <QAudioOutput>

extern "C" {
#include "gba/supervisor/thread.h"
}

using namespace QGBA;

AudioProcessorQt::AudioProcessorQt(QObject* parent)
	: AudioProcessor(parent)
	, m_audioOutput(nullptr)
	, m_device(nullptr)
	, m_sampleRate(44100)
{
}

void AudioProcessorQt::setInput(GBAThread* input) {
	AudioProcessor::setInput(input);
	if (m_device) {
		m_device->setInput(input);
		if (m_audioOutput) {
			m_device->setFormat(m_audioOutput->format());
		}
	}
}

void AudioProcessorQt::start() {
	if (!input()) {
		LOG(WARN) << tr("Can't start an audio processor without input");
		return;
	}

	if (!m_device) {
		m_device = new AudioDevice(this);
	}

	if (!m_audioOutput) {
		QAudioFormat format;
		format.setSampleRate(m_sampleRate);
		format.setChannelCount(2);
		format.setSampleSize(16);
		format.setCodec("audio/pcm");
		format.setByteOrder(QAudioFormat::LittleEndian);
		format.setSampleType(QAudioFormat::SignedInt);

		m_audioOutput = new QAudioOutput(format, this);
		m_audioOutput->setCategory("game");
	}

	m_device->setInput(input());
	m_device->setFormat(m_audioOutput->format());
	m_audioOutput->setBufferSize(input()->audioBuffers * 4);

	m_audioOutput->start(m_device);
}

void AudioProcessorQt::pause() {
	if (m_audioOutput) {
		m_audioOutput->stop();
	}
}

void AudioProcessorQt::setBufferSamples(int samples) {
	AudioProcessor::setBufferSamples(samples);
	if (m_audioOutput) {
		m_audioOutput->stop();
		m_audioOutput->setBufferSize(samples * 4);
		m_audioOutput->start(m_device);
	}
}

void AudioProcessorQt::inputParametersChanged() {
	if (m_device) {
		m_device->setFormat(m_audioOutput->format());
	}
}

void AudioProcessorQt::requestSampleRate(unsigned rate) {
	m_sampleRate = rate;
	if (m_device) {
		QAudioFormat format(m_audioOutput->format());
		format.setSampleRate(rate);
		m_device->setFormat(format);
	}
}

unsigned AudioProcessorQt::sampleRate() const {
	if (!m_audioOutput) {
		return 0;
	}
	return m_audioOutput->format().sampleRate();
}
