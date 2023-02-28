/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DisplayQt.h"

#include "CoreController.h"
#include "utils.h"

#include <QPainter>

#include <mgba/core/core.h>
#include <mgba/core/thread.h>
#include <mgba-util/math.h>

using namespace QGBA;

DisplayQt::DisplayQt(QWidget* parent)
	: Display(parent)
{
}

void DisplayQt::startDrawing(std::shared_ptr<CoreController> controller) {
	QSize size = controller->screenDimensions();
	m_width = size.width();
	m_height = size.height();
	m_backing = QImage();
	m_oldBacking = QImage();
	m_isDrawing = true;
	m_context = controller;
	emit drawingStarted();
}

void DisplayQt::stopDrawing() {
	m_isDrawing = false;
	m_context.reset();
}

void DisplayQt::lockAspectRatio(bool lock) {
	Display::lockAspectRatio(lock);
	update();
}

void DisplayQt::lockIntegerScaling(bool lock) {
	Display::lockIntegerScaling(lock);
	update();
}

void DisplayQt::interframeBlending(bool lock) {
	Display::interframeBlending(lock);
	update();
}

void DisplayQt::filter(bool filter) {
	Display::filter(filter);
	update();
}

void DisplayQt::framePosted() {
	update();
	const color_t* buffer = m_context->drawContext();
	if (const_cast<const QImage&>(m_backing).bits() == reinterpret_cast<const uchar*>(buffer)) {
		return;
	}
	m_oldBacking = m_backing;
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
#ifndef COLOR_5_6_5
	m_backing = m_backing.rgbSwapped();
#endif
}

void DisplayQt::resizeContext() {
	if (!m_context) {
		return;
	}
	QSize size = m_context->screenDimensions();
	if (m_width != size.width() || m_height != size.height()) {
		m_width = size.width();
		m_height = size.height();
		m_oldBacking = QImage();
		m_backing = QImage();
	}
}

void DisplayQt::setBackgroundImage(const QImage& image) {
	m_background = image;
	update();
}

void DisplayQt::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	painter.fillRect(QRect(QPoint(), size()), Qt::black);
	if (isFiltered()) {
		painter.setRenderHint(QPainter::SmoothPixmapTransform);
	}

	QRect bgRect(0, 0, m_background.width(), m_background.height());
	QRect imRect(0, 0, m_width, m_height);
	QSize outerFrame = contentSize();

	if (bgRect.width() > imRect.width()) {
		imRect.moveLeft(bgRect.width() - imRect.width());
	} else {
		bgRect.moveLeft(imRect.width() - bgRect.width());
	}

	if (bgRect.height() > imRect.height()) {
		imRect.moveTop(bgRect.height() - imRect.height());
	} else {
		bgRect.moveTop(imRect.height() - bgRect.height());
	}

	QRect full(clampSize(outerFrame, size(), isAspectRatioLocked(), isIntegerScalingLocked()));

	if (m_background.isNull()) {
		imRect = full;
	} else {
		if (imRect.x()) {
			imRect.moveLeft(imRect.x() * full.width() / bgRect.width() / 2);
			imRect.setWidth(imRect.width() * full.width() / bgRect.width());
			bgRect.setWidth(full.width());
		} else {
			bgRect.moveLeft(bgRect.x() * full.width() / imRect.width() / 2);
			bgRect.setWidth(bgRect.width() * full.width() / imRect.width());
			imRect.setWidth(full.width());
		}
		if (imRect.y()) {
			imRect.moveTop(imRect.y() * full.height() / bgRect.height() / 2);
			imRect.setHeight(imRect.height() * full.height() / bgRect.height());
			bgRect.setHeight(full.height());
		} else {
			bgRect.moveTop(bgRect.y() * full.height() / imRect.height() / 2);
			bgRect.setHeight(bgRect.height() * full.height() / imRect.height());
			imRect.setHeight(full.height());
		}

		if (bgRect.right() > imRect.right()) {
			if (bgRect.right() < full.right()) {
				imRect.translate((full.right() - bgRect.right()), 0);
				bgRect.translate((full.right() - bgRect.right()), 0);
			}
		} else {
			if (imRect.right() < full.right()) {
				bgRect.translate((full.right() - imRect.right()), 0);
				imRect.translate((full.right() - imRect.right()), 0);
			}
		}

		if (bgRect.bottom() > imRect.bottom()) {
			if (bgRect.bottom() < full.bottom()) {
				imRect.translate(0, (full.bottom() - bgRect.bottom()));
				bgRect.translate(0, (full.bottom() - bgRect.bottom()));
			}
		} else {
			if (imRect.bottom() < full.bottom()) {
				bgRect.translate(0, (full.bottom() - imRect.bottom()));
				imRect.translate(0, (full.bottom() - imRect.bottom()));
			}
		}
		painter.drawImage(bgRect, m_background);
	}

	if (hasInterframeBlending()) {
		painter.drawImage(imRect, m_oldBacking, QRect(0, 0, m_width, m_height));
		painter.setOpacity(0.5);
	}
	painter.drawImage(imRect, m_backing, QRect(0, 0, m_width, m_height));
	painter.setOpacity(1);
	if (isShowOSD() || isShowFrameCounter()) {
		messagePainter()->paint(&painter);
	}
}

QSize DisplayQt::contentSize() const {
	QSize outerFrame(m_width, m_height);

	if (m_background.width() > outerFrame.width()) {
		outerFrame.setWidth(m_background.width());
	}
	if (m_background.height() > outerFrame.height()) {
		outerFrame.setHeight(m_background.height());
	}
	return outerFrame;
}
