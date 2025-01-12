/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QAudioFormat>
#include <QIODevice>

#include <mgba-util/audio-buffer.h>
#include <mgba-util/audio-resampler.h>

struct mCoreThread;

namespace QGBA {

class AudioDevice : public QIODevice {
Q_OBJECT

public:
	AudioDevice(QObject* parent = nullptr);
	virtual ~AudioDevice();

	void setInput(mCoreThread* input);
	void setFormat(const QAudioFormat& format);
	void setBufferSamples(int samples);
	bool atEnd() const override;
	qint64 bytesAvailable() const override;
	qint64 bytesAvailable();
	bool isSequential() const override { return true; }

protected:
	virtual qint64 readData(char* data, qint64 maxSize) override;
	virtual qint64 writeData(const char* data, qint64 maxSize) override;

private:
	size_t m_samples = 512;
	QAudioFormat m_format;
	mCoreThread* m_context;
	mAudioBuffer m_buffer;
	mAudioResampler m_resampler;

	void adjustResampler();
};

}
