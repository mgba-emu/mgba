/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GIFView.h"

#ifdef USE_MAGICK

#include "GBAApp.h"
#include "LogController.h"

#include <QMap>

using namespace QGBA;

GIFView::GIFView(QWidget* parent)
	: QWidget(parent)
{
	m_ui.setupUi(this);

	connect(m_ui.buttonBox, SIGNAL(rejected()), this, SLOT(close()));
	connect(m_ui.start, SIGNAL(clicked()), this, SLOT(startRecording()));
	connect(m_ui.stop, SIGNAL(clicked()), this, SLOT(stopRecording()));

	connect(m_ui.selectFile, SIGNAL(clicked()), this, SLOT(selectFile()));
	connect(m_ui.filename, SIGNAL(textChanged(const QString&)), this, SLOT(setFilename(const QString&)));

	ImageMagickGIFEncoderInit(&m_encoder);
}

GIFView::~GIFView() {
	stopRecording();
}

void GIFView::startRecording() {
	if (!ImageMagickGIFEncoderOpen(&m_encoder, m_filename.toUtf8().constData())) {
		LOG(ERROR) << tr("Failed to open output GIF file: %1").arg(m_filename);
		return;
	}
	m_ui.start->setEnabled(false);
	m_ui.stop->setEnabled(true);
	emit recordingStarted(&m_encoder.d);
}

void GIFView::stopRecording() {
	emit recordingStopped();
	ImageMagickGIFEncoderClose(&m_encoder);
	m_ui.stop->setEnabled(false);
	m_ui.start->setEnabled(true);
}

void GIFView::selectFile() {
	QString filename = GBAApp::app()->getSaveFileName(this, tr("Select output file"), tr("Graphics Interchange Format (*.gif)"));
	if (!filename.isEmpty()) {
		m_ui.filename->setText(filename);
		if (!ImageMagickGIFEncoderIsOpen(&m_encoder)) {
			m_ui.start->setEnabled(true);
		}
	}
}

void GIFView::setFilename(const QString& fname) {
	m_filename = fname;
}

#endif
