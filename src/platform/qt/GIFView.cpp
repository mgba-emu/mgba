/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GIFView.h"

#ifdef USE_FFMPEG

#include "CoreController.h"
#include "GBAApp.h"
#include "LogController.h"

#include <QMap>

using namespace QGBA;

GIFView::GIFView(QWidget* parent)
	: QWidget(parent)
{
	m_ui.setupUi(this);

	connect(m_ui.start, &QAbstractButton::clicked, this, &GIFView::startRecording);
	connect(m_ui.stop, &QAbstractButton::clicked, this, &GIFView::stopRecording);

	connect(m_ui.selectFile, &QAbstractButton::clicked, this, &GIFView::selectFile);
	connect(m_ui.filename, &QLineEdit::textChanged, this, &GIFView::setFilename);
	connect(m_ui.fmtGif, &QAbstractButton::clicked, this, &GIFView::changeExtension);
	connect(m_ui.fmtApng, &QAbstractButton::clicked, this, &GIFView::changeExtension);
	connect(m_ui.fmtWebP, &QAbstractButton::clicked, this, &GIFView::changeExtension);

	FFmpegEncoderInit(&m_encoder);
	FFmpegEncoderSetAudio(&m_encoder, nullptr, 0);
}

GIFView::~GIFView() {
	stopRecording();
}

void GIFView::setController(std::shared_ptr<CoreController> controller) {
	connect(controller.get(), &CoreController::stopping, this, &GIFView::stopRecording);
	connect(this, &GIFView::recordingStarted, controller.get(), &CoreController::setAVStream);
	connect(this, &GIFView::recordingStopped, controller.get(), &CoreController::clearAVStream, Qt::DirectConnection);
	QSize size(controller->screenDimensions());
	FFmpegEncoderSetDimensions(&m_encoder, size.width(), size.height());
}

void GIFView::startRecording() {
	if (m_ui.fmtWebP->isChecked()) {
		FFmpegEncoderSetContainer(&m_encoder, "webp");
		FFmpegEncoderSetVideo(&m_encoder, "libwebp_anim", 0, m_ui.frameskip->value());
	} else if (m_ui.fmtApng->isChecked()) {
		FFmpegEncoderSetContainer(&m_encoder, "apng");
		FFmpegEncoderSetVideo(&m_encoder, "apng", 0, m_ui.frameskip->value());
	} else {
		FFmpegEncoderSetContainer(&m_encoder, "gif");
		FFmpegEncoderSetVideo(&m_encoder, "gif", 0, m_ui.frameskip->value());
	}
	FFmpegEncoderSetLooping(&m_encoder, m_ui.loop->isChecked());
	if (!FFmpegEncoderOpen(&m_encoder, m_filename.toUtf8().constData())) {
		LOG(QT, ERROR) << tr("Failed to open output file: %1").arg(m_filename);
		return;
	}
	m_ui.start->setEnabled(false);
	m_ui.stop->setEnabled(true);
	m_ui.frameskip->setEnabled(false);
	m_ui.loop->setEnabled(false);
	m_ui.fmtWebP->setEnabled(false);
	m_ui.fmtApng->setEnabled(false);
	m_ui.fmtGif->setEnabled(false);
	m_ui.fmtWebP->setEnabled(false);
	emit recordingStarted(&m_encoder.d);
}

void GIFView::stopRecording() {
	emit recordingStopped();
	FFmpegEncoderClose(&m_encoder);
	m_ui.stop->setEnabled(false);
	m_ui.start->setEnabled(!m_filename.isEmpty());
	m_ui.frameskip->setEnabled(true);
	m_ui.loop->setEnabled(true);
	m_ui.fmtWebP->setEnabled(true);
	m_ui.fmtApng->setEnabled(true);
	m_ui.fmtGif->setEnabled(true);
}

void GIFView::selectFile() {
	QString filename = GBAApp::app()->getSaveFileName(this, tr("Select output file"), tr("Graphics Interchange Format (*.gif);;WebP ( *.webp);;Animated Portable Network Graphics (*.png *.apng)"));
	if (!filename.isNull()) {
		m_ui.filename->setText(filename);
	}
}

void GIFView::setFilename(const QString& filename) {
	m_filename = filename;
	if (!FFmpegEncoderIsOpen(&m_encoder)) {
		m_ui.start->setEnabled(!filename.isEmpty());
		if (filename.endsWith(".gif")) {
			m_ui.fmtGif->setChecked(true);
		} else if (filename.endsWith(".png") || filename.endsWith(".apng")) {
			m_ui.fmtApng->setChecked(true);
		} else if (filename.endsWith(".webp")) {
			m_ui.fmtWebP->setChecked(true);
		}
	}
}

void GIFView::changeExtension() {
	if (m_filename.isEmpty()) {
		return;
	}
	QString filename = m_filename;
	int index = m_filename.lastIndexOf(".");
	if (index >= 0) {
		filename.truncate(index);
	}
	if (m_ui.fmtGif->isChecked()) {
		filename += ".gif";
	} else if (m_ui.fmtWebP->isChecked()) {
		filename += ".webp";
	} else if (m_ui.fmtApng->isChecked()) {
		filename += ".png";
	}
	m_ui.filename->setText(filename);
}

#endif
