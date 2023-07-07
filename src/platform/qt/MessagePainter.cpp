/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MessagePainter.h"

#include "GBAApp.h"

#include <QPainter>

#include <mgba/gba/interface.h>

using namespace QGBA;

MessagePainter::MessagePainter(QObject* parent)
	: QObject(parent)
{
	m_messageFont = GBAApp::app()->monospaceFont();
	m_messageFont.setPixelSize(13);
	m_frameFont = GBAApp::app()->monospaceFont();
	m_frameFont.setPixelSize(10);
	connect(&m_messageTimer, &QTimer::timeout, this, &MessagePainter::clearMessage);
	m_messageTimer.setSingleShot(true);
	m_messageTimer.setInterval(5000);

	clearMessage();
}

void MessagePainter::resize(const QSize& size, qreal scaleFactor) {
	double drawW = size.width();
	double drawH = size.height();
	double area = pow(drawW * drawW * drawW * drawH * drawH, 0.185);
	m_scaleFactor = scaleFactor;
	m_world.reset();
	m_world.scale(area / 170., area / 170.);
	m_local = QPoint(area / 80., drawH - m_messageFont.pixelSize() * m_world.m22() * 1.3);

	QFontMetrics metrics(m_frameFont);
	m_framePoint = QPoint(drawW / m_world.m11() - metrics.height() * 0.1, metrics.height() * 0.75);

	m_mutex.lock();
	redraw();
	m_mutex.unlock();
}

void MessagePainter::redraw() {
	if (m_message.text().isEmpty()) {
		m_pixmapBuffer.fill(Qt::transparent);
		m_pixmap = m_pixmapBuffer;
		return;
	}
	m_message.prepare(m_world, m_messageFont);
	QSizeF sizef = m_message.size() * m_scaleFactor;
	m_pixmapBuffer = QPixmap(sizef.width() * m_world.m11(), sizef.height() * m_world.m22());
	m_pixmapBuffer.setDevicePixelRatio(m_scaleFactor);
	m_pixmapBuffer.fill(Qt::transparent);

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
}

void MessagePainter::paint(QPainter* painter) {
	if (!m_message.text().isEmpty()) {
		painter->drawPixmap(m_local, m_pixmap);
	}
	if (m_drawFrameCounter) {
		QString frame(tr("Frame %1").arg(m_frameCounter));
		QFontMetrics metrics(m_frameFont);
		painter->setWorldTransform(m_world);
		painter->setRenderHint(QPainter::Antialiasing);
		painter->setFont(m_frameFont);
		painter->setPen(Qt::black);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
		painter->translate(-metrics.horizontalAdvance(frame), 0);
#else
		painter->translate(-metrics.width(frame), 0);
#endif
		const static int ITERATIONS = 11;
		for (int i = 0; i < ITERATIONS; ++i) {
			painter->save();
			painter->translate(cos(i * 2.0 * M_PI / ITERATIONS) * 0.8, sin(i * 2.0 * M_PI / ITERATIONS) * 0.8);
			painter->drawText(m_framePoint, frame);
			painter->restore();
		}
		painter->setPen(Qt::white);
		painter->drawText(m_framePoint, frame);
	}
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

void MessagePainter::showFrameCounter(uint64_t frameCounter) {
	m_mutex.lock();
	m_frameCounter = frameCounter;
	m_drawFrameCounter = true;
	m_mutex.unlock();
}

void MessagePainter::clearFrameCounter() {
	m_mutex.lock();
	m_drawFrameCounter = false;
	m_mutex.unlock();
}
