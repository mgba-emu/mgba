/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AudioDevice.h"

#include "LogController.h"

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
	, m_ratio(1.f)
{
	setOpenMode(ReadOnly);
}

void AudioDevice::setFormat(const QAudioFormat& format) {
	if (!m_context || !GBAThreadIsActive(m_context)) {
		LOG(INFO) << tr("Can't set format of context-less audio device");
		return;
	}
	double fauxClock = GBAAudioCalculateRatio(1, m_context->fpsTarget, 1);
	mCoreSyncLockAudio(&m_context->sync);
	blip_set_rates(m_context->gba->audio.psg.left, GBA_ARM7TDMI_FREQUENCY, format.sampleRate() * fauxClock);
	blip_set_rates(m_context->gba->audio.psg.right, GBA_ARM7TDMI_FREQUENCY, format.sampleRate() * fauxClock);
	mCoreSyncUnlockAudio(&m_context->sync);
}

void AudioDevice::setInput(GBAThread* input) {
	m_context = input;
}

qint64 AudioDevice::readData(char* data, qint64 maxSize) {
	if (maxSize > 0xFFFFFFFF) {
		maxSize = 0xFFFFFFFF;
	}

	if (!m_context->gba) {
		LOG(WARN) << tr("Audio device is missing its GBA");
		return 0;
	}

	mCoreSyncLockAudio(&m_context->sync);
	int available = blip_samples_avail(m_context->gba->audio.psg.left);
	if (available > maxSize / sizeof(GBAStereoSample)) {
		available = maxSize / sizeof(GBAStereoSample);
	}
	blip_read_samples(m_context->gba->audio.psg.left, &reinterpret_cast<GBAStereoSample*>(data)->left, available, true);
	blip_read_samples(m_context->gba->audio.psg.right, &reinterpret_cast<GBAStereoSample*>(data)->right, available, true);
	mCoreSyncConsumeAudio(&m_context->sync);
	return available * sizeof(GBAStereoSample);
}

qint64 AudioDevice::writeData(const char*, qint64) {
	LOG(WARN) << tr("Writing data to read-only audio device");
	return 0;
}
