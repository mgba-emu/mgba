/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "Display.h"

#include <QApplication>
#include <QResizeEvent>

extern "C" {
#include "gba-thread.h"
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

Display::Display(QGLFormat format, QWidget* parent)
	: QGLWidget(format, parent)
	, m_painter(nullptr)
	, m_drawThread(nullptr)
{
	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	setMinimumSize(VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
	setAutoBufferSwap(false);
	setCursor(Qt::BlankCursor);
}

void Display::startDrawing(const uint32_t* buffer, GBAThread* thread) {
	if (m_drawThread) {
		return;
	}
	m_drawThread = new QThread(this);
	m_painter = new Painter(this);
	m_painter->setContext(thread);
	m_painter->setBacking(buffer);
	m_painter->moveToThread(m_drawThread);
	m_context = thread;
	doneCurrent();
	context()->moveToThread(m_drawThread);
	connect(m_drawThread, SIGNAL(started()), m_painter, SLOT(start()));
	m_drawThread->start(QThread::TimeCriticalPriority);
}

void Display::stopDrawing() {
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

void Display::pauseDrawing() {
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

void Display::unpauseDrawing() {
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

void Display::forceDraw() {
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter, "forceDraw", Qt::QueuedConnection);
	}
}

#ifdef USE_PNG
void Display::screenshot() {
	GBAThreadInterrupt(m_context);
	GBAThreadTakeScreenshot(m_context);
	GBAThreadContinue(m_context);
}
#endif

void Display::initializeGL() {
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	swapBuffers();
}

void Display::resizeEvent(QResizeEvent* event) {
	if (m_drawThread) {
		GBAThreadInterrupt(m_context);
		GBASyncSuspendDrawing(&m_context->sync);
		QMetaObject::invokeMethod(m_painter, "resize", Qt::BlockingQueuedConnection, Q_ARG(QSize, event->size()));
		GBASyncResumeDrawing(&m_context->sync);
		GBAThreadContinue(m_context);
	}
}

Painter::Painter(Display* parent)
	: m_gl(parent)
{
	m_size = parent->size();
}

void Painter::setContext(GBAThread* context) {
	m_context = context;
}

void Painter::setBacking(const uint32_t* backing) {
	m_backing = backing;
}

void Painter::resize(const QSize& size) {
	m_size = size;
	m_gl->makeCurrent();
	glViewport(0, 0, m_size.width() * m_gl->devicePixelRatio(), m_size.height() * m_gl->devicePixelRatio());
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	m_gl->swapBuffers();
	m_gl->doneCurrent();
}

void Painter::start() {
	m_gl->makeCurrent();
	glEnable(GL_TEXTURE_2D);
	glGenTextures(1, &m_tex);
	glBindTexture(GL_TEXTURE_2D, m_tex);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_INT, 0, _glVertices);
	glTexCoordPointer(2, GL_INT, 0, _glTexCoords);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 240, 160, 0, 0, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	m_gl->doneCurrent();

	m_drawTimer = new QTimer;
	m_drawTimer->moveToThread(QThread::currentThread());
	m_drawTimer->setInterval(0);
	connect(m_drawTimer, SIGNAL(timeout()), this, SLOT(draw()));
	m_drawTimer->start();
}

void Painter::draw() {
	m_gl->makeCurrent();
	if (GBASyncWaitFrameStart(&m_context->sync, m_context->frameskip)) {
		glViewport(0, 0, m_size.width() * m_gl->devicePixelRatio(), m_size.height() * m_gl->devicePixelRatio());
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_backing);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		if (m_context->sync.videoFrameWait) {
			glFlush();
		}
	}
	GBASyncWaitFrameEnd(&m_context->sync);
	m_gl->swapBuffers();
	m_gl->doneCurrent();
}

void Painter::forceDraw() {
	m_gl->makeCurrent();
	glViewport(0, 0, m_size.width() * m_gl->devicePixelRatio(), m_size.height() * m_gl->devicePixelRatio());
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_backing);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	if (m_context->sync.videoFrameWait) {
		glFlush();
	}
	m_gl->swapBuffers();
	m_gl->doneCurrent();
}

void Painter::stop() {
	m_drawTimer->stop();
	delete m_drawTimer;
	m_gl->makeCurrent();
	glDeleteTextures(1, &m_tex);
	glClear(GL_COLOR_BUFFER_BIT);
	m_gl->swapBuffers();
	m_gl->doneCurrent();
	m_gl->context()->moveToThread(QApplication::instance()->thread());
}

void Painter::pause() {
	m_drawTimer->stop();
	// Make sure both buffers are filled
	forceDraw();
	forceDraw();
}

void Painter::unpause() {
	m_drawTimer->start();
}
