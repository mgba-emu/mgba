/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DisplayGL.h"

#if defined(BUILD_GL) || defined(BUILD_GLES2) || defined(BUILD_GLES3) || defined(USE_EPOXY)

#include <QApplication>
#include <QMutexLocker>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLPaintDevice>
#include <QResizeEvent>
#include <QScreen>
#include <QTimer>
#include <QWindow>

#include <cmath>

#include <mgba/core/core.h>
#include <mgba-util/math.h>
#ifdef BUILD_GL
#include "platform/opengl/gl.h"
#endif
#if defined(BUILD_GLES2) || defined(BUILD_GLES3)
#include "platform/opengl/gles2.h"
#ifdef _WIN32
#include <epoxy/wgl.h>
#endif
#endif

using namespace QGBA;

QHash<QSurfaceFormat, bool> DisplayGL::s_supports;

uint qHash(const QSurfaceFormat& format, uint seed) {
	QByteArray representation;
	QDataStream stream(&representation, QIODevice::WriteOnly);
	stream << format.version() << format.renderableType() << format.profile();
	return qHash(representation, seed);
}

DisplayGL::DisplayGL(const QSurfaceFormat& format, QWidget* parent)
	: Display(parent)
{
	setAttribute(Qt::WA_NativeWindow);
	windowHandle()->create();

	m_painter = std::make_unique<PainterGL>(windowHandle(), format);
#ifdef Q_OS_MAC
	m_drawThread = qApp->thread();
#else
	m_drawThread = &_m_drawThread;
	m_drawThread->setObjectName("Painter Thread");
#endif
	m_painter->setThread(m_drawThread);

	connect(m_painter.get(), &PainterGL::started, this, [this] {
		m_hasStarted = true;
		resizePainter();
		emit drawingStarted();
	});

#ifdef Q_OS_MAC
	m_painter->create();
#else
	connect(&m_drawThread, &QThread::started, m_painter.get(), &PainterGL::create);
	m_drawThread.start();
#endif
}

DisplayGL::~DisplayGL() {
	stopDrawing();
#ifdef Q_OS_MAC
	if (QThread::currentThread() == qApp->thread())
		m_painter->destroy();
	else
#endif
	QMetaObject::invokeMethod(m_painter.get(), "destroy", Qt::BlockingQueuedConnection);
#ifndef Q_OS_MAC
	m_drawThread->exit();
	m_drawThread->wait();
#endif
}

bool DisplayGL::supportsShaders() const {
	return m_painter->supportsShaders();
}

VideoShader* DisplayGL::shaders() {
	VideoShader* shaders = nullptr;
#ifdef Q_OS_MAC
	if (QThread::currentThread() == qApp->thread())
		shaders = m_painter->shaders();
	else
#endif
	QMetaObject::invokeMethod(m_painter.get(), "shaders", Qt::BlockingQueuedConnection, Q_RETURN_ARG(VideoShader*, shaders));
	return shaders;
}

void DisplayGL::startDrawing(std::shared_ptr<CoreController> controller) {
	if (m_isDrawing) {
		return;
	}
	m_isDrawing = true;
	m_painter->setContext(controller);
	m_painter->setMessagePainter(messagePainter());
	m_context = controller;
	if (videoProxy()) {
		videoProxy()->moveToThread(m_drawThread);
	}

	lockAspectRatio(isAspectRatioLocked());
	lockIntegerScaling(isIntegerScalingLocked());
	interframeBlending(hasInterframeBlending());
	showOSDMessages(isShowOSD());
	filter(isFiltered());

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
	messagePainter()->resize(size(), isAspectRatioLocked(), devicePixelRatioF());
#else
	messagePainter()->resize(size(), isAspectRatioLocked(), devicePixelRatio());
#endif
	CoreController::Interrupter interrupter(controller);
#ifdef Q_OS_MAC
	if (QThread::currentThread() == qApp->thread())
		m_painter->start();
	else
#endif
	QMetaObject::invokeMethod(m_painter.get(), "start");
	setUpdatesEnabled(false);
}

