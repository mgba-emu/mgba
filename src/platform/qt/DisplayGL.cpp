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
	m_drawThread = new QThread(this);
	m_gl->context()->doneCurrent();
	m_gl->context()->moveToThread(m_drawThread);
	m_painter->moveToThread(m_drawThread);
	connect(m_drawThread, SIGNAL(started()), m_painter, SLOT(start()));
	m_drawThread->start();

	lockAspectRatio(m_lockAspectRatio);
	filter(m_filter);
	resizePainter();
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

void DisplayGL::showMessage(const QString& message) {
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter, "showMessage", Q_ARG(const QString&, message));
	}
}

void DisplayGL::resizeEvent(QResizeEvent*) {
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
	, m_drawTimer(nullptr)
	, m_messageTimer(this)
	, m_context(nullptr)
{
	GBAGLContextCreate(&m_backend);
	m_backend.d.swap = [](VideoBackend* v) {
		PainterGL* painter = static_cast<PainterGL*>(v->user);
		painter->m_gl->swapBuffers();
	};
	m_backend.d.user = this;
	m_backend.d.filter = false;
	m_backend.d.lockAspectRatio = false;
	m_messageFont.setFamily("Source Code Pro");
	m_messageFont.setStyleHint(QFont::Monospace);
	m_messageFont.setPixelSize(13);
	connect(&m_messageTimer, SIGNAL(timeout()), this, SLOT(clearMessage()));
	m_messageTimer.setSingleShot(true);
	m_messageTimer.setInterval(5000);

	clearMessage();
}

void PainterGL::setContext(GBAThread* context) {
	m_context = context;
}

void PainterGL::setBacking(const uint32_t* backing) {
	m_gl->makeCurrent();
	m_backend.d.postFrame(&m_backend.d, backing);
	m_gl->doneCurrent();
}

void PainterGL::resize(const QSize& size) {
	m_size = size;
	int w = m_size.width();
	int h = m_size.height();
	int drawW = w;
	int drawH = h;
	if (m_backend.d.lockAspectRatio) {
		if (w * 2 > h * 3) {
			drawW = h * 3 / 2;
		} else if (w * 2 < h * 3) {
			drawH = w * 2 / 3;
		}
	}
	m_world.reset();
	m_world.translate((w - drawW) / 2, (h - drawH) / 2);
	m_world.scale(qreal(drawW) / VIDEO_HORIZONTAL_PIXELS, qreal(drawH) / VIDEO_VERTICAL_PIXELS);
	m_message.prepare(m_world, m_messageFont);
	if (m_drawTimer) {
		forceDraw();
	}
}

void PainterGL::lockAspectRatio(bool lock) {
	m_backend.d.lockAspectRatio = lock;
	if (m_drawTimer) {
		forceDraw();
	}
}

void PainterGL::filter(bool filter) {
	m_backend.d.filter = filter;
	if (m_drawTimer) {
		forceDraw();
	}
}

void PainterGL::start() {
	m_gl->makeCurrent();
	m_backend.d.init(&m_backend.d, reinterpret_cast<WHandle>(m_gl->winId()));
	m_gl->doneCurrent();

	m_drawTimer = new QTimer;
	m_drawTimer->moveToThread(QThread::currentThread());
	m_drawTimer->setInterval(0);
	connect(m_drawTimer, SIGNAL(timeout()), this, SLOT(draw()));
	m_drawTimer->start();
}

void PainterGL::draw() {
	GBASyncWaitFrameStart(&m_context->sync, m_context->frameskip);
	m_painter.begin(m_gl->context()->device());
	performDraw();
	m_painter.end();
	GBASyncWaitFrameEnd(&m_context->sync);
	m_backend.d.swap(&m_backend.d);
}

void PainterGL::forceDraw() {
	m_painter.begin(m_gl->context()->device());
	performDraw();
	m_painter.end();
	m_backend.d.swap(&m_backend.d);
}

void PainterGL::stop() {
	m_drawTimer->stop();
	delete m_drawTimer;
	m_drawTimer = nullptr;
	m_gl->makeCurrent();
	m_backend.d.clear(&m_backend.d);
	m_backend.d.swap(&m_backend.d);
	m_backend.d.deinit(&m_backend.d);
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
	m_painter.beginNativePainting();
	float r = m_gl->devicePixelRatio();
	m_backend.d.resized(&m_backend.d, m_size.width() * r, m_size.height() * r);
	m_backend.d.drawFrame(&m_backend.d);
	m_painter.endNativePainting();
	m_painter.setWorldTransform(m_world);
	m_painter.setRenderHint(QPainter::Antialiasing);
	m_painter.setFont(m_messageFont);
	m_painter.setPen(Qt::black);
	m_painter.translate(1, VIDEO_VERTICAL_PIXELS - m_messageFont.pixelSize() - 1);
	for (int i = 0; i < 16; ++i) {
		m_painter.save();
		m_painter.translate(cos(i * M_PI / 8.0) * 0.8, sin(i * M_PI / 8.0) * 0.8);
		m_painter.drawStaticText(0, 0, m_message);
		m_painter.restore();
	}
	m_painter.setPen(Qt::white);
	m_painter.drawStaticText(0, 0, m_message);
}

void PainterGL::showMessage(const QString& message) {
	m_message.setText(message);
	m_message.prepare(m_world, m_messageFont);
	m_messageTimer.stop();
	m_messageTimer.start();
}

void PainterGL::clearMessage() {
	m_message.setText(QString());
}
