/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "Display.h"

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

Display::Display(QGLFormat format, QWidget* parent)
	: QGLWidget(format, parent)
	, m_painter(nullptr)
	, m_started(false)
{
	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	setMinimumSize(VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
	setAutoBufferSwap(false);
	setCursor(Qt::BlankCursor);
}

void Display::startDrawing(const uint32_t* buffer, GBAThread* thread) {
	if (m_started) {
		return;
	}
	m_painter = new Painter(this);
	m_painter->setContext(thread);
	m_painter->setBacking(buffer);
	m_context = thread;
	doneCurrent();
	m_painter->start();
	m_started = true;

	lockAspectRatio(m_lockAspectRatio);
	filter(m_filter);
}

void Display::stopDrawing() {
	if (m_started) {
		if (GBAThreadIsActive(m_context)) {
			GBAThreadInterrupt(m_context);
			GBASyncSuspendDrawing(&m_context->sync);
		}
		m_painter->stop();
		m_started = false;
		if (GBAThreadIsActive(m_context)) {
			GBASyncResumeDrawing(&m_context->sync);
			GBAThreadContinue(m_context);
		}
	}
}

void Display::pauseDrawing() {
	if (m_started) {
		if (GBAThreadIsActive(m_context)) {
			GBAThreadInterrupt(m_context);
			GBASyncSuspendDrawing(&m_context->sync);
		}
		m_painter->pause();
		if (GBAThreadIsActive(m_context)) {
			GBASyncResumeDrawing(&m_context->sync);
			GBAThreadContinue(m_context);
		}
	}
}

void Display::unpauseDrawing() {
	if (m_started) {
		if (GBAThreadIsActive(m_context)) {
			GBAThreadInterrupt(m_context);
			GBASyncSuspendDrawing(&m_context->sync);
		}
		m_painter->unpause();
		if (GBAThreadIsActive(m_context)) {
			GBASyncResumeDrawing(&m_context->sync);
			GBAThreadContinue(m_context);
		}
	}
}

void Display::forceDraw() {
	if (m_started) {
		m_painter->forceDraw();
	}
}

void Display::lockAspectRatio(bool lock) {
	m_lockAspectRatio = lock;
	if (m_started) {
		m_painter->lockAspectRatio(lock);
	}
}

void Display::filter(bool filter) {
	m_filter = filter;
	if (m_started) {
		m_painter->filter(filter);
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
	if (m_started) {
		GBAThreadInterrupt(m_context);
		GBASyncSuspendDrawing(&m_context->sync);
		m_painter->resize(event->size());
		GBASyncResumeDrawing(&m_context->sync);
		GBAThreadContinue(m_context);
	}
}

Painter::Painter(Display* parent)
	: m_gl(parent)
	, m_lockAspectRatio(false)
	, m_filter(false)
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
	forceDraw();
	forceDraw();
}

void Painter::lockAspectRatio(bool lock) {
	m_lockAspectRatio = lock;
	forceDraw();
	forceDraw();
}

void Painter::filter(bool filter) {
	m_filter = filter;
	m_gl->makeCurrent();
	if (m_filter) {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	} else {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	m_gl->doneCurrent();
	forceDraw();
}

void Painter::start() {
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
	m_gl->doneCurrent();

	m_drawTimer = new QTimer;
	m_drawTimer->moveToThread(QThread::currentThread());
	m_drawTimer->setInterval(0);
	connect(m_drawTimer, SIGNAL(timeout()), this, SLOT(draw()));
	m_drawTimer->start();
}

void Painter::draw() {
	m_gl->makeCurrent();
	GBASyncWaitFrameStart(&m_context->sync, m_context->frameskip);
	performDraw();
	GBASyncWaitFrameEnd(&m_context->sync);
	m_gl->swapBuffers();
	m_gl->doneCurrent();
}

void Painter::forceDraw() {
	m_gl->makeCurrent();
	glViewport(0, 0, m_size.width() * m_gl->devicePixelRatio(), m_size.height() * m_gl->devicePixelRatio());
	glClear(GL_COLOR_BUFFER_BIT);
	performDraw();
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

void Painter::performDraw() {
	int w = m_size.width() * m_gl->devicePixelRatio();
	int h = m_size.height() * m_gl->devicePixelRatio();
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
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, m_backing);
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, m_backing);
#endif
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_backing);
#endif
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	if (m_context->sync.videoFrameWait) {
		glFlush();
	}
}
