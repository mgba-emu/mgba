/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AudioDevice.h"

#include "LogController.h"

extern "C" {
#include "core/thread.h"
#include "gba/gba.h"
#include "gba/audio.h"
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
	if (!m_context || !mCoreThreadIsActive(m_context)) {
		LOG(QT, INFO) << tr("Can't set format of context-less audio device");
		return;
	}
	double fauxClock = GBAAudioCalculateRatio(1, 60, 1); // TODO: Put back fpsTarget
	mCoreSyncLockAudio(&m_context->sync);
	blip_set_rates(m_context->core->getAudioChannel(m_context->core, 0), GBA_ARM7TDMI_FREQUENCY, format.sampleRate() * fauxClock);
	blip_set_rates(m_context->core->getAudioChannel(m_context->core, 1), GBA_ARM7TDMI_FREQUENCY, format.sampleRate() * fauxClock);
	mCoreSyncUnlockAudio(&m_context->sync);
}

void AudioDevice::setInput(mCoreThread* input) {
	m_context = input;
}

qint64 AudioDevice::readData(char* data, qint64 maxSize) {
	if (maxSize > 0xFFFFFFFF) {
		maxSize = 0xFFFFFFFF;
	}

	if (!m_context->core) {
		LOG(QT, WARN) << tr("Audio device is missing its core");
		return 0;
	}

	mCoreSyncLockAudio(&m_context->sync);
	int available = blip_samples_avail(m_context->core->getAudioChannel(m_context->core, 0));
	if (available > maxSize / sizeof(GBAStereoSample)) {
		available = maxSize / sizeof(GBAStereoSample);
	}
	blip_read_samples(m_context->core->getAudioChannel(m_context->core, 0), &reinterpret_cast<GBAStereoSample*>(data)->left, available, true);
	blip_read_samples(m_context->core->getAudioChannel(m_context->core, 1), &reinterpret_cast<GBAStereoSample*>(data)->right, available, true);
	mCoreSyncConsumeAudio(&m_context->sync);
	return available * sizeof(GBAStereoSample);
}

qint64 AudioDevice::writeData(const char*, qint64) {
	LOG(QT, WARN) << tr("Writing data to read-only audio device");
	return 0;
}
