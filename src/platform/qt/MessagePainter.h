/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_MESSAGE_PAINTER
#define QGBA_MESSAGE_PAINTER

#include <QObject>
#include <QStaticText>
#include <QTimer>

namespace QGBA {

class MessagePainter : public QObject {
Q_OBJECT

public:
	MessagePainter(QObject* parent = nullptr);

	void resize(const QSize& size, bool lockAspectRatio);
	void paint(QPainter* painter);

public slots:
	void showMessage(const QString& message);
	void clearMessage();

private:
	QStaticText m_message;
	QTimer m_messageTimer;
	QTransform m_world;
	QFont m_messageFont;
};

}

#endif
