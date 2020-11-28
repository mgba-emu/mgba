/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DisplayGL.h"

#if defined(BUILD_GL) || defined(BUILD_GLES2)

#include "CoreController.h"

#include <QApplication>
#include <QOpenGLContext>
#include <QOpenGLPaintDevice>
#include <QMutexLocker>
#include <QResizeEvent>
#include <QScreen>
#include <QTimer>
#include <QWindow>

#include <mgba/core/core.h>
#include <mgba-util/math.h>
#ifdef BUILD_GL
#include "platform/opengl/gl.h"
#endif
#ifdef BUILD_GLES2
#include "platform/opengl/gles2.h"
#ifdef _WIN32
#include <epoxy/wgl.h>
#endif
#endif

using namespace QGBA;

DisplayGL::DisplayGL(const QSurfaceFormat& format, QWidget* parent)
	: Display(parent)
{
	setAttribute(Qt::WA_NativeWindow);
	windowHandle()->create();

	m_painter = std::make_unique<PainterGL>(windowHandle(), format);
}

DisplayGL::~DisplayGL() {
	stopDrawing();
}

bool DisplayGL::supportsShaders() const {
	return m_painter->supportsShaders();
}

VideoShader* DisplayGL::shaders() {
	VideoShader* shaders = nullptr;
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter.get(), "shaders", Qt::BlockingQueuedConnection, Q_RETURN_ARG(VideoShader*, shaders));
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
	m_drawThread = new QThread(this);
	m_drawThread->setObjectName("Painter Thread");
	m_painter->moveToThread(m_drawThread);
	if (videoProxy()) {
		videoProxy()->moveToThread(m_drawThread);
	}
	connect(m_drawThread, &QThread::started, m_painter.get(), &PainterGL::start);
	connect(m_painter.get(), &PainterGL::started, this, [this] {
		m_hasStarted = true;
		resizePainter();
		emit drawingStarted();
	});

	lockAspectRatio(isAspectRatioLocked());
	lockIntegerScaling(isIntegerScalingLocked());
	interframeBlending(hasInterframeBlending());
	showOSDMessages(isShowOSD());
	filter(isFiltered());

	m_drawThread->start();

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
	messagePainter()->resize(size(), isAspectRatioLocked(), devicePixelRatioF());
#else
	messagePainter()->resize(size(), isAspectRatioLocked(), devicePixelRatio());
#endif
	setUpdatesEnabled(false);
}

void DisplayGL::stopDrawing() {
	if (m_drawThread) {
		m_isDrawing = false;
		m_hasStarted = false;
		CoreController::Interrupter interrupter(m_context);
		QMetaObject::invokeMethod(m_painter.get(), "stop", Qt::BlockingQueuedConnection);
		m_drawThread->exit();
		m_drawThread->wait();
		m_drawThread = nullptr;
		setUpdatesEnabled(true);
	}
	m_context.reset();
}

void DisplayGL::pauseDrawing() {
	if (m_drawThread) {
		m_isDrawing = false;
		CoreController::Interrupter interrupter(m_context);
		QMetaObject::invokeMethod(m_painter.get(), "pause", Qt::BlockingQueuedConnection);
#ifndef Q_OS_MAC
		setUpdatesEnabled(true);
#endif
	}
}

void DisplayGL::unpauseDrawing() {
	if (m_drawThread) {
		m_isDrawing = true;
		CoreController::Interrupter interrupter(m_context);
		QMetaObject::invokeMethod(m_painter.get(), "unpause", Qt::BlockingQueuedConnection);
#ifndef Q_OS_MAC
		setUpdatesEnabled(false);
#endif
	}
}

void DisplayGL::forceDraw() {
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter.get(), "forceDraw");
	}
}

void DisplayGL::lockAspectRatio(bool lock) {
	Display::lockAspectRatio(lock);
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter.get(), "lockAspectRatio", Q_ARG(bool, lock));
	}
}

void DisplayGL::lockIntegerScaling(bool lock) {
	Display::lockIntegerScaling(lock);
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter.get(), "lockIntegerScaling", Q_ARG(bool, lock));
	}
}

void DisplayGL::interframeBlending(bool enable) {
	Display::interframeBlending(enable);
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter.get(), "interframeBlending", Q_ARG(bool, enable));
	}
}

