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

	void resize(const QSize& size, bool lockAspectRatio, qreal scaleFactor);
	void paint(QPainter* painter);
	void setScaleFactor(qreal factor);

public slots:
	void showMessage(const QString& message);
	void clearMessage();

private:
	void redraw();

	QMutex m_mutex;
	QStaticText m_message;
	QPixmap m_pixmap;
	QPixmap m_pixmapBuffer;
	QTimer m_messageTimer{this};
	QPoint m_local;
	QTransform m_world;
	QFont m_messageFont;
	qreal m_scaleFactor = 1;
};

}
