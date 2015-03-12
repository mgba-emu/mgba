/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AudioProcessorSDL.h"

extern "C" {
#include "gba/supervisor/thread.h"
}

using namespace QGBA;

AudioProcessorSDL::AudioProcessorSDL(QObject* parent)
	: AudioProcessor(parent)
	, m_audio()
{
}

AudioProcessorSDL::~AudioProcessorSDL() {
	GBASDLDeinitAudio(&m_audio);
}

void AudioProcessorSDL::start() {
	if (m_audio.thread) {
		GBASDLResumeAudio(&m_audio);
	} else {
		if (!m_audio.samples) {
			m_audio.samples = input()->audioBuffers;
		}
		GBASDLInitAudio(&m_audio, input());
	}
}

void AudioProcessorSDL::pause() {
	GBASDLPauseAudio(&m_audio);
}

void AudioProcessorSDL::setBufferSamples(int samples) {
	AudioProcessor::setBufferSamples(samples);
	m_audio.samples = samples;
	if (m_audio.thread) {
		GBASDLDeinitAudio(&m_audio);
		GBASDLInitAudio(&m_audio, input());
	}
}

void AudioProcessorSDL::inputParametersChanged() {
}
