/* Copyright (c) 2013-2026 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AudioProcessorDummy.h"
#include "moc_AudioProcessorDummy.cpp"

#include "AudioDevice.h"
#include "LogController.h"

#include <mgba/core/core.h>
#include <mgba/core/sync.h>
#include <mgba/core/thread.h>

using namespace QGBA;

AudioProcessorDummy::AudioProcessorDummy(QObject* parent)
	: AudioProcessor(parent)
{
	m_timer.setInterval(AudioProcessorDummy::POLL_INTERVAL);
	connect(&m_timer, &QTimer::timeout, this, &AudioProcessorDummy::refresh);
}

void AudioProcessorDummy::stop() {
	m_timer.stop();
}

bool AudioProcessorDummy::start() {
	if (!input()) {
		LOG(QT, WARN) << tr("Can't start an audio processor without input");
		return false;
	}

	m_interval.start();
	m_lastRefresh = 0;
	m_timer.start();
	return true;
}

void AudioProcessorDummy::pause() {
	m_timer.stop();
}

void AudioProcessorDummy::setBufferSamples(int samples) {
	AudioProcessor::setBufferSamples(samples);
}

void AudioProcessorDummy::inputParametersChanged() {
}

void AudioProcessorDummy::requestSampleRate(unsigned rate) {
	m_sampleRate = rate;
}

unsigned AudioProcessorDummy::sampleRate() const {
	return m_sampleRate;
}

void AudioProcessorDummy::refresh() {
	struct mCoreThread* thread = input();
	if (!thread) {
		return;
	}

	struct mAudioBuffer* buffer = thread->core->getAudioBuffer(thread->core);
	double sampleRate = thread->core->audioSampleRate(thread->core);
	double second = 1'000'000'000;
	if (thread->impl->sync.fpsTarget > 0) {
		second *= mCoreCalculateFramerateRatio(thread->core, thread->impl->sync.fpsTarget);
	}

	qint64 totalElapsed = m_interval.nsecsElapsed();
	qint64 elapsed = totalElapsed - m_lastRefresh;
	m_lastRefresh = totalElapsed;

	mCoreSyncLockAudio(&thread->impl->sync);
	mAudioBufferRead(buffer, NULL, elapsed * sampleRate / second);
	mCoreSyncConsumeAudio(&thread->impl->sync);
}
