/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#if defined(BUILD_GL) || defined(BUILD_GLES2)

#include "Display.h"

#ifdef USE_EPOXY
#include <epoxy/gl.h>
#ifndef GLdouble
#define GLdouble GLdouble
#endif
#endif

#include <QElapsedTimer>
#include <QOpenGLContext>
#include <QList>
#include <QMouseEvent>
#include <QPainter>
#include <QQueue>
#include <QThread>
#include <QTimer>

#include "VideoProxy.h"

#include "platform/video-backend.h"

class QOpenGLPaintDevice;

namespace QGBA {

class PainterGL;
class DisplayGL : public Display {
Q_OBJECT

public:
	DisplayGL(const QSurfaceFormat& format, QWidget* parent = nullptr);
	~DisplayGL();

	void startDrawing(std::shared_ptr<CoreController>) override;
	bool isDrawing() const override { return m_isDrawing; }
	bool supportsShaders() const override;
	VideoShader* shaders() override;
	void setVideoProxy(std::shared_ptr<VideoProxy>) override;
	int framebufferHandle() override;

public slots:
	void stopDrawing() override;
	void pauseDrawing() override;
	void unpauseDrawing() override;
	void forceDraw() override;
	void lockAspectRatio(bool lock) override;
	void lockIntegerScaling(bool lock) override;
	void interframeBlending(bool enable) override;
	void showOSDMessages(bool enable) override;
	void filter(bool filter) override;
	void framePosted() override;
	void setShaders(struct VDir*) override;
	void clearShaders() override;
	void resizeContext() override;
	void setVideoScale(int scale) override;

protected:
	virtual void paintEvent(QPaintEvent*) override { forceDraw(); }
	virtual void resizeEvent(QResizeEvent*) override;

private:
	void resizePainter();

	bool m_isDrawing = false;
	QOpenGLContext* m_gl;
	PainterGL* m_painter;
	QThread* m_drawThread = nullptr;
	std::shared_ptr<CoreController> m_context;
};

class PainterGL : public QObject {
Q_OBJECT

public:
	PainterGL(QWindow* surface, QOpenGLContext* parent, int forceVersion = 0);
	~PainterGL();

	void setContext(std::shared_ptr<CoreController>);
	void setMessagePainter(MessagePainter*);
	void enqueue(const uint32_t* backing);

	bool supportsShaders() const { return m_supportsShaders; }

	void setVideoProxy(std::shared_ptr<VideoProxy>);

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
	void interframeBlending(bool enable);
	void showOSD(bool enable);
	void filter(bool filter);
	void resizeContext();

	void setShaders(struct VDir*);
	void clearShaders();
	VideoShader* shaders();

	int glTex();

private:
	void performDraw();
	void dequeue();
	void dequeueAll();

	QList<uint32_t*> m_free;
	QQueue<uint32_t*> m_queue;
	uint32_t* m_buffer;
	QPainter m_painter;
	QMutex m_mutex;
	QWindow* m_surface;
	QOpenGLPaintDevice* m_window;
	QOpenGLContext* m_gl;
	bool m_active = false;
	bool m_started = false;
	std::shared_ptr<CoreController> m_context = nullptr;
	bool m_supportsShaders;
	bool m_showOSD;
	VideoShader m_shader{};
	VideoBackend* m_backend = nullptr;
	QSize m_size;
	MessagePainter* m_messagePainter = nullptr;
	QElapsedTimer m_delayTimer;
	std::shared_ptr<VideoProxy> m_videoProxy;
};

}

#endif
