/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_DISPLAY_GL
#define QGBA_DISPLAY_GL

#include "Display.h"

#include <QGLWidget>
#include <QMouseEvent>
#include <QThread>
#include <QTimer>

extern "C" {
#ifdef BUILD_GL
#include "platform/opengl/gl.h"
#elif defined(BUILD_GLES2)
#include "platform/opengl/gles2.h"
#endif
}

struct GBAThread;

namespace QGBA {

class EmptyGLWidget : public QGLWidget {
public:
	EmptyGLWidget(const QGLFormat& format, QWidget* parent) : QGLWidget(format, parent) { setAutoBufferSwap(false); }

protected:
	void paintEvent(QPaintEvent*) override {}
	void resizeEvent(QResizeEvent*) override {}
	void mouseMoveEvent(QMouseEvent* event) override { event->ignore(); }
};

class PainterGL;
class DisplayGL : public Display {
Q_OBJECT

public:
	DisplayGL(const QGLFormat& format, QWidget* parent = nullptr);
	~DisplayGL();

	bool isDrawing() const override { return m_isDrawing; }

public slots:
	void startDrawing(GBAThread* context) override;
	void stopDrawing() override;
	void pauseDrawing() override;
	void unpauseDrawing() override;
	void forceDraw() override;
	void lockAspectRatio(bool lock) override;
	void filter(bool filter) override;
	void framePosted(const uint32_t*) override;

protected:
	virtual void paintEvent(QPaintEvent*) override {}
	virtual void resizeEvent(QResizeEvent*) override;

private:
	void resizePainter();

	bool m_isDrawing;
	QGLWidget* m_gl;
	PainterGL* m_painter;
	QThread* m_drawThread;
	GBAThread* m_context;
};

class PainterGL : public QObject {
Q_OBJECT

public:
	PainterGL(QGLWidget* parent);

	void setContext(GBAThread*);
	void setMessagePainter(MessagePainter*);

public slots:
	void setBacking(const uint32_t*);
	void forceDraw();
	void draw();
	void start();
	void stop();
	void pause();
	void unpause();
	void resize(const QSize& size);
	void lockAspectRatio(bool lock);
	void filter(bool filter);

private:
	void performDraw();

	QPainter m_painter;
	QGLWidget* m_gl;
	bool m_active;
	GBAThread* m_context;
#ifdef BUILD_GL
	GBAGLContext m_backend;
#elif defined(BUILD_GLES2)
	GBAGLES2Context m_backend;
#endif
	QSize m_size;
	MessagePainter* m_messagePainter;
};

}

#endif
