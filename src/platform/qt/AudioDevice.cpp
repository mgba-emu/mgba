/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AudioDevice.h"

extern "C" {
#include "gba/gba.h"
#include "gba/audio.h"
#include "gba/supervisor/thread.h"
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
#if RESAMPLE_LIBRARY == RESAMPLE_NN
	GBAThreadInterrupt(m_context);
	m_ratio = GBAAudioCalculateRatio(m_context->gba->audio.sampleRate, m_context->fpsTarget, format.sampleRate());
	GBAThreadContinue(m_context);
#elif RESAMPLE_LIBRARY == RESAMPLE_BLIP_BUF
	double fauxClock = GBAAudioCalculateRatio(1, m_context->fpsTarget, 1);
	GBASyncLockAudio(&m_context->sync);
	blip_set_rates(m_context->gba->audio.left, GBA_ARM7TDMI_FREQUENCY, format.sampleRate() * fauxClock);
	blip_set_rates(m_context->gba->audio.right, GBA_ARM7TDMI_FREQUENCY, format.sampleRate() * fauxClock);
#endif
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

#if RESAMPLE_LIBRARY == RESAMPLE_NN
	return GBAAudioResampleNN(&m_context->gba->audio, m_ratio, &m_drift, reinterpret_cast<GBAStereoSample*>(data), maxSize / sizeof(GBAStereoSample)) * sizeof(GBAStereoSample);
#elif RESAMPLE_LIBRARY == RESAMPLE_BLIP_BUF
	int available = blip_samples_avail(m_context->gba->audio.left);
	if (available > maxSize / sizeof(GBAStereoSample)) {
		available = maxSize / sizeof(GBAStereoSample);
	}
	blip_read_samples(m_context->gba->audio.left, &reinterpret_cast<GBAStereoSample*>(data)->left, available, true);
	blip_read_samples(m_context->gba->audio.right, &reinterpret_cast<GBAStereoSample*>(data)->right, available, true);
	GBASyncConsumeAudio(&m_context->sync);
	return available * sizeof(GBAStereoSample);
#endif
}

qint64 AudioDevice::writeData(const char*, qint64) {
	return 0;
}
