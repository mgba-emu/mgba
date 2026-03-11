/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MessagePainter.h"

#include "GBAApp.h"

#include <algorithm>

#include <QPainter>
#include <QStringList>

#include <mgba/gba/interface.h>

using namespace QGBA;

static QString _activeButtonLabel(int activeKeys) {
	if (!activeKeys) {
		return QStringLiteral("None");
	}

	static const struct {
		int key;
		const char* name;
	} keyNames[] = {
		{ GBA_KEY_A, "A" },
		{ GBA_KEY_B, "B" },
		{ GBA_KEY_L, "L" },
		{ GBA_KEY_R, "R" },
		{ GBA_KEY_START, "Start" },
		{ GBA_KEY_SELECT, "Select" },
		{ GBA_KEY_UP, "Up" },
		{ GBA_KEY_DOWN, "Down" },
		{ GBA_KEY_LEFT, "Left" },
		{ GBA_KEY_RIGHT, "Right" },
	};

	QStringList labels;
	for (const auto& keyName : keyNames) {
		if (activeKeys & (1 << keyName.key)) {
			labels.append(QString::fromLatin1(keyName.name));
		}
	}
	if (labels.isEmpty()) {
		return QStringLiteral("None");
	}
	return labels.join(QStringLiteral(" + "));
}

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
		QString fpsLine(tr("FPS %1").arg(QString::number(m_fps, 'f', 1)));
		QString buttonLine(tr("Button %1").arg(_activeButtonLabel(m_activeKeys)));
		QFontMetrics metrics(m_frameFont);
		int maxWidth = 0;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
		maxWidth = std::max(metrics.horizontalAdvance(fpsLine), metrics.horizontalAdvance(buttonLine));
#else
		maxWidth = std::max(metrics.width(fpsLine), metrics.width(buttonLine));
#endif
		const int lineHeight = metrics.height();

		painter->setWorldTransform(m_world);
		painter->setRenderHint(QPainter::Antialiasing);
		painter->setFont(m_frameFont);
		painter->setPen(Qt::black);
		painter->translate(-maxWidth, 0);
		const static int ITERATIONS = 11;
		for (int i = 0; i < ITERATIONS; ++i) {
			painter->save();
			painter->translate(cos(i * 2.0 * M_PI / ITERATIONS) * 0.8, sin(i * 2.0 * M_PI / ITERATIONS) * 0.8);
			painter->drawText(m_framePoint, fpsLine);
			painter->drawText(m_framePoint + QPointF(0, lineHeight), buttonLine);
			painter->restore();
		}
		painter->setPen(Qt::white);
		painter->drawText(m_framePoint, fpsLine);
		painter->drawText(m_framePoint + QPointF(0, lineHeight), buttonLine);
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

void MessagePainter::showFrameCounter(uint64_t frameCounter, int activeKeys) {
	m_mutex.lock();
	if (!m_fpsTimer.isValid()) {
		m_fpsTimer.start();
		m_fps = 0.0f;
	} else {
		qint64 elapsedMs = m_fpsTimer.elapsed();
		if (elapsedMs >= 250) {
			uint64_t deltaFrames = frameCounter - m_frameCounter;
			m_fps = deltaFrames > 0 ? (deltaFrames * 1000.f) / elapsedMs : 0.0f;
			m_fpsTimer.restart();
		}
	}
	m_frameCounter = frameCounter;
	m_activeKeys = activeKeys;
	m_drawFrameCounter = true;
	m_mutex.unlock();
}

void MessagePainter::clearFrameCounter() {
	m_mutex.lock();
	m_drawFrameCounter = false;
	m_fps = 0.0f;
	m_activeKeys = 0;
	m_fpsTimer.invalidate();
	m_mutex.unlock();
}
