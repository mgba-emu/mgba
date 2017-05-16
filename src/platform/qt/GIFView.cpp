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

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/video.h>

using namespace QGBA;

GIFView::GIFView(QWidget* parent)
	: QWidget(parent)
{
	m_ui.setupUi(this);

	connect(m_ui.start, &QAbstractButton::clicked, this, &GIFView::startRecording);
	connect(m_ui.stop, &QAbstractButton::clicked, this, &GIFView::stopRecording);

	connect(m_ui.selectFile, &QAbstractButton::clicked, this, &GIFView::selectFile);
	connect(m_ui.filename, &QLineEdit::textChanged, this, &GIFView::setFilename);

	connect(m_ui.frameskip, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
	        this, &GIFView::updateDelay);
	connect(m_ui.delayAuto, &QAbstractButton::clicked, this, &GIFView::updateDelay);

	ImageMagickGIFEncoderInit(&m_encoder);
}

GIFView::~GIFView() {
	stopRecording();
}

void GIFView::startRecording() {
	int delayMs = m_ui.delayAuto->isChecked() ? -1 : m_ui.delayMs->value();
	ImageMagickGIFEncoderSetParams(&m_encoder, m_ui.frameskip->value(), delayMs);
	if (!ImageMagickGIFEncoderOpen(&m_encoder, m_filename.toUtf8().constData())) {
		LOG(QT, ERROR) << tr("Failed to open output GIF file: %1").arg(m_filename);
		return;
	}
	m_ui.start->setEnabled(false);
	m_ui.stop->setEnabled(true);
	m_ui.groupBox->setEnabled(false);
	emit recordingStarted(&m_encoder.d);
}

void GIFView::stopRecording() {
	emit recordingStopped();
	ImageMagickGIFEncoderClose(&m_encoder);
	m_ui.stop->setEnabled(false);
	m_ui.start->setEnabled(true);
	m_ui.groupBox->setEnabled(true);
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

void GIFView::updateDelay() {
	if (!m_ui.delayAuto->isChecked()) {
		return;
	}

	uint64_t s = (m_ui.frameskip->value() + 1);
	s *= VIDEO_TOTAL_LENGTH * 1000;
	s /= GBA_ARM7TDMI_FREQUENCY;
	m_ui.delayMs->setValue(s);
}

#endif
