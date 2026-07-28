/* Copyright (c) 2013-2026 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QElapsedTimer>
#include <QTimer>

#include "AudioProcessor.h"

namespace QGBA {

class AudioProcessorDummy : public AudioProcessor {
Q_OBJECT

private:
	const int POLL_INTERVAL = 4;

public:
	AudioProcessorDummy(QObject* parent = nullptr);

	virtual unsigned sampleRate() const override;

public slots:
	virtual void stop() override;

	virtual bool start() override;
	virtual void pause() override;

	virtual void setBufferSamples(int samples) override;
	virtual void inputParametersChanged() override;

	virtual void requestSampleRate(unsigned) override;

private slots:
	void refresh();

private:
	unsigned m_sampleRate = 44100;
	QElapsedTimer m_interval;
	QTimer m_timer;
	qint64 m_lastRefresh;
};

}
