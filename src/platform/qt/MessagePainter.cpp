/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MessagePainter.h"

#include <QPainter>

#include <QDebug>

#include <mgba/internal/gba/video.h>

using namespace QGBA;

MessagePainter::MessagePainter(QObject* parent)
	: QObject(parent)
{
	m_messageFont.setFamily("Source Code Pro");
	m_messageFont.setStyleHint(QFont::Monospace);
	m_messageFont.setPixelSize(13);
	connect(&m_messageTimer, &QTimer::timeout, this, &MessagePainter::clearMessage);
	m_messageTimer.setSingleShot(true);
	m_messageTimer.setInterval(5000);

	clearMessage();
}

void MessagePainter::resize(const QSize& size, bool lockAspectRatio, qreal scaleFactor) {
	int w = size.width();
	int h = size.height();
	int drawW = w;
	int drawH = h;
	if (lockAspectRatio) {
		if (w * 2 > h * 3) {
			drawW = h * 3 / 2;
		} else if (w * 2 < h * 3) {
			drawH = w * 2 / 3;
		}
	}
	m_world.reset();
	m_world.scale(qreal(drawW) / VIDEO_HORIZONTAL_PIXELS, qreal(drawH) / VIDEO_VERTICAL_PIXELS);
	m_scaleFactor = scaleFactor;
	m_local = QPoint(1, VIDEO_VERTICAL_PIXELS - m_messageFont.pixelSize() - 1);
	m_local = m_world.map(m_local);
	m_local += QPoint((w - drawW) / 2, (h - drawH) / 2);
	m_pixmapBuffer = QPixmap(drawW * m_scaleFactor,
		                     (m_messageFont.pixelSize() + 2) * m_world.m22() * m_scaleFactor);
	m_pixmapBuffer.setDevicePixelRatio(m_scaleFactor);
	m_mutex.lock();
	m_message.prepare(m_world, m_messageFont);
	redraw();
	m_mutex.unlock();
}

void MessagePainter::redraw() {
	m_pixmapBuffer.fill(Qt::transparent);
	if (m_message.text().isEmpty()) {
		m_pixmap = m_pixmapBuffer;
		m_pixmap.setDevicePixelRatio(m_scaleFactor);
		return;
	}
	QPainter painter(&m_pixmapBuffer);
	painter.setWorldTransform(m_world);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setFont(m_messageFont);
	painter.setPen(Qt::black);
	const static int ITERATIONS = 11;
	for (int i = 0; i < ITERATIONS; ++i) {
		painter.save();
		painter.translate(cos(i * 2.0 * M_PI / ITERATIONS) * 0.8, sin(i * 2.0 * M_PI / ITERATIONS) * 0.8);
		painter.drawStaticText(0, 0, m_message);
		painter.restore();
	}
	painter.setPen(Qt::white);
	painter.drawStaticText(0, 0, m_message);
	m_pixmap = m_pixmapBuffer;
	m_pixmap.setDevicePixelRatio(m_scaleFactor);
}

void MessagePainter::paint(QPainter* painter) {
	painter->drawPixmap(m_local, m_pixmap);
}


void MessagePainter::showMessage(const QString& message) {
	m_mutex.lock();
	m_message.setText(message);
	redraw();
	m_mutex.unlock();
	m_messageTimer.stop();
	m_messageTimer.start();
}

void MessagePainter::clearMessage() {
	m_mutex.lock();
	m_message.setText(QString());
	redraw();
	m_mutex.unlock();
	m_messageTimer.stop();
}