void DisplayGL::showOSDMessages(bool enable) {
	Display::showOSDMessages(enable);
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter.get(), "showOSD", Q_ARG(bool, enable));
	}
}

void DisplayGL::filter(bool filter) {
	Display::filter(filter);
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter.get(), "filter", Q_ARG(bool, filter));
	}
}

void DisplayGL::framePosted() {
	if (m_drawThread) {
		m_painter->enqueue(m_context->drawContext());
		QMetaObject::invokeMethod(m_painter.get(), "draw");
	}
}

void DisplayGL::setShaders(struct VDir* shaders) {
	if (m_drawThread) {
		QMetaObject::invokeMethod(m_painter.get(), "setShaders", Qt::BlockingQueuedConnection, Q_ARG(struct VDir*, shaders));
	} else {
		m_painter->setShaders(shaders);
	}
}

void DisplayGL::clearShaders() {
	QMetaObject::invokeMethod(m_painter.get(), "clearShaders");
}

void DisplayGL::resizeContext() {
	if (m_drawThread) {
		CoreController::Interrupter interrupter(m_context);
		QMetaObject::invokeMethod(m_painter.get(), "resizeContext");
	}
}

void DisplayGL::setVideoScale(int scale) {
	if (m_drawThread) {
		CoreController::Interrupter interrupter(m_context);
		mCoreConfigSetIntValue(&m_context->thread()->core->config, "videoScale", scale);
		QMetaObject::invokeMethod(m_painter.get(), "resizeContext");
	}
}

void DisplayGL::resizeEvent(QResizeEvent* event) {
	Display::resizeEvent(event);
	resizePainter();
}

void DisplayGL::resizePainter() {
	if (m_drawThread && m_hasStarted) {
		QMetaObject::invokeMethod(m_painter.get(), "resize", Qt::BlockingQueuedConnection, Q_ARG(QSize, size()));
	}
}

void DisplayGL::setVideoProxy(std::shared_ptr<VideoProxy> proxy) {
	Display::setVideoProxy(proxy);
	if (m_drawThread && proxy) {
		proxy->moveToThread(m_drawThread);
	}
	m_painter->setVideoProxy(proxy);
}

int DisplayGL::framebufferHandle() {
	return m_painter->glTex();
}

PainterGL::PainterGL(QWindow* surface, const QSurfaceFormat& format)
	: m_surface(surface)
	, m_format(format)
{
	for (auto& buf : m_buffers) {
		m_free.append(&buf.front());
	}
}

PainterGL::~PainterGL() {
	if (m_gl) {
		destroy();
	}
}

void PainterGL::makeCurrent() {
	m_gl->makeCurrent(m_surface);
#if defined(_WIN32) && defined(USE_EPOXY)
	epoxy_handle_external_wglMakeCurrent();
#endif
}

void PainterGL::create() {
	m_gl = std::make_unique<QOpenGLContext>();
	m_gl->setFormat(m_format);
	m_gl->create();
	makeCurrent();

	auto version = m_gl->format().version();
	QStringList extensions = QString(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS))).split(' ');

	int forceVersion = 0;
	if (m_format.majorVersion() < 2) {
		forceVersion = 1;
	}

	if ((version == qMakePair(2, 1) && !extensions.contains("GL_ARB_framebuffer_object")) || version == qMakePair(2, 0)) {
		QSurfaceFormat newFormat(m_format);
		newFormat.setVersion(1, 4);
		forceVersion = 1;
		m_gl->doneCurrent();
		m_gl->setFormat(newFormat);
		m_gl->create();
		makeCurrent();
	}

#ifdef BUILD_GL
	mGLContext* glBackend;
#endif
#ifdef BUILD_GLES2
	mGLES2Context* gl2Backend;
#endif

	m_window = std::make_unique<QOpenGLPaintDevice>();

#ifdef BUILD_GLES2
	version = m_gl->format().version();
	extensions = QString(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS))).split(' ');
	if (forceVersion != 1 && ((version == qMakePair(2, 1) && extensions.contains("GL_ARB_framebuffer_object")) || version.first > 2)) {
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
		if (!painter->m_gl->isValid()) {
			return;
		}
		painter->m_gl->swapBuffers(painter->m_surface);
		painter->makeCurrent();
	};

	m_backend->init(m_backend, 0);
