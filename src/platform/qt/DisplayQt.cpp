/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DisplayQt.h"

#include <QPainter>

extern "C" {
#include "gba/video.h"
}

using namespace QGBA;

DisplayQt::DisplayQt(QWidget* parent)
	: Display(parent)
	, m_isDrawing(false)
	, m_backing(nullptr)
{
}

void DisplayQt::startDrawing(GBAThread*) {
	m_isDrawing = true;
}

void DisplayQt::lockAspectRatio(bool lock) {
	Display::lockAspectRatio(lock);
	update();
}

void DisplayQt::filter(bool filter) {
	Display::filter(filter);
	update();
}

void DisplayQt::framePosted(const uint32_t* buffer) {
	update();
	if (const_cast<const QImage&>(m_backing).bits() == reinterpret_cast<const uchar*>(buffer)) {
		return;
	}
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	m_backing = QImage(reinterpret_cast<const uchar*>(buffer), VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, QImage::Format_RGB16);
#else
	m_backing = QImage(reinterpret_cast<const uchar*>(buffer), VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, QImage::Format_RGB555);
#endif
#else
	m_backing = QImage(reinterpret_cast<const uchar*>(buffer), VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, QImage::Format_RGB32);
#endif
}

void DisplayQt::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	painter.fillRect(QRect(QPoint(), size()), Qt::black);
	if (isFiltered()) {
		painter.setRenderHint(QPainter::SmoothPixmapTransform);
	}
	QSize s = size();
	QSize ds = s;
	if (isAspectRatioLocked()) {
		if (s.width() * 2 > s.height() * 3) {
			ds.setWidth(s.height() * 3 / 2);
		} else if (s.width() * 2 < s.height() * 3) {
			ds.setHeight(s.width() * 2 / 3);
		}
	}
	QPoint origin = QPoint((s.width() - ds.width()) / 2, (s.height() - ds.height()) / 2);
	QRect full(origin, ds);

#ifdef COLOR_5_6_5
	painter.drawImage(full, m_backing, QRect(0, 0, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS));
#else
	painter.drawImage(full, m_backing.rgbSwapped(), QRect(0, 0, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS));
#endif
	messagePainter()->paint(&painter);
}
