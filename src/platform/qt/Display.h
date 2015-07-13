/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_DISPLAY
#define QGBA_DISPLAY

#include <QWidget>

#include "MessagePainter.h"

struct GBAThread;

namespace QGBA {

class Display : public QWidget {
Q_OBJECT

public:
	enum class Driver {
		QT = 0,
#ifdef BUILD_GL
		OPENGL = 1,
#endif
	};

	Display(QWidget* parent = nullptr);

	static Display* create(QWidget* parent = nullptr);
	static void setDriver(Driver driver) { s_driver = driver; }

	bool isAspectRatioLocked() const { return m_lockAspectRatio; }
	bool isFiltered() const { return m_filter; }

signals:
	void showCursor();
	void hideCursor();

public slots:
	virtual void startDrawing(GBAThread* context) = 0;
	virtual void stopDrawing() = 0;
	virtual void pauseDrawing() = 0;
	virtual void unpauseDrawing() = 0;
	virtual void forceDraw() = 0;
	virtual void lockAspectRatio(bool lock);
	virtual void filter(bool filter);
	virtual void framePosted(const uint32_t*) = 0;

	void showMessage(const QString& message);

protected:
	void resizeEvent(QResizeEvent*);
	virtual void mouseMoveEvent(QMouseEvent*) override;

	MessagePainter* messagePainter() { return &m_messagePainter; }


private:
	static Driver s_driver;
	static const int MOUSE_DISAPPEAR_TIMER = 2000;

	MessagePainter m_messagePainter;
	bool m_lockAspectRatio;
	bool m_filter;
	QTimer m_mouseTimer;
};

}

#endif
