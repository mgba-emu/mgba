/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DisplayGL.h"

#if defined(BUILD_GL) || defined(BUILD_GLES2)

#include "CoreController.h"

#include <QApplication>
#include <QResizeEvent>
#include <QTimer>

#include <mgba/core/core.h>
#include <mgba-util/math.h>
#ifdef BUILD_GL
#include "platform/opengl/gl.h"
#endif
#if !defined(_WIN32) || defined(USE_EPOXY)
#include "platform/opengl/gles2.h"
#ifdef _WIN32
#include <epoxy/wgl.h>
#endif
#endif

using namespace QGBA;

DisplayGL::DisplayGL(const QGLFormat& format, QWidget* parent)
	: Display(parent)
	, m_gl(nullptr)
{
	// This can spontaneously re-enter into this->resizeEvent before creation is done, so we
	// need to make sure it's initialized to nullptr before we assign the new object to it
	m_gl = new EmptyGLWidget(format, this);
	m_painter = new PainterGL(format.majorVersion() < 2 ? 1 : m_gl->format().majorVersion(), m_gl);
	m_gl->setMouseTracking(true);
	m_gl->setAttribute(Qt::WA_TransparentForMouseEvents); // This doesn't seem to work?
}

DisplayGL::~DisplayGL() {
	delete m_painter;
}

bool DisplayGL::supportsShaders() const {
	return m_painter->supportsShaders();
}

VideoShader* DisplayGL::shaders() {
	VideoShader* shaders = nullptr;
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter, "shaders", Qt::BlockingQueuedConnection, Q_RETURN_ARG(VideoShader*, shaders));
	} else {
		shaders = m_painter->shaders();
	}
	return shaders;
}

void DisplayGL::startDrawing(std::shared_ptr<CoreController> controller) {
	if (m_drawThread) {
		return;
	}
	m_isDrawing = true;
	m_painter->setContext(controller);
	m_painter->setMessagePainter(messagePainter());
	m_context = controller;
	m_painter->resize(size());
	m_gl->move(0, 0);
	m_drawThread = new QThread(this);
	m_drawThread->setObjectName("Painter Thread");
	m_gl->context()->doneCurrent();
	m_gl->context()->moveToThread(m_drawThread);
	m_painter->moveToThread(m_drawThread);
	connect(m_drawThread, &QThread::started, m_painter, &PainterGL::start);
	m_drawThread->start();

	lockAspectRatio(isAspectRatioLocked());
	lockIntegerScaling(isIntegerScalingLocked());
	filter(isFiltered());
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
	messagePainter()->resize(size(), isAspectRatioLocked(), devicePixelRatioF());
#else
	messagePainter()->resize(size(), isAspectRatioLocked(), devicePixelRatio());
#endif
	resizePainter();
}

void DisplayGL::stopDrawing() {
	if (m_drawThread) {
		m_isDrawing = false;
		CoreController::Interrupter interrupter(m_context);
		QMetaObject::invokeMethod(m_painter, "stop", Qt::BlockingQueuedConnection);
		m_drawThread->exit();
		m_drawThread = nullptr;
	}
	m_context.reset();
}

void DisplayGL::pauseDrawing() {
	if (m_drawThread) {
		m_isDrawing = false;
		CoreController::Interrupter interrupter(m_context);
		QMetaObject::invokeMethod(m_painter, "pause", Qt::BlockingQueuedConnection);
	}
}

void DisplayGL::unpauseDrawing() {
	if (m_drawThread) {
		m_isDrawing = true;
		CoreController::Interrupter interrupter(m_context);
		QMetaObject::invokeMethod(m_painter, "unpause", Qt::BlockingQueuedConnection);
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

void DisplayGL::lockIntegerScaling(bool lock) {
	Display::lockIntegerScaling(lock);
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter, "lockIntegerScaling", Q_ARG(bool, lock));
	}
}

void DisplayGL::filter(bool filter) {
	Display::filter(filter);
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter, "filter", Q_ARG(bool, filter));
	}
}

void DisplayGL::framePosted() {
	if (m_drawThread) {
		m_painter->enqueue(m_context->drawContext());
		QMetaObject::invokeMethod(m_painter, "draw");
	}
}

void DisplayGL::setShaders(struct VDir* shaders) {
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter, "setShaders", Qt::BlockingQueuedConnection, Q_ARG(struct VDir*, shaders));
	} else {
		m_painter->setShaders(shaders);
	}
}

