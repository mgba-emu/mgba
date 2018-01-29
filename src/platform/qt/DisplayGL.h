/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_DISPLAY_GL
#define QGBA_DISPLAY_GL

#if defined(BUILD_GL) || defined(BUILD_GLES2)

#include "Display.h"

#ifdef USE_EPOXY
#include <epoxy/gl.h>
#ifndef GLdouble
#define GLdouble GLdouble
#endif
#endif

#include <QElapsedTimer>
#include <QGLWidget>
#include <QList>
#include <QMouseEvent>
#include <QQueue>
#include <QThread>

#include "platform/video-backend.h"

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
	bool supportsShaders() const override;
	VideoShader* shaders() override;

public slots:
	void startDrawing(mCoreThread* context) override;
	void stopDrawing() override;
	void pauseDrawing() override;
	void unpauseDrawing() override;
	void forceDraw() override;
	void lockAspectRatio(bool lock) override;
	void lockIntegerScaling(bool lock) override;
	void filter(bool filter) override;
	void framePosted(const uint32_t*) override;
	void setShaders(struct VDir*) override;
	void clearShaders() override;

protected:
	virtual void paintEvent(QPaintEvent*) override {}
	virtual void resizeEvent(QResizeEvent*) override;

private:
	void resizePainter();

	bool m_isDrawing = false;
	QGLWidget* m_gl;
	PainterGL* m_painter;
	QThread* m_drawThread = nullptr;
	mCoreThread* m_context = nullptr;
};

class PainterGL : public QObject {
Q_OBJECT

public:
	PainterGL(int majorVersion, QGLWidget* parent);
	~PainterGL();

	void setContext(mCoreThread*);
	void setMessagePainter(MessagePainter*);
	void enqueue(const uint32_t* backing);

	bool supportsShaders() const { return m_supportsShaders; }

public slots:
	void forceDraw();
	void draw();
	void start();
	void stop();
	void pause();
	void unpause();
	void resize(const QSize& size);
	void lockAspectRatio(bool lock);
	void lockIntegerScaling(bool lock);
	void filter(bool filter);

	void setShaders(struct VDir*);
	void clearShaders();
	VideoShader* shaders();

private:
	void performDraw();
	void dequeue();
	void dequeueAll();

	QList<uint32_t*> m_free;
	QQueue<uint32_t*> m_queue;
	QPainter m_painter;
	QMutex m_mutex;
	QGLWidget* m_gl;
	bool m_active;
	bool m_started;
	mCoreThread* m_context;
	bool m_supportsShaders;
	VideoShader m_shader;
	VideoBackend* m_backend;
	QSize m_size;
	MessagePainter* m_messagePainter;
	QElapsedTimer m_delayTimer;
};

}

#endif

#endif