bool DisplayGL::supportsFormat(const QSurfaceFormat& format) {
	if (!s_supports.contains(format)) {
		QOpenGLContext context;
		context.setFormat(format);
		if (!context.create()) {
			s_supports[format] = false;
			return false;
		}
		auto foundVersion = context.format().version();
		if (foundVersion == format.version()) {
			// Match!
			s_supports[format] = true;
		} else if (format.version() >= qMakePair(3, 2) && foundVersion > format.version()) {
			// At least as good
			s_supports[format] = true;
		} else if (format.majorVersion() == 1 && (foundVersion < qMakePair(3, 0) ||
		           context.format().profile() == QSurfaceFormat::CompatibilityProfile ||
		           context.format().testOption(QSurfaceFormat::DeprecatedFunctions))) {
			// Supports the old stuff
			QOffscreenSurface surface;
			surface.create();
			if (!context.makeCurrent(&surface)) {
				s_supports[format] = false;
				return false;
			}
#ifdef Q_OS_WIN
			QLatin1String renderer(reinterpret_cast<const char*>(context.functions()->glGetString(GL_RENDERER)));
			if (renderer == "GDI Generic") {
				// Windows' software OpenGL 1.1 implementation is not sufficient
				s_supports[format] = false;
				return false;
			}
#endif
			s_supports[format] = context.hasExtension("GL_EXT_blend_color"); // Core as of 1.2
			context.doneCurrent();
		} else if (!context.isOpenGLES() && format.version() >= qMakePair(2, 1) && foundVersion < qMakePair(3, 0) && foundVersion >= qMakePair(2, 1)) {
			// Weird edge case we support if ARB_framebuffer_object is present
			QOffscreenSurface surface;
			surface.create();
			if (!context.makeCurrent(&surface)) {
				s_supports[format] = false;
				return false;
			}
			s_supports[format] = context.hasExtension("GL_ARB_framebuffer_object");
			context.doneCurrent();
		} else {
			// No match
			s_supports[format] = false;
		}
	}
	return s_supports[format];
}

void DisplayGL::stopDrawing() {
	if (m_hasStarted || m_isDrawing) {
		m_isDrawing = false;
		m_hasStarted = false;
		CoreController::Interrupter interrupter(m_context);
#ifdef Q_OS_MAC
		if (QThread::currentThread() == qApp->thread())
			m_painter->stop();
		else
#endif
		QMetaObject::invokeMethod(m_painter.get(), "stop", Qt::BlockingQueuedConnection);
		setUpdatesEnabled(true);
	}
	m_context.reset();
}

void DisplayGL::pauseDrawing() {
	if (m_hasStarted) {
		m_isDrawing = false;
#ifdef Q_OS_MAC
		if (QThread::currentThread() == qApp->thread())
			m_painter->pause();
		else
#endif
		QMetaObject::invokeMethod(m_painter.get(), "pause", Qt::BlockingQueuedConnection);
#ifndef Q_OS_MAC
		setUpdatesEnabled(true);
#endif
	}
}

void DisplayGL::unpauseDrawing() {
	if (m_hasStarted) {
		m_isDrawing = true;
#ifdef Q_OS_MAC
		if (QThread::currentThread() == qApp->thread())
			m_painter->unpause();
		else
#endif
		QMetaObject::invokeMethod(m_painter.get(), "unpause", Qt::BlockingQueuedConnection);
#ifndef Q_OS_MAC
		setUpdatesEnabled(false);
#endif
	}
}

void DisplayGL::forceDraw() {
	if (m_hasStarted) {
#ifdef Q_OS_MAC
		if (QThread::currentThread() == qApp->thread())
			m_painter->forceDraw();
		else
#endif
		QMetaObject::invokeMethod(m_painter.get(), "forceDraw");
	}
}

void DisplayGL::lockAspectRatio(bool lock) {
	Display::lockAspectRatio(lock);
#ifdef Q_OS_MAC
	if (QThread::currentThread() == qApp->thread())
		m_painter->lockAspectRatio(lock);
	else
#endif
	QMetaObject::invokeMethod(m_painter.get(), "lockAspectRatio", Q_ARG(bool, lock));
}

void DisplayGL::lockIntegerScaling(bool lock) {
	Display::lockIntegerScaling(lock);
#ifdef Q_OS_MAC
	if (QThread::currentThread() == qApp->thread())
		m_painter->lockIntegerScaling(lock);
	else
#endif
	QMetaObject::invokeMethod(m_painter.get(), "lockIntegerScaling", Q_ARG(bool, lock));
}

void DisplayGL::interframeBlending(bool enable) {
	Display::interframeBlending(enable);
#ifdef Q_OS_MAC
	if (QThread::currentThread() == qApp->thread())
		m_painter->interframeBlending(enable);
	else
#endif
	QMetaObject::invokeMethod(m_painter.get(), "interframeBlending", Q_ARG(bool, enable));
}