void DisplayGL::clearShaders() {
	QMetaObject::invokeMethod(m_painter, "clearShaders");
}


void DisplayGL::resizeContext() {
	if (m_drawThread) {
		m_isDrawing = false;
		CoreController::Interrupter interrupter(m_context);
		QMetaObject::invokeMethod(m_painter, "resizeContext", Qt::BlockingQueuedConnection);
	}
}

void DisplayGL::resizeEvent(QResizeEvent* event) {
	Display::resizeEvent(event);
	resizePainter();
}

void DisplayGL::resizePainter() {
	if (m_gl) {
		m_gl->resize(size());
	}
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter, "resize", Qt::BlockingQueuedConnection, Q_ARG(QSize, size()));
	}
}

PainterGL::PainterGL(int majorVersion, QGLWidget* parent)
	: m_gl(parent)
{
#ifdef BUILD_GL
	mGLContext* glBackend;
#endif
#if !defined(_WIN32) || defined(USE_EPOXY)
	mGLES2Context* gl2Backend;
#endif

#if !defined(_WIN32) || defined(USE_EPOXY)
	if (majorVersion >= 2) {
		gl2Backend = static_cast<mGLES2Context*>(malloc(sizeof(mGLES2Context)));
		mGLES2ContextCreate(gl2Backend);
		m_backend = &gl2Backend->d;
		m_supportsShaders = true;
	}
#endif

#ifdef BUILD_GL
	 if (!m_backend) {
		glBackend = static_cast<mGLContext*>(malloc(sizeof(mGLContext)));
		mGLContextCreate(glBackend);
		m_backend = &glBackend->d;
		m_supportsShaders = false;
	}
#endif
	m_backend->swap = [](VideoBackend* v) {
		PainterGL* painter = static_cast<PainterGL*>(v->user);
		painter->m_gl->swapBuffers();
	};

	m_gl->makeCurrent();
#if defined(_WIN32) && defined(USE_EPOXY)
	epoxy_handle_external_wglMakeCurrent();
#endif
	m_backend->init(m_backend, reinterpret_cast<WHandle>(m_gl->winId()));
#if !defined(_WIN32) || defined(USE_EPOXY)
	if (m_supportsShaders) {
		m_shader.preprocessShader = static_cast<void*>(&reinterpret_cast<mGLES2Context*>(m_backend)->initialShader);
	}
#endif
	m_gl->doneCurrent();

	m_backend->user = this;
	m_backend->filter = false;
	m_backend->lockAspectRatio = false;

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
	m_gl->makeCurrent();
#if defined(_WIN32) && defined(USE_EPOXY)
	epoxy_handle_external_wglMakeCurrent();
#endif
#if !defined(_WIN32) || defined(USE_EPOXY)
	if (m_shader.passes) {
		mGLES2ShaderFree(&m_shader);
	}
#endif
	m_backend->deinit(m_backend);
	m_gl->doneCurrent();
	free(m_backend);
	m_backend = nullptr;
}

void PainterGL::setContext(std::shared_ptr<CoreController> context) {
	m_context = context;
	resizeContext();
}

void PainterGL::resizeContext() {
	if (!m_context) {
		return;
	}

	m_gl->makeCurrent();
#if defined(_WIN32) && defined(USE_EPOXY)
	epoxy_handle_external_wglMakeCurrent();
#endif
	QSize size = m_context->screenDimensions();
	m_backend->setDimensions(m_backend, size.width(), size.height());
	m_gl->doneCurrent();
}

void PainterGL::setMessagePainter(MessagePainter* messagePainter) {
	m_messagePainter = messagePainter;
}

void PainterGL::resize(const QSize& size) {
	m_size = size;
	if (m_started && !m_active) {
		forceDraw();
	}
}

void PainterGL::lockAspectRatio(bool lock) {
	m_backend->lockAspectRatio = lock;
	if (m_started && !m_active) {
		forceDraw();
	}
}

void PainterGL::lockIntegerScaling(bool lock) {
	m_backend->lockIntegerScaling = lock;
	if (m_started && !m_active) {
		forceDraw();
	}
}

void PainterGL::filter(bool filter) {
	m_backend->filter = filter;
	if (m_started && !m_active) {
		forceDraw();
	}
}

