/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QAudioFormat>
#include <QIODevice>

struct mCoreThread;

namespace QGBA {

class AudioDevice : public QIODevice {
Q_OBJECT

public:
	AudioDevice(QObject* parent = nullptr);

	void setInput(mCoreThread* input);
	void setFormat(const QAudioFormat& format);

protected:
	virtual qint64 readData(char* data, qint64 maxSize) override;
	virtual qint64 writeData(const char* data, qint64 maxSize) override;

private:
	mCoreThread* m_context;
};

}
