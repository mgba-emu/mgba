/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DisplayQt.h"

#include <QPainter>

using namespace QGBA;

DisplayQt::DisplayQt(QWidget* parent)
	: Display(parent)
	, m_backing(nullptr)
	, m_lockAspectRatio(false)
	, m_filter(false)
{
}

void DisplayQt::startDrawing(GBAThread*) {
}

void DisplayQt::lockAspectRatio(bool lock) {
	m_lockAspectRatio = lock;
	update();
}

void DisplayQt::filter(bool filter) {
	m_filter = filter;
	update();
}

void DisplayQt::framePosted(const uint32_t* buffer) {
	update();
	if (const_cast<const QImage&>(m_backing).bits() == reinterpret_cast<const uchar*>(buffer)) {
		return;
	}
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	m_backing = QImage(reinterpret_cast<const uchar*>(buffer), 256, 256, QImage::Format_RGB16);
#else
	m_backing = QImage(reinterpret_cast<const uchar*>(buffer), 256, 256, QImage::Format_RGB555);
#endif
#else
	m_backing = QImage(reinterpret_cast<const uchar*>(buffer), 256, 256, QImage::Format_RGB32);
#endif
}

void DisplayQt::showMessage(const QString& message) {
	m_messagePainter.showMessage(message);
}

void DisplayQt::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	painter.fillRect(QRect(QPoint(), size()), Qt::black);
	if (m_filter) {
		painter.setRenderHint(QPainter::SmoothPixmapTransform);
	}
	QSize s = size();
	QSize ds = s;
	if (m_lockAspectRatio) {
		if (s.width() * 2 > s.height() * 3) {
			ds.setWidth(s.height() * 3 / 2);
		} else if (s.width() * 2 < s.height() * 3) {
			ds.setHeight(s.width() * 2 / 3);
		}
	}
	QPoint origin = QPoint((s.width() - ds.width()) / 2, (s.height() - ds.height()) / 2);
	QRect full(origin, ds);

#ifdef COLOR_5_6_5
	painter.drawImage(full, m_backing, QRect(0, 0, 240, 160));
#else
	painter.drawImage(full, m_backing.rgbSwapped(), QRect(0, 0, 240, 160));
#endif
	m_messagePainter.paint(&painter);
}

void DisplayQt::resizeEvent(QResizeEvent*) {
	m_messagePainter.resize(size(), m_lockAspectRatio);
}
