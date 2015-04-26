/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DisplayGL.h"

#include <QApplication>
#include <QResizeEvent>

extern "C" {
#include "gba/supervisor/thread.h"
}

using namespace QGBA;

static const GLint _glVertices[] = {
	0, 0,
	256, 0,
	256, 256,
	0, 256
};

static const GLint _glTexCoords[] = {
	0, 0,
	1, 0,
	1, 1,
	0, 1
};

DisplayGL::DisplayGL(const QGLFormat& format, QWidget* parent)
	: Display(parent)
	, m_gl(new EmptyGLWidget(format, this))
	, m_painter(new PainterGL(m_gl))
	, m_drawThread(nullptr)
	, m_lockAspectRatio(false)
	, m_filter(false)
	, m_context(nullptr)
{
}

DisplayGL::~DisplayGL() {
	delete m_painter;
}

void DisplayGL::startDrawing(GBAThread* thread) {
	if (m_drawThread) {
		return;
	}
	m_painter->setContext(thread);
	m_context = thread;
	m_painter->resize(size());
	m_gl->move(0, 0);
	m_gl->resize(size());
	m_drawThread = new QThread(this);
	m_gl->context()->doneCurrent();
	m_gl->context()->moveToThread(m_drawThread);
	m_painter->moveToThread(m_drawThread);
	connect(m_drawThread, SIGNAL(started()), m_painter, SLOT(start()));
	m_drawThread->start();

	lockAspectRatio(m_lockAspectRatio);
	filter(m_filter);
}

void DisplayGL::stopDrawing() {
	if (m_drawThread) {
		if (GBAThreadIsActive(m_context)) {
			GBAThreadInterrupt(m_context);
			GBASyncSuspendDrawing(&m_context->sync);
		}
		QMetaObject::invokeMethod(m_painter, "stop", Qt::BlockingQueuedConnection);
		m_drawThread->exit();
		m_drawThread = nullptr;
		if (GBAThreadIsActive(m_context)) {
			GBASyncResumeDrawing(&m_context->sync);
			GBAThreadContinue(m_context);
		}
	}
}

void DisplayGL::pauseDrawing() {
	if (m_drawThread) {
		if (GBAThreadIsActive(m_context)) {
			GBAThreadInterrupt(m_context);
			GBASyncSuspendDrawing(&m_context->sync);
		}
		QMetaObject::invokeMethod(m_painter, "pause", Qt::BlockingQueuedConnection);
		if (GBAThreadIsActive(m_context)) {
			GBASyncResumeDrawing(&m_context->sync);
			GBAThreadContinue(m_context);
		}
	}
}

void DisplayGL::unpauseDrawing() {
	if (m_drawThread) {
		if (GBAThreadIsActive(m_context)) {
			GBAThreadInterrupt(m_context);
			GBASyncSuspendDrawing(&m_context->sync);
		}
		QMetaObject::invokeMethod(m_painter, "unpause", Qt::BlockingQueuedConnection);
		if (GBAThreadIsActive(m_context)) {
			GBASyncResumeDrawing(&m_context->sync);
			GBAThreadContinue(m_context);
		}
	}
}

void DisplayGL::forceDraw() {
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter, "forceDraw");
	}
}

void DisplayGL::lockAspectRatio(bool lock) {
	m_lockAspectRatio = lock;
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter, "lockAspectRatio", Q_ARG(bool, lock));
	}
}

void DisplayGL::filter(bool filter) {
	m_filter = filter;
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter, "filter", Q_ARG(bool, filter));
	}
}

void DisplayGL::framePosted(const uint32_t* buffer) {
	if (m_drawThread && buffer) {
		QMetaObject::invokeMethod(m_painter, "setBacking", Q_ARG(const uint32_t*, buffer));
	}
}

void DisplayGL::resizeEvent(QResizeEvent* event) {
	m_gl->resize(size());
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter, "resize", Qt::BlockingQueuedConnection, Q_ARG(QSize, event->size()));
	}
}

