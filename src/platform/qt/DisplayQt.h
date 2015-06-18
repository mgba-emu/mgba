/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_DISPLAY_QT
#define QGBA_DISPLAY_QT

#include "Display.h"
#include "MessagePainter.h"

#include <QImage>
#include <QTimer>

struct GBAThread;

namespace QGBA {

class DisplayQt : public Display {
Q_OBJECT

public:
	DisplayQt(QWidget* parent = nullptr);

public slots:
	void startDrawing(GBAThread* context) override;
	void stopDrawing() override {}
	void pauseDrawing() override {}
	void unpauseDrawing() override {}
	void forceDraw() override { update(); }
	void lockAspectRatio(bool lock) override;
	void filter(bool filter) override;
	void framePosted(const uint32_t*) override;

	void showMessage(const QString& message) override;

protected:
	virtual void paintEvent(QPaintEvent*) override;
	virtual void resizeEvent(QResizeEvent*) override;;

private:
	QImage m_backing;
	bool m_lockAspectRatio;
	bool m_filter;
	MessagePainter m_messagePainter;
};

}

#endif