#ifdef BUILD_GLES2
	if (m_supportsShaders) {
		m_shader.preprocessShader = static_cast<void*>(&reinterpret_cast<mGLES2Context*>(m_backend)->initialShader);
	}
#endif

	m_backend->user = this;
	m_backend->filter = false;
	m_backend->lockAspectRatio = false;
	m_backend->interframeBlending = false;
}

void PainterGL::destroy() {
	if (!m_gl) {
		return;
	}
	makeCurrent();
	if (m_context) {
		if (m_videoProxy) {
			m_videoProxy->detach(m_context.get());
		}
		m_context->setFramebufferHandle(-1);
		if (m_videoProxy) {
			m_videoProxy->processData();
		}
	}
#ifdef BUILD_GLES2
	if (m_shader.passes) {
		mGLES2ShaderFree(&m_shader);
	}
#endif
	m_backend->deinit(m_backend);
	m_gl->doneCurrent();
	m_window.reset();
	m_gl.reset();

	free(m_backend);
	m_backend = nullptr;
}

void PainterGL::setContext(std::shared_ptr<CoreController> context) {
	m_context = context;
}

void PainterGL::resizeContext() {
	if (!m_context) {
		return;
	}

	if (m_started) {
		mCore* core = m_context->thread()->core;
		core->reloadConfigOption(core, "videoScale", NULL);
	}

	QSize size = m_context->screenDimensions();
	m_backend->setDimensions(m_backend, size.width(), size.height());
}

void PainterGL::setMessagePainter(MessagePainter* messagePainter) {
	m_messagePainter = messagePainter;
}

void PainterGL::resize(const QSize& size) {
	m_size = size;
	m_window->setSize(m_size);
	if (m_started && !m_active) {
		forceDraw();
	}
}

void PainterGL::lockAspectRatio(bool lock) {
	m_backend->lockAspectRatio = lock;
	resize(m_size);
}

void PainterGL::lockIntegerScaling(bool lock) {
	m_backend->lockIntegerScaling = lock;
	resize(m_size);
}

void PainterGL::interframeBlending(bool enable) {
	m_backend->interframeBlending = enable;
}

void PainterGL::showOSD(bool enable) {
	m_showOSD = enable;
}

void PainterGL::filter(bool filter) {
	m_backend->filter = filter;
	if (m_started && !m_active) {
		forceDraw();
	}
}

void PainterGL::start() {
	if (!m_gl) {
		create();
	}
	makeCurrent();

#ifdef BUILD_GLES2
	if (m_supportsShaders && m_shader.passes) {
		mGLES2ShaderAttach(reinterpret_cast<mGLES2Context*>(m_backend), static_cast<mGLES2Shader*>(m_shader.passes), m_shader.nPasses);
	}
#endif
	resizeContext();

	m_buffer = nullptr;
	m_active = true;
	m_started = true;
	emit started();
}

void PainterGL::draw() {
	if (!m_active || m_queue.isEmpty()) {
		return;
	}
	mCoreSync* sync = &m_context->thread()->impl->sync;
	mCoreSyncWaitFrameStart(sync);
	dequeue();
	if (m_videoProxy) {
		// Only block on the next frame if we're trying to run at roughly 60fps via audio
		m_videoProxy->setBlocking(sync->audioWait && std::abs(60.f - sync->fpsTarget) < 0.1f);
	}
	if (!m_delayTimer.isValid()) {
		m_delayTimer.start();
	} else if (sync->audioWait || sync->videoFrameWait) {
		while (m_delayTimer.nsecsElapsed() + 2000000 < 1000000000 / sync->fpsTarget) {
			QThread::usleep(500);
		}
		m_delayTimer.restart();
	}
	mCoreSyncWaitFrameEnd(sync);

	forceDraw();
}

void PainterGL::forceDraw() {
	m_painter.begin(m_window.get());
	performDraw();
	m_painter.end();
	if (!m_context->thread()->impl->sync.audioWait && !m_context->thread()->impl->sync.videoFrameWait) {
		if (m_delayTimer.elapsed() < 1000 / m_surface->screen()->refreshRate()) {
			return;
		}
		m_delayTimer.restart();
	}
	m_backend->swap(m_backend);
}