PainterGL::PainterGL(QGLWidget* parent)
	: m_gl(parent)
	, m_drawTimer(nullptr)
	, m_lockAspectRatio(false)
	, m_filter(false)
	, m_context(nullptr)
{
}

void PainterGL::setContext(GBAThread* context) {
	m_context = context;
}

void PainterGL::setBacking(const uint32_t* backing) {
	m_gl->makeCurrent();
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, backing);
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, backing);
#endif
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, backing);
#endif
	m_gl->doneCurrent();
}

void PainterGL::resize(const QSize& size) {
	m_size = size;
	if (m_drawTimer) {
		forceDraw();
		forceDraw();
	}
}

void PainterGL::lockAspectRatio(bool lock) {
	m_lockAspectRatio = lock;
	if (m_drawTimer) {
		forceDraw();
		forceDraw();
	}
}

void PainterGL::filter(bool filter) {
	m_filter = filter;
	m_gl->makeCurrent();
	if (m_filter) {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	} else {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	m_gl->doneCurrent();
	if (m_drawTimer) {
		forceDraw();
	}
}

void PainterGL::start() {
	m_gl->makeCurrent();
	glEnable(GL_TEXTURE_2D);
	glGenTextures(1, &m_tex);
	glBindTexture(GL_TEXTURE_2D, m_tex);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	if (m_filter) {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	} else {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_INT, 0, _glVertices);
	glTexCoordPointer(2, GL_INT, 0, _glTexCoords);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 240, 160, 0, 0, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	m_gl->doneCurrent();

	m_drawTimer = new QTimer;
	m_drawTimer->moveToThread(QThread::currentThread());
	m_drawTimer->setInterval(0);
	connect(m_drawTimer, SIGNAL(timeout()), this, SLOT(draw()));
	m_drawTimer->start();
}

void PainterGL::draw() {
	m_gl->makeCurrent();
	GBASyncWaitFrameStart(&m_context->sync, m_context->frameskip);
	performDraw();
	GBASyncWaitFrameEnd(&m_context->sync);
	m_gl->swapBuffers();
	m_gl->doneCurrent();
}

void PainterGL::forceDraw() {
	m_gl->makeCurrent();
	glViewport(0, 0, m_size.width() * m_gl->devicePixelRatio(), m_size.height() * m_gl->devicePixelRatio());
	glClear(GL_COLOR_BUFFER_BIT);
	performDraw();
	m_gl->swapBuffers();
	m_gl->doneCurrent();
}

void PainterGL::stop() {
	m_drawTimer->stop();
	delete m_drawTimer;
	m_drawTimer = nullptr;
	m_gl->makeCurrent();
	glDeleteTextures(1, &m_tex);
	glClear(GL_COLOR_BUFFER_BIT);
	m_gl->swapBuffers();
	m_gl->doneCurrent();
	m_gl->context()->moveToThread(m_gl->thread());
	moveToThread(m_gl->thread());
}

void PainterGL::pause() {
	m_drawTimer->stop();
	// Make sure both buffers are filled
	forceDraw();
	forceDraw();
}

void PainterGL::unpause() {
	m_drawTimer->start();
}

void PainterGL::performDraw() {
	int w = m_size.width() * m_gl->devicePixelRatio();
	int h = m_size.height() *m_gl->devicePixelRatio();
#ifndef Q_OS_MAC
	// TODO: This seems to cause framerates to drag down to 120 FPS on OS X,
	// even if the emulator can go faster. Look into why.
	glViewport(0, 0, m_size.width() * m_gl->devicePixelRatio(), m_size.height() * m_gl->devicePixelRatio());
	glClear(GL_COLOR_BUFFER_BIT);
#endif
	int drawW = w;
	int drawH = h;
	if (m_lockAspectRatio) {
		if (w * 2 > h * 3) {
			drawW = h * 3 / 2;
		} else if (w * 2 < h * 3) {
			drawH = w * 2 / 3;
		}
	}
	glViewport((w - drawW) / 2, (h - drawH) / 2, drawW, drawH);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	if (m_context->sync.videoFrameWait) {
		glFlush();
	}
}