void DisplayGL::showOSDMessages(bool enable) {
	Display::showOSDMessages(enable);
#ifdef Q_OS_MAC
	if (QThread::currentThread() == qApp->thread())
		m_painter->showOSD(enable);
	else
#endif
	QMetaObject::invokeMethod(m_painter.get(), "showOSD", Q_ARG(bool, enable));
}

void DisplayGL::filter(bool filter) {
	Display::filter(filter);
#ifdef Q_OS_MAC
	if (QThread::currentThread() == qApp->thread())
		m_painter->filter(filter);
	else
#endif
	QMetaObject::invokeMethod(m_painter.get(), "filter", Q_ARG(bool, filter));
}

void DisplayGL::framePosted() {
	m_painter->enqueue(m_context->drawContext());
#ifdef Q_OS_MAC
	if (QThread::currentThread() == qApp->thread())
		m_painter->draw();
	else
#endif
	QMetaObject::invokeMethod(m_painter.get(), "draw");
}

void DisplayGL::setShaders(struct VDir* shaders) {
#ifdef Q_OS_MAC
	if (QThread::currentThread() == qApp->thread())
		m_painter->setShaders(shaders);
	else
#endif
	QMetaObject::invokeMethod(m_painter.get(), "setShaders", Qt::BlockingQueuedConnection, Q_ARG(struct VDir*, shaders));
}

void DisplayGL::clearShaders() {
#ifdef Q_OS_MAC
	if (QThread::currentThread() == qApp->thread())
		m_painter->clearShaders();
	else
#endif
	QMetaObject::invokeMethod(m_painter.get(), "clearShaders", Qt::BlockingQueuedConnection);
}

void DisplayGL::resizeContext() {
	m_painter->interrupt();
#ifdef Q_OS_MAC
	if (QThread::currentThread() == qApp->thread())
		m_painter->resizeContext();
	else
#endif
	QMetaObject::invokeMethod(m_painter.get(), "resizeContext");
}

void DisplayGL::setVideoScale(int scale) {
	if (m_context) {
		m_painter->interrupt();
		mCoreConfigSetIntValue(&m_context->thread()->core->config, "videoScale", scale);
	}
#ifdef Q_OS_MAC
	if (QThread::currentThread() == qApp->thread())
		m_painter->resizeContext();
	else
#endif
	QMetaObject::invokeMethod(m_painter.get(), "resizeContext");
}

void DisplayGL::resizeEvent(QResizeEvent* event) {
	Display::resizeEvent(event);
	resizePainter();
}

void DisplayGL::resizePainter() {
	if (m_hasStarted) {
#ifdef Q_OS_MAC
		if (QThread::currentThread() == qApp->thread())
			m_painter->resize(size());
		else
#endif
		QMetaObject::invokeMethod(m_painter.get(), "resize", Qt::BlockingQueuedConnection, Q_ARG(QSize, size()));
	}
}