void PainterGL::stop() {
	m_active = false;
	m_started = false;
	dequeueAll();
	m_backend->clear(m_backend);
	m_backend->swap(m_backend);
	if (m_videoProxy) {
		m_videoProxy->reset();
	}
	destroy();
	moveToThread(m_surface->thread());
	if (m_videoProxy) {
		m_videoProxy->moveToThread(m_surface->thread());
	}
}

void PainterGL::pause() {
	m_active = false;
}

void PainterGL::unpause() {
	m_active = true;
}

void PainterGL::performDraw() {
	m_painter.beginNativePainting();
	float r = m_surface->devicePixelRatio();
	m_backend->resized(m_backend, m_size.width() * r, m_size.height() * r);
	if (m_buffer) {
		m_backend->postFrame(m_backend, m_buffer);
	}
	m_backend->drawFrame(m_backend);
	m_painter.endNativePainting();
	if (m_showOSD && m_messagePainter) {
		m_messagePainter->paint(&m_painter);
	}
}

void PainterGL::enqueue(const uint32_t* backing) {
	QMutexLocker locker(&m_mutex);
	uint32_t* buffer = nullptr;
	if (backing) {
		if (m_free.isEmpty()) {
			buffer = m_queue.dequeue();
		} else {
			buffer = m_free.takeLast();
		}
		if (buffer) {
			QSize size = m_context->screenDimensions();
			memcpy(buffer, backing, size.width() * size.height() * BYTES_PER_PIXEL);
		}
	}
	m_queue.enqueue(buffer);
}

void PainterGL::dequeue() {
	QMutexLocker locker(&m_mutex);
	if (m_queue.isEmpty()) {
		return;
	}
	uint32_t* buffer = m_queue.dequeue();
	if (m_buffer) {
		m_free.append(m_buffer);
		m_buffer = nullptr;
	}
	m_buffer = buffer;
}

void PainterGL::dequeueAll() {
	uint32_t* buffer = 0;
	m_mutex.lock();
	while (!m_queue.isEmpty()) {
		buffer = m_queue.dequeue();
		if (buffer) {
			m_free.append(buffer);
		}
	}
	if (buffer) {
		m_backend->postFrame(m_backend, buffer);
	}
	if (m_buffer) {
		m_free.append(m_buffer);
		m_buffer = nullptr;
	}
	m_mutex.unlock();
}

void PainterGL::setVideoProxy(std::shared_ptr<VideoProxy> proxy) {
	m_videoProxy = proxy;
}

void PainterGL::setShaders(struct VDir* dir) {
	if (!supportsShaders()) {
		return;
	}
#ifdef BUILD_GLES2
	if (!m_started) {
		return; // TODO
	}
	if (m_shader.passes) {
		mGLES2ShaderDetach(reinterpret_cast<mGLES2Context*>(m_backend));
		mGLES2ShaderFree(&m_shader);
	}
	mGLES2ShaderLoad(&m_shader, dir);
	mGLES2ShaderAttach(reinterpret_cast<mGLES2Context*>(m_backend), static_cast<mGLES2Shader*>(m_shader.passes), m_shader.nPasses);
#endif
}

void PainterGL::clearShaders() {
	if (!supportsShaders()) {
		return;
	}
#ifdef BUILD_GLES2
	if (!m_started) {
		return; // TODO
	}
	if (m_shader.passes) {
		mGLES2ShaderDetach(reinterpret_cast<mGLES2Context*>(m_backend));
		mGLES2ShaderFree(&m_shader);
	}
#endif
}

VideoShader* PainterGL::shaders() {
	return &m_shader;
}

int PainterGL::glTex() {
#ifdef BUILD_GLES2
	if (supportsShaders()) {
		mGLES2Context* gl2Backend = reinterpret_cast<mGLES2Context*>(m_backend);
		return gl2Backend->tex;
	}
#endif
#ifdef BUILD_GL
	mGLContext* glBackend = reinterpret_cast<mGLContext*>(m_backend);
	return glBackend->tex[0];
#else
	return -1;
#endif
}

#endif
