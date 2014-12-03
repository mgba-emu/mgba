/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_AUDIO_PROCESSOR
#define QGBA_AUDIO_PROCESSOR
#include <QObject>

struct GBAThread;

namespace QGBA {

class AudioProcessor : public QObject {
Q_OBJECT

public:
	static AudioProcessor* create();
	AudioProcessor(QObject* parent = nullptr);

	virtual void setInput(GBAThread* input);

public slots:
	virtual void start() = 0;
	virtual void pause() = 0;

	virtual void setBufferSamples(int samples) = 0;
	virtual void inputParametersChanged() = 0;

protected:
	GBAThread* input() { return m_context; }
private:
	GBAThread* m_context;
};

}

#endif
