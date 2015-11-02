/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_DISPLAY_GL
#define QGBA_DISPLAY_GL

#include "Display.h"

#ifdef USE_EPOXY
#include <epoxy/gl.h>
#include <epoxy/wgl.h>
#endif

#include <QGLWidget>
#include <QList>
#include <QMouseEvent>
#include <QQueue>
#include <QThread>
#include <QTimer>

struct GBAThread;
struct VideoBackend;
struct GBAGLES2Shader;

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

public slots:
	void startDrawing(GBAThread* context) override;
	void stopDrawing() override;
	void pauseDrawing() override;
	void unpauseDrawing() override;
	void forceDraw() override;
	void lockAspectRatio(bool lock) override;
	void filter(bool filter) override;
	void framePosted(const uint32_t*) override;
	void setShaders(struct VDir*) override;

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
	PainterGL(QGLWidget* parent, QGLFormat::OpenGLVersionFlags = QGLFormat::OpenGL_Version_1_1);
	~PainterGL();

	void setContext(GBAThread*);
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
	void filter(bool filter);

	void setShaders(struct VDir*);

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
	GBAThread* m_context;
	bool m_supportsShaders;
	GBAGLES2Shader* m_shaders;
	size_t m_nShaders;
	VideoBackend* m_backend;
	QSize m_size;
	MessagePainter* m_messagePainter;
};

}

#endif
