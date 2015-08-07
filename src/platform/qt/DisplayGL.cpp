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

DisplayGL::DisplayGL(const QGLFormat& format, QWidget* parent)
	: Display(parent)
	, m_isDrawing(false)
	, m_gl(new EmptyGLWidget(format, this))
	, m_painter(new PainterGL(m_gl))
	, m_drawThread(nullptr)
	, m_context(nullptr)
{
	m_gl->setMouseTracking(true);
	m_gl->setAttribute(Qt::WA_TransparentForMouseEvents); // This doesn't seem to work?
}

DisplayGL::~DisplayGL() {
	delete m_painter;
}

void DisplayGL::startDrawing(GBAThread* thread) {
	if (m_drawThread) {
		return;
	}
	m_isDrawing = true;
	m_painter->setContext(thread);
	m_painter->setMessagePainter(messagePainter());
	m_context = thread;
	m_painter->resize(size());
	m_gl->move(0, 0);
	m_drawThread = new QThread(this);
	m_drawThread->setObjectName("Painter Thread");
	m_gl->context()->doneCurrent();
	m_gl->context()->moveToThread(m_drawThread);
	m_painter->moveToThread(m_drawThread);
	connect(m_drawThread, SIGNAL(started()), m_painter, SLOT(start()));
	m_drawThread->start();
	GBASyncSetVideoSync(&m_context->sync, false);

	lockAspectRatio(isAspectRatioLocked());
	filter(isFiltered());
	messagePainter()->resize(size(), isAspectRatioLocked(), devicePixelRatio());
	resizePainter();
}

void DisplayGL::stopDrawing() {
	if (m_drawThread) {
		m_isDrawing = false;
		if (GBAThreadIsActive(m_context)) {
			GBAThreadInterrupt(m_context);
		}
		QMetaObject::invokeMethod(m_painter, "stop", Qt::BlockingQueuedConnection);
		m_drawThread->exit();
		m_drawThread = nullptr;
		if (GBAThreadIsActive(m_context)) {
			GBAThreadContinue(m_context);
		}
	}
}

void DisplayGL::pauseDrawing() {
	if (m_drawThread) {
		m_isDrawing = false;
		if (GBAThreadIsActive(m_context)) {
			GBAThreadInterrupt(m_context);
		}
		QMetaObject::invokeMethod(m_painter, "pause", Qt::BlockingQueuedConnection);
		if (GBAThreadIsActive(m_context)) {
			GBAThreadContinue(m_context);
		}
	}
}

void DisplayGL::unpauseDrawing() {
	if (m_drawThread) {
		m_isDrawing = true;
		if (GBAThreadIsActive(m_context)) {
			GBAThreadInterrupt(m_context);
		}
		QMetaObject::invokeMethod(m_painter, "unpause", Qt::BlockingQueuedConnection);
		if (GBAThreadIsActive(m_context)) {
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
	Display::lockAspectRatio(lock);
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter, "lockAspectRatio", Q_ARG(bool, lock));
	}
}

void DisplayGL::filter(bool filter) {
	Display::filter(filter);
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter, "filter", Q_ARG(bool, filter));
	}
}

void DisplayGL::framePosted(const uint32_t* buffer) {
	if (m_drawThread && buffer) {
		m_painter->enqueue(buffer);
		QMetaObject::invokeMethod(m_painter, "draw");
	}
}

void DisplayGL::resizeEvent(QResizeEvent* event) {
	Display::resizeEvent(event);
	resizePainter();
}

void DisplayGL::resizePainter() {
	m_gl->resize(size());
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter, "resize", Qt::BlockingQueuedConnection, Q_ARG(QSize, size()));
	}
}

PainterGL::PainterGL(QGLWidget* parent)
	: m_gl(parent)
	, m_active(false)
	, m_context(nullptr)
	, m_messagePainter(nullptr)
{
#ifdef BUILD_GL
	GBAGLContextCreate(&m_backend);
#elif defined(BUILD_GLES2)
	GBAGLES2ContextCreate(&m_backend);
#endif
	m_backend.d.swap = [](VideoBackend* v) {
		PainterGL* painter = static_cast<PainterGL*>(v->user);
		painter->m_gl->swapBuffers();
	};
	m_backend.d.user = this;
	m_backend.d.filter = false;
	m_backend.d.lockAspectRatio = false;

	for (int i = 0; i < 2; ++i) {
		m_free.append(new uint32_t[256 * 256]);
	}
}