void PainterGL::start() {
	m_gl->makeCurrent();
#if defined(_WIN32) && defined(USE_EPOXY)
	epoxy_handle_external_wglMakeCurrent();
#endif

#if !defined(_WIN32) || defined(USE_EPOXY)
	if (m_supportsShaders && m_shader.passes) {
		mGLES2ShaderAttach(reinterpret_cast<mGLES2Context*>(m_backend), static_cast<mGLES2Shader*>(m_shader.passes), m_shader.nPasses);
	}
#endif

	m_gl->doneCurrent();
	m_active = true;
	m_started = true;
}

void PainterGL::draw() {
	if (m_queue.isEmpty()) {
		return;
	}

	if (mCoreSyncWaitFrameStart(&m_context->thread()->impl->sync) || !m_queue.isEmpty()) {
		dequeue();
		mCoreSyncWaitFrameEnd(&m_context->thread()->impl->sync);
		m_painter.begin(m_gl->context()->device());
		performDraw();
		m_painter.end();
		m_backend->swap(m_backend);
		if (!m_delayTimer.isValid()) {
			m_delayTimer.start();
		} else {
			while (m_delayTimer.elapsed() < 15) {
				QThread::usleep(100);
			}
			m_delayTimer.restart();
		}
	} else {
		mCoreSyncWaitFrameEnd(&m_context->thread()->impl->sync);
	}
	if (!m_queue.isEmpty()) {
		QMetaObject::invokeMethod(this, "draw", Qt::QueuedConnection);
	}
}

void PainterGL::forceDraw() {
	m_painter.begin(m_gl->context()->device());
	performDraw();
	m_painter.end();
	m_backend->swap(m_backend);
}

void PainterGL::stop() {
	m_active = false;
	m_started = false;
	m_gl->makeCurrent();
#if defined(_WIN32) && defined(USE_EPOXY)
	epoxy_handle_external_wglMakeCurrent();
#endif
	dequeueAll();
	m_backend->clear(m_backend);
	m_backend->swap(m_backend);
	m_gl->doneCurrent();
	m_gl->context()->moveToThread(m_gl->thread());
	m_context.reset();
	moveToThread(m_gl->thread());
}

void PainterGL::pause() {
	m_active = false;
}

void PainterGL::unpause() {
	m_active = true;
}

void PainterGL::performDraw() {
	m_painter.beginNativePainting();
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
	float r = m_gl->devicePixelRatioF();
#else
	float r = m_gl->devicePixelRatio();
#endif
	m_backend->resized(m_backend, m_size.width() * r, m_size.height() * r);
	m_backend->drawFrame(m_backend);
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
	QSize size = m_context->screenDimensions();
	memcpy(buffer, backing, size.width() * size.height() * BYTES_PER_PIXEL);
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
	m_backend->postFrame(m_backend, buffer);
	m_free.append(buffer);
	m_mutex.unlock();
}

void PainterGL::dequeueAll() {
	uint32_t* buffer = 0;
	m_mutex.lock();
	while (!m_queue.isEmpty()) {
		buffer = m_queue.dequeue();
		m_free.append(buffer);
	}
	if (buffer) {
		m_backend->postFrame(m_backend, buffer);
	}
	m_mutex.unlock();
}

void PainterGL::setShaders(struct VDir* dir) {
	if (!supportsShaders()) {
		return;
	}
#if !defined(_WIN32) || defined(USE_EPOXY)
	m_gl->makeCurrent();
#if defined(_WIN32) && defined(USE_EPOXY)
	epoxy_handle_external_wglMakeCurrent();
#endif
	if (m_shader.passes) {
		mGLES2ShaderDetach(reinterpret_cast<mGLES2Context*>(m_backend));
		mGLES2ShaderFree(&m_shader);
	}
	mGLES2ShaderLoad(&m_shader, dir);
	if (m_started) {
		mGLES2ShaderAttach(reinterpret_cast<mGLES2Context*>(m_backend), static_cast<mGLES2Shader*>(m_shader.passes), m_shader.nPasses);
	}
	m_gl->doneCurrent();
#endif
}

void PainterGL::clearShaders() {
	if (!supportsShaders()) {
		return;
	}
#if !defined(_WIN32) || defined(USE_EPOXY)
	m_gl->makeCurrent();
#if defined(_WIN32) && defined(USE_EPOXY)
	epoxy_handle_external_wglMakeCurrent();
#endif
	if (m_shader.passes) {
		mGLES2ShaderDetach(reinterpret_cast<mGLES2Context*>(m_backend));
		mGLES2ShaderFree(&m_shader);
	}
	m_gl->doneCurrent();
#endif
}

VideoShader* PainterGL::shaders() {
	return &m_shader;
}

#endif
