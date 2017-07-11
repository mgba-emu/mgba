/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DisplayQt.h"

#include <QPainter>

#include <mgba/core/core.h>
#include <mgba/core/thread.h>

using namespace QGBA;

DisplayQt::DisplayQt(QWidget* parent)
	: Display(parent)
{
}

void DisplayQt::startDrawing(mCoreThread* context) {
	context->core->desiredVideoDimensions(context->core, &m_width, &m_height);
	m_backing = std::move(QImage());
	m_isDrawing = true;
}

void DisplayQt::lockAspectRatio(bool lock) {
	Display::lockAspectRatio(lock);
	update();
}

void DisplayQt::lockIntegerScaling(bool lock) {
	Display::lockIntegerScaling(lock);
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
	m_backing = QImage(reinterpret_cast<const uchar*>(buffer), m_width, m_height, QImage::Format_RGB16);
#else
	m_backing = QImage(reinterpret_cast<const uchar*>(buffer), m_width, m_height, QImage::Format_RGB555);
#endif
#else
	m_backing = QImage(reinterpret_cast<const uchar*>(buffer), m_width, m_height, QImage::Format_ARGB32);
	m_backing = m_backing.convertToFormat(QImage::Format_RGB32);
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
		if (s.width() * m_height > s.height() * m_width) {
			ds.setWidth(s.height() * m_width / m_height);
		} else if (s.width() * m_height < s.height() * m_width) {
			ds.setHeight(s.width() * m_height / m_width);
		}
	}
	if (isIntegerScalingLocked()) {
		ds.setWidth(ds.width() - ds.width() % m_width);
		ds.setHeight(ds.height() - ds.height() % m_height);
	}
	QPoint origin = QPoint((s.width() - ds.width()) / 2, (s.height() - ds.height()) / 2);
	QRect full(origin, ds);

#ifdef COLOR_5_6_5
	painter.drawImage(full, m_backing, QRect(0, 0, m_width, m_height));
#else
	painter.drawImage(full, m_backing.rgbSwapped(), QRect(0, 0, m_width, m_height));
#endif
	messagePainter()->paint(&painter);
}
