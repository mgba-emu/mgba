/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#ifdef USE_FFMPEG

#include <QWidget>

#include <memory>

#include "ui_GIFView.h"

#include "feature/ffmpeg/ffmpeg-encoder.h"

namespace QGBA {

class CoreController;

class GIFView : public QWidget {
Q_OBJECT

public:
	GIFView(QWidget* parent = nullptr);
	virtual ~GIFView();

	mAVStream* getStream() { return &m_encoder.d; }

public slots:
	void setController(std::shared_ptr<CoreController>);

	void startRecording();
	void stopRecording();

signals:
	void recordingStarted(mAVStream*);
	void recordingStopped();

private slots:
	void selectFile();
	void setFilename(const QString&);
	void changeExtension();

private:
	Ui::GIFView m_ui;

	FFmpegEncoder m_encoder;

	QString m_filename;
};

}

#endif
