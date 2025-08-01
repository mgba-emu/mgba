/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AudioDevice.h"

#include "GBAApp.h"
#include "LogController.h"

#include <mgba/core/core.h>
#include <mgba/core/thread.h>
#include <mgba/internal/gba/audio.h>

#include <QDebug>

using namespace QGBA;

AudioDevice::AudioDevice(QObject* parent)
	: QIODevice(parent)
	, m_context(nullptr)
{
	setOpenMode(ReadOnly);
	mAudioBufferInit(&m_buffer, 0x4000, 2);
	mAudioResamplerInit(&m_resampler, mINTERPOLATOR_SINC);
}

AudioDevice::~AudioDevice() {
	mAudioResamplerDeinit(&m_resampler);
	mAudioBufferDeinit(&m_buffer);
}

void AudioDevice::setFormat(const QAudioFormat& format) {
	if (!m_context || !mCoreThreadIsActive(m_context)) {
		LOG(QT, INFO) << tr("Can't set format of context-less audio device");
		return;
	}
	mCoreSyncLockAudio(&m_context->impl->sync);
	mCore* core = m_context->core;
	mAudioResamplerSetSource(&m_resampler, core->getAudioBuffer(core), core->audioSampleRate(core), true);
	m_format = format;
	adjustResampler();
	mCoreSyncUnlockAudio(&m_context->impl->sync);
}

void AudioDevice::setBufferSamples(int samples) {
	m_samples = samples;
}

void AudioDevice::setInput(mCoreThread* input) {
	m_context = input;
}

qint64 AudioDevice::readData(char* data, qint64 maxSize) {
	if (!m_context->core) {
		LOG(QT, WARN) << tr("Audio device is missing its core");
		return 0;
	}

	if (!maxSize) {
		return 0;
	}

	mCoreSyncLockAudio(&m_context->impl->sync);
	mAudioResamplerProcess(&m_resampler);
	if (mAudioBufferAvailable(&m_buffer) < 128) {
		mCoreSyncConsumeAudio(&m_context->impl->sync);
		// Audio is running slow...let's wait a tiny bit for more to come in
		QThread::usleep(100);
		mCoreSyncLockAudio(&m_context->impl->sync);
		mAudioResamplerProcess(&m_resampler);
	}
	quint64 available = std::min<quint64>({
		mAudioBufferAvailable(&m_buffer),
		static_cast<quint64>(maxSize / sizeof(mStereoSample)),
		std::numeric_limits<int>::max()
	});
	mAudioBufferRead(&m_buffer, reinterpret_cast<int16_t*>(data), available);
	mCoreSyncConsumeAudio(&m_context->impl->sync);
	return available * sizeof(mStereoSample);
}

qint64 AudioDevice::writeData(const char*, qint64) {
	LOG(QT, WARN) << tr("Writing data to read-only audio device");
	return 0;
}

bool AudioDevice::atEnd() const {
	return false;
}

qint64 AudioDevice::bytesAvailable() const {
	if (!m_context->core) {
		return true;
	}
	int available = mAudioBufferAvailable(&m_buffer);
	return available * sizeof(mStereoSample);
}

qint64 AudioDevice::bytesAvailable() {
	if (!m_context->core) {
		return true;
	}
	mCoreSyncLockAudio(&m_context->impl->sync);
	adjustResampler();
	mAudioResamplerProcess(&m_resampler);
	int available = mAudioBufferAvailable(&m_buffer);
	mCoreSyncUnlockAudio(&m_context->impl->sync);
	return available * sizeof(mStereoSample);
}

void AudioDevice::adjustResampler() {
	mCore* core = m_context->core;
	double fauxClock = mCoreCalculateFramerateRatio(m_context->core, m_context->impl->sync.fpsTarget);
	mAudioResamplerSetDestination(&m_resampler, &m_buffer, m_format.sampleRate() * fauxClock);
	m_context->impl->sync.audioHighWater = m_samples + m_resampler.highWaterMark + m_resampler.lowWaterMark;
	m_context->impl->sync.audioHighWater *= core->audioSampleRate(core) / (m_format.sampleRate() * fauxClock);
}
