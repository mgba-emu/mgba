/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_DISPLAY
#define QGBA_DISPLAY

#include <QWidget>

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

public slots:
	virtual void startDrawing(GBAThread* context) = 0;
	virtual void stopDrawing() = 0;
	virtual void pauseDrawing() = 0;
	virtual void unpauseDrawing() = 0;
	virtual void forceDraw() = 0;
	virtual void lockAspectRatio(bool lock) = 0;
	virtual void filter(bool filter) = 0;
	virtual void framePosted(const uint32_t*) = 0;

	virtual void showMessage(const QString& message) = 0;

private:
	static Driver s_driver;
};

}

#endif