void DisplayGL::setVideoProxy(std::shared_ptr<VideoProxy> proxy) {
	Display::setVideoProxy(proxy);
	if (proxy) {
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
	m_supportsShaders = m_format.version() >= qMakePair(2, 0);
	for (auto& buf : m_buffers) {
		m_free.append(&buf.front());
	}
	connect(&m_drawTimer, &QTimer::timeout, this, &PainterGL::draw);
	m_drawTimer.setSingleShot(true);
}

PainterGL::~PainterGL() {
	if (m_gl) {
		destroy();
	}
}

void PainterGL::setThread(QThread* thread) {
	moveToThread(thread);
	m_drawTimer.moveToThread(thread);
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

#ifdef BUILD_GL
	mGLContext* glBackend;
#endif
#if defined(BUILD_GLES2) || defined(BUILD_GLES3)
	mGLES2Context* gl2Backend;
#endif

	m_window = std::make_unique<QOpenGLPaintDevice>();

#if defined(BUILD_GLES2) || defined(BUILD_GLES3)
	auto version = m_format.version();
	if (version >= qMakePair(2, 0)) {
		gl2Backend = static_cast<mGLES2Context*>(malloc(sizeof(mGLES2Context)));
		mGLES2ContextCreate(gl2Backend);
		m_backend = &gl2Backend->d;
	}
#endif

#ifdef BUILD_GL
	 if (!m_backend) {
		glBackend = static_cast<mGLContext*>(malloc(sizeof(mGLContext)));
		mGLContextCreate(glBackend);
		m_backend = &glBackend->d;
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
#if defined(BUILD_GLES2) || defined(BUILD_GLES3)
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
#if defined(BUILD_GLES2) || defined(BUILD_GLES3)
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
	m_interrupter.resume();

	QSize size = m_context->screenDimensions();
	if (m_dims == size) {
		return;
	}
	dequeueAll(false);
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
	makeCurrent();

#if defined(BUILD_GLES2) || defined(BUILD_GLES3)
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
	if (!m_started || m_queue.isEmpty()) {
		return;
	}
	mCoreSync* sync = &m_context->thread()->impl->sync;
	if (!mCoreSyncWaitFrameStart(sync)) {
		mCoreSyncWaitFrameEnd(sync);
		if (!sync->audioWait && !sync->videoFrameWait) {
			return;
		}
		if (m_delayTimer.elapsed() >= 1000 / m_surface->screen()->refreshRate()) {
			return;
		}
		if (!m_drawTimer.isActive()) {
			m_drawTimer.start(1);
		}
		return;
	}
	dequeue();
	bool forceRedraw = !m_videoProxy;
	if (!m_delayTimer.isValid()) {
		m_delayTimer.start();
	} else {
		if (sync->audioWait || sync->videoFrameWait) {
			while (m_delayTimer.nsecsElapsed() + 1000000 < 1000000000 / sync->fpsTarget) {
				QThread::usleep(500);
			}
			forceRedraw = sync->videoFrameWait;
		}
		if (!forceRedraw) {
			forceRedraw = m_delayTimer.nsecsElapsed() + 1000000 >= 1000000000 / m_surface->screen()->refreshRate();
		}
	}
	mCoreSyncWaitFrameEnd(sync);

	if (forceRedraw) {
		m_delayTimer.restart();
		performDraw();
		m_backend->swap(m_backend);
	}
}

void PainterGL::forceDraw() {
	performDraw();
	if (!m_context->thread()->impl->sync.audioWait && !m_context->thread()->impl->sync.videoFrameWait) {
		if (m_delayTimer.elapsed() < 1000 / m_surface->screen()->refreshRate()) {
			return;
		}
		m_delayTimer.restart();
	}
	m_backend->swap(m_backend);
}

void PainterGL::stop() {
	m_drawTimer.stop();
	m_active = false;
	m_started = false;
	dequeueAll(false);
	if (m_context) {
		if (m_videoProxy) {
			m_videoProxy->detach(m_context.get());
		}
		m_context->setFramebufferHandle(-1);
		m_context.reset();
		if (m_videoProxy) {
			m_videoProxy->processData();
		}
	}
	if (m_videoProxy) {
		m_videoProxy->reset();
		m_videoProxy->moveToThread(m_surface->thread());
		m_videoProxy.reset();
	}
	m_backend->clear(m_backend);
	m_backend->swap(m_backend);
}

void PainterGL::pause() {
	m_drawTimer.stop();
	m_active = false;
	dequeueAll(true);
}

void PainterGL::unpause() {
	m_active = true;
}

void PainterGL::performDraw() {
	float r = m_surface->devicePixelRatio();
	m_backend->resized(m_backend, m_size.width() * r, m_size.height() * r);
	if (m_buffer) {
		m_backend->postFrame(m_backend, m_buffer);
	}
	m_backend->drawFrame(m_backend);
	if (m_showOSD && m_messagePainter) {
		m_painter.begin(m_window.get());
		m_messagePainter->paint(&m_painter);
		m_painter.end();
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

void PainterGL::dequeueAll(bool keep) {
	QMutexLocker locker(&m_mutex);
	uint32_t* buffer = 0;
	while (!m_queue.isEmpty()) {
		buffer = m_queue.dequeue();
		if (keep) {
			if (m_buffer && buffer) {
				m_free.append(m_buffer);
				m_buffer = buffer;
			}
		} else if (buffer) {
			m_free.append(buffer);
		}
	}
	if (m_buffer && !keep) {
		m_free.append(m_buffer);
		m_buffer = nullptr;
	}
}

void PainterGL::setVideoProxy(std::shared_ptr<VideoProxy> proxy) {
	m_videoProxy = proxy;
}

void PainterGL::interrupt() {
	m_interrupter.interrupt(m_context);
}

void PainterGL::setShaders(struct VDir* dir) {
	if (!supportsShaders()) {
		return;
	}
#if defined(BUILD_GLES2) || defined(BUILD_GLES3)
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
#if defined(BUILD_GLES2) || defined(BUILD_GLES3)
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
#if defined(BUILD_GLES2) || defined(BUILD_GLES3)
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
