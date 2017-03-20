/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_GIF_VIEW
#define QGBA_GIF_VIEW

#ifdef USE_MAGICK

#include <QWidget>

#include "ui_GIFView.h"

#include "feature/imagemagick/imagemagick-gif-encoder.h"

namespace QGBA {

class GIFView : public QWidget {
Q_OBJECT

public:
	GIFView(QWidget* parent = nullptr);
	virtual ~GIFView();

	mAVStream* getStream() { return &m_encoder.d; }

public slots:
	void startRecording();
	void stopRecording();

signals:
	void recordingStarted(mAVStream*);
	void recordingStopped();

private slots:
	void selectFile();
	void setFilename(const QString&);
	void updateDelay();

private:
	Ui::GIFView m_ui;

	ImageMagickGIFEncoder m_encoder;

	QString m_filename;
};

}

#endif

#endif
