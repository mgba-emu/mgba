/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AudioProcessorQt.h"

#include "AudioDevice.h"

#include <QAudioOutput>

extern "C" {
#include "gba-thread.h"
}

using namespace QGBA;

AudioProcessorQt::AudioProcessorQt(QObject* parent)
	: AudioProcessor(parent)
	, m_audioOutput(nullptr)
	, m_device(nullptr)
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
