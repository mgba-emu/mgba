/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AudioProcessor.h"

#include "ConfigController.h"

#ifdef BUILD_SDL
#include "AudioProcessorSDL.h"
#endif

#ifdef BUILD_QT_MULTIMEDIA
#include "AudioProcessorQt.h"
#endif

using namespace QGBA;

#ifndef BUILD_SDL
AudioProcessor::Driver AudioProcessor::s_driver = AudioProcessor::Driver::QT_MULTIMEDIA;
#else
AudioProcessor::Driver AudioProcessor::s_driver = AudioProcessor::Driver::SDL;
#endif

AudioProcessor* AudioProcessor::create() {
	switch (s_driver) {
#ifdef BUILD_SDL
	case Driver::SDL:
		return new AudioProcessorSDL();
#endif

#ifdef BUILD_QT_MULTIMEDIA
	case Driver::QT_MULTIMEDIA:
		return new AudioProcessorQt();
#endif

	default:
#ifdef BUILD_SDL
		return new AudioProcessorSDL();
#else
		return new AudioProcessorQt();
#endif
	}
}

AudioProcessor::AudioProcessor(QObject* parent)
	: QObject(parent)
{
}

AudioProcessor::~AudioProcessor() {
	stop();
}

void AudioProcessor::configure(ConfigController* config) {
	const mCoreOptions* opts = config->options();
	setBufferSamples(opts->audioBuffers);
	requestSampleRate(opts->sampleRate);
}

void AudioProcessor::setInput(std::shared_ptr<CoreController> input) {
	m_context = input;
	connect(input.get(), &CoreController::stopping, this, &AudioProcessor::stop);
	connect(input.get(), &CoreController::fastForwardChanged, this, &AudioProcessor::inputParametersChanged);
	connect(input.get(), &CoreController::paused, this, &AudioProcessor::pause);
	connect(input.get(), &CoreController::unpaused, this, &AudioProcessor::start);
}

void AudioProcessor::stop() {
	m_context.reset();
}

void AudioProcessor::setBufferSamples(int samples) {
	m_samples = samples;
}
