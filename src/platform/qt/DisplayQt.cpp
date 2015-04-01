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
	, m_lockAspectRatio(false)
	, m_filter(false)
{
	connect(&m_drawTimer, SIGNAL(timeout()), this, SLOT(update()));
	m_drawTimer.setInterval(12); // Give update time roughly 4.6ms of clearance
}

void DisplayQt::startDrawing(const uint32_t* buffer, GBAThread* context) {
	m_context = context;
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	m_backing = QImage(reinterpret_cast<const uchar*>(buffer), 256, 256, QImage::Format_RGB16);
#else
	m_backing = QImage(reinterpret_cast<const uchar*>(buffer), 256, 256, QImage::Format_RGB555);
#endif
#else
	m_backing = QImage(reinterpret_cast<const uchar*>(buffer), 256, 256, QImage::Format_RGB32);
#endif
	m_drawTimer.start();
}

void DisplayQt::stopDrawing() {
	m_drawTimer.stop();
}

void DisplayQt::pauseDrawing() {
	m_drawTimer.stop();
}

void DisplayQt::unpauseDrawing() {
	m_drawTimer.start();
}

void DisplayQt::forceDraw() {
	update();
}

void DisplayQt::lockAspectRatio(bool lock) {
	m_lockAspectRatio = lock;
	update();
}

void DisplayQt::filter(bool filter) {
	m_filter = filter;
	update();
}

void DisplayQt::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	painter.fillRect(QRect(QPoint(), size()), Qt::black);
	if (m_filter) {
		painter.setRenderHint(QPainter::SmoothPixmapTransform);
	}
	QSize s = size();
	QSize ds = s;
	if (s.width() * 2 > s.height() * 3) {
		ds.setWidth(s.height() * 3 / 2);
	} else if (s.width() * 2 < s.height() * 3) {
		ds.setHeight(s.width() * 2 / 3);
	}
	QPoint origin = QPoint((s.width() - ds.width()) / 2, (s.height() - ds.height()) / 2);
	QRect full(origin, ds);

#ifdef COLOR_5_6_5
	painter.drawImage(full, m_backing, QRect(0, 0, 240, 160));
#else
	painter.drawImage(full, m_backing.rgbSwapped(), QRect(0, 0, 240, 160));
#endif
}
