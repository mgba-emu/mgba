/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AudioProcessorSDL.h"

#include "LogController.h"

#include <mgba/core/thread.h>

using namespace QGBA;

AudioProcessorSDL::AudioProcessorSDL(QObject* parent)
	: AudioProcessor(parent)
{
}

void AudioProcessorSDL::setInput(std::shared_ptr<CoreController> controller) {
	AudioProcessor::setInput(controller);
	if (m_audio.core && input()->core != m_audio.core) {
		mSDLDeinitAudio(&m_audio);
		mSDLInitAudio(&m_audio, input());
	}
}

void AudioProcessorSDL::stop() {
	mSDLDeinitAudio(&m_audio);
	AudioProcessor::stop();
}

bool AudioProcessorSDL::start() {
	if (!input()) {
		LOG(QT, WARN) << tr("Can't start an audio processor without input");
		return false;
	}

	if (m_audio.core) {
		mSDLResumeAudio(&m_audio);
		return true;
	} else {
		if (!m_audio.samples) {
			m_audio.samples = 2048; // TODO?
		}
		return mSDLInitAudio(&m_audio, input());
	}
}

void AudioProcessorSDL::pause() {
	mSDLPauseAudio(&m_audio);
}

void AudioProcessorSDL::setBufferSamples(int samples) {
	AudioProcessor::setBufferSamples(samples);
	if (m_audio.samples != static_cast<size_t>(samples)) {
		m_audio.samples = samples;
		if (m_audio.core) {
			mSDLDeinitAudio(&m_audio);
			mSDLInitAudio(&m_audio, input());
		}
	}
}

void AudioProcessorSDL::inputParametersChanged() {
}

void AudioProcessorSDL::requestSampleRate(unsigned rate) {
	if (m_audio.sampleRate != rate) {
		m_audio.sampleRate = rate;
		if (m_audio.core) {
			mSDLDeinitAudio(&m_audio);
			mSDLInitAudio(&m_audio, input());
		}
	}
}

unsigned AudioProcessorSDL::sampleRate() const {
	if (m_audio.core) {
		return m_audio.obtainedSpec.freq;
	} else {
		return 0;
	}
}