PainterGL::~PainterGL() {
	while (!m_queue.isEmpty()) {
		delete[] m_queue.dequeue();
	}
	for (auto item : m_free) {
		delete[] item;
	}
}

void PainterGL::setContext(GBAThread* context) {
	m_context = context;
}

void PainterGL::setMessagePainter(MessagePainter* messagePainter) {
	m_messagePainter = messagePainter;
}

void PainterGL::resize(const QSize& size) {
	m_size = size;
	if (m_active) {
		forceDraw();
	}
}

void PainterGL::lockAspectRatio(bool lock) {
	m_backend.d.lockAspectRatio = lock;
	if (m_active) {
		forceDraw();
	}
}

void PainterGL::filter(bool filter) {
	m_backend.d.filter = filter;
	if (m_active) {
		forceDraw();
	}
}

void PainterGL::start() {
	m_gl->makeCurrent();
	m_backend.d.init(&m_backend.d, reinterpret_cast<WHandle>(m_gl->winId()));
	m_gl->doneCurrent();
	m_active = true;
}

void PainterGL::draw() {
	if (m_queue.isEmpty()) {
		return;
	}
	if (GBASyncWaitFrameStart(&m_context->sync, m_context->frameskip) || !m_queue.isEmpty()) {
		dequeue();
		m_painter.begin(m_gl->context()->device());
		performDraw();
		m_painter.end();
		GBASyncWaitFrameEnd(&m_context->sync);
		m_backend.d.swap(&m_backend.d);
	} else {
		GBASyncWaitFrameEnd(&m_context->sync);
	}
	if (!m_queue.isEmpty()) {
		QMetaObject::invokeMethod(this, "draw", Qt::QueuedConnection);
	}
}

void PainterGL::forceDraw() {
	m_painter.begin(m_gl->context()->device());
	performDraw();
	m_painter.end();
	m_backend.d.swap(&m_backend.d);
}

void PainterGL::stop() {
	m_active = false;
	m_gl->makeCurrent();
	dequeueAll();
	m_backend.d.clear(&m_backend.d);
	m_backend.d.swap(&m_backend.d);
	m_backend.d.deinit(&m_backend.d);
	m_gl->doneCurrent();
	m_gl->context()->moveToThread(m_gl->thread());
	moveToThread(m_gl->thread());
}

void PainterGL::pause() {
	m_active = false;
	// Make sure both buffers are filled
	m_gl->makeCurrent();
	dequeueAll();
	forceDraw();
	forceDraw();
	m_gl->doneCurrent();
}

void PainterGL::unpause() {
	m_active = true;
}

void PainterGL::performDraw() {
	m_painter.beginNativePainting();
	float r = m_gl->devicePixelRatio();
	m_backend.d.resized(&m_backend.d, m_size.width() * r, m_size.height() * r);
	m_backend.d.drawFrame(&m_backend.d);
	m_painter.endNativePainting();
	if (m_messagePainter) {
		m_messagePainter->paint(&m_painter);
	}
}

void PainterGL::enqueue(const uint32_t* backing) {
	m_mutex.lock();
	uint32_t* buffer;
	if (m_free.isEmpty()) {
		buffer = m_queue.dequeue();
	} else {
		buffer = m_free.takeLast();
	}
	memcpy(buffer, backing, 256 * VIDEO_VERTICAL_PIXELS * BYTES_PER_PIXEL);
	m_queue.enqueue(buffer);
	m_mutex.unlock();
}

void PainterGL::dequeue() {
	m_mutex.lock();
	if (m_queue.isEmpty()) {
		m_mutex.unlock();
		return;
	}
	uint32_t* buffer = m_queue.dequeue();
	m_backend.d.postFrame(&m_backend.d, buffer);
	m_free.append(buffer);
	m_mutex.unlock();
}

void PainterGL::dequeueAll() {
	uint32_t* buffer;
	m_mutex.lock();
	while (!m_queue.isEmpty()) {
		buffer = m_queue.dequeue();
		m_free.append(buffer);
	}
	m_backend.d.postFrame(&m_backend.d, buffer);
	m_mutex.unlock();
}
