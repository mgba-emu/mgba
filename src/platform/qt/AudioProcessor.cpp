/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AudioProcessor.h"

#ifdef BUILD_SDL
#include "AudioProcessorSDL.h"
#else
#include "AudioProcessorQt.h"
#endif

extern "C" {
#include "gba-thread.h"
}

using namespace QGBA;

AudioProcessor* AudioProcessor::create() {
#ifdef BUILD_SDL
	return new AudioProcessorSDL();
#else
	return new AudioProcessorQt();
#endif	
}

AudioProcessor::AudioProcessor(QObject* parent)
	: QObject(parent)
{
}

void AudioProcessor::setInput(GBAThread* input) {
	m_context = input;
}
