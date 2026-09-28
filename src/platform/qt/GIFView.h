/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#ifdef USE_FFMPEG

#include <QWidget>

#include <memory>

#include "CorePointer.h"
#include "ui_GIFView.h"

#include "feature/ffmpeg/ffmpeg-encoder.h"

namespace QGBA {

class CoreController;

class GIFView : public QWidget, public CoreConsumer {
Q_OBJECT

public:
	GIFView(CorePointerSource* controller, QWidget* parent = nullptr);
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
	void changeExtension();

private:
	void onCoreAttached(std::shared_ptr<CoreController>);

	Ui::GIFView m_ui;

	FFmpegEncoder m_encoder;

	QString m_filename;
};

}

#endif
