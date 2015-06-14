/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MessagePainter.h"

#include <QPainter>

#include <QDebug>

extern "C" {
#include "gba/video.h"
}

using namespace QGBA;

MessagePainter::MessagePainter(QObject* parent)
	: QObject(parent)
	, m_messageTimer(this)
{
	m_messageFont.setFamily("Source Code Pro");
	m_messageFont.setStyleHint(QFont::Monospace);
	m_messageFont.setPixelSize(13);
	connect(&m_messageTimer, SIGNAL(timeout()), this, SLOT(clearMessage()));
	m_messageTimer.setSingleShot(true);
	m_messageTimer.setInterval(5000);

	clearMessage();
}

void MessagePainter::resize(const QSize& size, bool lockAspectRatio) {
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
	m_world.translate((w - drawW) / 2, (h - drawH) / 2);
	m_world.scale(qreal(drawW) / VIDEO_HORIZONTAL_PIXELS, qreal(drawH) / VIDEO_VERTICAL_PIXELS);
	m_message.prepare(m_world, m_messageFont);
}

void MessagePainter::paint(QPainter* painter) {
	painter->setWorldTransform(m_world);
	painter->setRenderHint(QPainter::Antialiasing);
	painter->setFont(m_messageFont);
	painter->setPen(Qt::black);
	painter->translate(1, VIDEO_VERTICAL_PIXELS - m_messageFont.pixelSize() - 1);
	const static int ITERATIONS = 11;
	for (int i = 0; i < ITERATIONS; ++i) {
		painter->save();
		painter->translate(cos(i * 2.0 * M_PI / ITERATIONS) * 0.8, sin(i * 2.0 * M_PI / ITERATIONS) * 0.8);
		painter->drawStaticText(0, 0, m_message);
		painter->restore();
	}
	painter->setPen(Qt::white);
	painter->drawStaticText(0, 0, m_message);
}

void MessagePainter::showMessage(const QString& message) {
	m_message.setText(message);
	m_message.prepare(m_world, m_messageFont);
	m_messageTimer.stop();
	m_messageTimer.start();
}

void MessagePainter::clearMessage() {
	m_message.setText(QString());
	m_messageTimer.stop();
}
