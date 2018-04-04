/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "AudioProcessor.h"

class QAudioOutput;

namespace QGBA {

class AudioDevice;

class AudioProcessorQt : public AudioProcessor {
Q_OBJECT

public:
	AudioProcessorQt(QObject* parent = nullptr);

	virtual unsigned sampleRate() const override;

public slots:
	virtual void setInput(std::shared_ptr<CoreController> input) override;
	virtual void stop() override;
	virtual bool start() override;
	virtual void pause() override;

	virtual void setBufferSamples(int samples) override;
	virtual void inputParametersChanged() override;

	virtual void requestSampleRate(unsigned) override;

private:
	QAudioOutput* m_audioOutput = nullptr;
	std::unique_ptr<AudioDevice> m_device;
	unsigned m_sampleRate = 44100;
};

}
