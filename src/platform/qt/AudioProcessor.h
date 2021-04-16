/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QObject>

#include <memory>

#include "CoreController.h"

struct mCoreThread;

namespace QGBA {

class ConfigController;

class AudioProcessor : public QObject {
Q_OBJECT

public:
	enum class Driver {
		QT_MULTIMEDIA = 0,
		SDL = 1,
	};

	static AudioProcessor* create();
	static void setDriver(Driver driver) { s_driver = driver; }

	AudioProcessor(QObject* parent = nullptr);
	~AudioProcessor();

	int getBufferSamples() const { return m_samples; }
	virtual unsigned sampleRate() const = 0;

	virtual void configure(ConfigController*);

public slots:
	virtual void setInput(std::shared_ptr<CoreController>);
	virtual void stop();

	virtual bool start() = 0;
	virtual void pause() = 0;

	virtual void setBufferSamples(int samples) = 0;
	virtual void inputParametersChanged() = 0;

	virtual void requestSampleRate(unsigned) = 0;

protected:
	mCoreThread* input() { return m_context ? m_context->thread() : nullptr; }

private:
	std::shared_ptr<CoreController> m_context;
	int m_samples = 2048;
	static Driver s_driver;
};

}
