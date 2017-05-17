/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_DISPLAY
#define QGBA_DISPLAY

#include <mgba-util/common.h>

#include <QWidget>

#include "MessagePainter.h"

struct mCoreThread;
struct VDir;
struct VideoShader;

namespace QGBA {

class Display : public QWidget {
Q_OBJECT

public:
	enum class Driver {
		QT = 0,
#if defined(BUILD_GL) || defined(BUILD_GLES2) || defined(USE_EPOXY)
		OPENGL = 1,
#endif
#ifdef BUILD_GL
		OPENGL1 = 2,
#endif
	};

	Display(QWidget* parent = nullptr);

	static Display* create(QWidget* parent = nullptr);
	static void setDriver(Driver driver) { s_driver = driver; }

	bool isAspectRatioLocked() const { return m_lockAspectRatio; }
	bool isIntegerScalingLocked() const { return m_lockIntegerScaling; }
	bool isFiltered() const { return m_filter; }

	virtual bool isDrawing() const = 0;
	virtual bool supportsShaders() const = 0;
	virtual VideoShader* shaders() = 0;

signals:
	void showCursor();
	void hideCursor();

public slots:
	virtual void startDrawing(mCoreThread* context) = 0;
	virtual void stopDrawing() = 0;
	virtual void pauseDrawing() = 0;
	virtual void unpauseDrawing() = 0;
	virtual void forceDraw() = 0;
	virtual void lockAspectRatio(bool lock);
	virtual void lockIntegerScaling(bool lock);
	virtual void filter(bool filter);
	virtual void framePosted(const uint32_t*) = 0;
	virtual void setShaders(struct VDir*) = 0;
	virtual void clearShaders() = 0;

	void showMessage(const QString& message);

protected:
	virtual void resizeEvent(QResizeEvent*) override;
	virtual void mouseMoveEvent(QMouseEvent*) override;

	MessagePainter* messagePainter() { return &m_messagePainter; }

private:
	static Driver s_driver;
	static const int MOUSE_DISAPPEAR_TIMER = 1000;

	MessagePainter m_messagePainter;
	bool m_lockAspectRatio = false;
	bool m_lockIntegerScaling = false;
	bool m_filter = false;
	QTimer m_mouseTimer;
};

}

#endif
