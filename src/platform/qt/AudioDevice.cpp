/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AudioDevice.h"

extern "C" {
#include "gba.h"
#include "gba-audio.h"
#include "gba-thread.h"
}

using namespace QGBA;

AudioDevice::AudioDevice(QObject* parent)
	: QIODevice(parent)
	, m_context(nullptr)
	, m_drift(0)
{
	setOpenMode(ReadOnly);
}

void AudioDevice::setFormat(const QAudioFormat& format) {
	if (!GBAThreadHasStarted(m_context)) {
		return;
	}
	GBAThreadInterrupt(m_context);
	m_ratio = GBAAudioCalculateRatio(&m_context->gba->audio, m_context->fpsTarget, format.sampleRate());
	GBAThreadContinue(m_context);
}

void AudioDevice::setInput(GBAThread* input) {
	m_context = input;
}

qint64 AudioDevice::readData(char* data, qint64 maxSize) {
	if (maxSize > 0xFFFFFFFF) {
		maxSize = 0xFFFFFFFF;
	}

	if (!m_context->gba) {
		return 0;
	}

	return GBAAudioResampleNN(&m_context->gba->audio, m_ratio, &m_drift, reinterpret_cast<GBAStereoSample*>(data), maxSize / sizeof(GBAStereoSample)) * sizeof(GBAStereoSample);
}

qint64 AudioDevice::writeData(const char*, qint64) {
	return 0;
}
