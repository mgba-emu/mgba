/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QMutex>
#include <QObject>
#include <QPixmap>
#include <QStaticText>
#include <QTimer>

namespace QGBA {

class MessagePainter : public QObject {
Q_OBJECT

public:
	MessagePainter(QObject* parent = nullptr);

	void resize(const QSize& size, qreal scaleFactor);
	void paint(QPainter* painter);
	void setScaleFactor(qreal factor);

public slots:
	void showMessage(const QString& message);
	void clearMessage();

	void showFrameCounter(uint64_t);
	void clearFrameCounter();

private:
	void redraw();

	QMutex m_mutex;
	QStaticText m_message;
	qreal m_scaleFactor = 1;
	uint64_t m_frameCounter;
	bool m_drawFrameCounter = false;

	QPoint m_local;
	QPixmap m_pixmap;
	QPixmap m_pixmapBuffer;

	QPointF m_framePoint = QPointF(0, 0);
	QFont m_frameFont;

	QTimer m_messageTimer{this};
	QTransform m_world;
	QFont m_messageFont;
};

}
