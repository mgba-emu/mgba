/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#if defined(BUILD_GL) || defined(BUILD_GLES2) || defined(BUILD_GLES3) || defined(USE_EPOXY)

#include "Display.h"

#ifdef USE_EPOXY
#include <epoxy/gl.h>
#ifndef GLdouble
#define GLdouble GLdouble
#endif
#endif

#include <QAtomicInt>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QPainter>
#include <QQueue>
#include <QThread>
#include <QTimer>

#include <array>

#include "CoreController.h"
#include "VideoProxy.h"

#include "platform/video-backend.h"

class QOpenGLPaintDevice;

uint qHash(const QSurfaceFormat&, uint seed = 0);

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

	static bool supportsFormat(const QSurfaceFormat&);

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

	static QHash<QSurfaceFormat, bool> s_supports;

	bool m_isDrawing = false;
	bool m_hasStarted = false;
	std::unique_ptr<PainterGL> m_painter;
	QThread *m_drawThread;
#ifndef Q_OS_MAC
	QThread _m_drawThread;
#endif
	std::shared_ptr<CoreController> m_context;
};

class PainterGL : public QObject {
Q_OBJECT

public:
	PainterGL(QWindow* surface, const QSurfaceFormat& format);
	~PainterGL();

	void setThread(QThread*);
	void setContext(std::shared_ptr<CoreController>);
	void setMessagePainter(MessagePainter*);
	void enqueue(const uint32_t* backing);

	bool supportsShaders() const { return m_supportsShaders; }
	int glTex();

	void setVideoProxy(std::shared_ptr<VideoProxy>);
	void interrupt();

public slots:
	void create();
	void destroy();

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

signals:
	void started();

private:
	void makeCurrent();
	void performDraw();
	void dequeue();
	void dequeueAll(bool keep = false);

	std::array<std::array<uint32_t, 0x100000>, 3> m_buffers;
	QList<uint32_t*> m_free;
	QQueue<uint32_t*> m_queue;
	uint32_t* m_buffer = nullptr;
	QPainter m_painter;
	QMutex m_mutex;
	QWindow* m_surface;
	QSurfaceFormat m_format;
	std::unique_ptr<QOpenGLPaintDevice> m_window;
	std::unique_ptr<QOpenGLContext> m_gl;
	bool m_active = false;
	bool m_started = false;
	QTimer m_drawTimer;
	std::shared_ptr<CoreController> m_context;
	CoreController::Interrupter m_interrupter;
	bool m_supportsShaders;
	bool m_showOSD;
	VideoShader m_shader{};
	VideoBackend* m_backend = nullptr;
	QSize m_size;
	QSize m_dims;
	MessagePainter* m_messagePainter = nullptr;
	QElapsedTimer m_delayTimer;
	std::shared_ptr<VideoProxy> m_videoProxy;
};

}

#endif
