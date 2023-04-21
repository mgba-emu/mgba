/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DisplayGL.h"

#if defined(BUILD_GL) || defined(BUILD_GLES2) || defined(BUILD_GLES3) || defined(USE_EPOXY)

#include <QApplication>
#include <QMutexLocker>
#include <QOpenGLFunctions>
#ifdef QT_OPENGL_ES_2
#include <QOpenGLFunctions_ES2>
using QOpenGLFunctions_Baseline = QOpenGLFunctions_ES2;
#else
#include <QOpenGLFunctions_3_2_Core>
using QOpenGLFunctions_Baseline = QOpenGLFunctions_3_2_Core;
#endif
#include <QOpenGLPaintDevice>
#include <QResizeEvent>
#include <QScreen>
#include <QTimer>
#include <QWindow>
#include <QVBoxLayout>

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

#ifdef _WIN32
#include <windows.h>
#elif defined(Q_OS_MAC)
#include <OpenGL/OpenGL.h>
#endif
#ifdef USE_GLX
#define GLX_GLXEXT_PROTOTYPES
typedef struct _XDisplay Display;
#include <GL/glx.h>
#include <GL/glxext.h>
#endif
#ifdef USE_EGL
#include <EGL/egl.h>
#endif

#ifdef _WIN32
#define OVERHEAD_NSEC 1000000
#else
#define OVERHEAD_NSEC 300000
#endif

#include "OpenGLBug.h"

using namespace QGBA;

QHash<QSurfaceFormat, bool> DisplayGL::s_supports;

uint qHash(const QSurfaceFormat& format, uint seed) {
	QByteArray representation;
	QDataStream stream(&representation, QIODevice::WriteOnly);
	stream << format.version() << format.renderableType() << format.profile();
	return qHash(representation, seed);
}

mGLWidget::mGLWidget(QWidget* parent)
	: QOpenGLWidget(parent)
{
	setUpdateBehavior(QOpenGLWidget::PartialUpdate);

	connect(&m_refresh, &QTimer::timeout, this, static_cast<void (QWidget::*)()>(&QWidget::update));
}

mGLWidget::~mGLWidget() {
	// This is needed for unique_ptr<QOpenGLPaintDevice> to work
}

void mGLWidget::initializeGL() {
	m_vao = std::make_unique<QOpenGLVertexArrayObject>();
	m_vao->create();

	m_program = std::make_unique<QOpenGLShaderProgram>();
	m_program->create();

	m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, R"(#version 150 core
		in vec4 position;
		out vec2 texCoord;
		void main() {
			gl_Position = position;
			texCoord = (position.st + 1.0) * 0.5;
		})");

	m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, R"(#version 150 core
		in vec2 texCoord;
		out vec4 color;
		uniform sampler2D tex;
		void main() {
			color = vec4(texture(tex, texCoord).rgb, 1.0);
		})");

	m_program->link();
	m_program->setUniformValue("tex", 0);
	m_positionLocation = m_program->attributeLocation("position");

	m_vaoDone = false;
	m_tex = 0;

	m_paintDev = std::make_unique<QOpenGLPaintDevice>();
}

bool mGLWidget::finalizeVAO() {
	if (!context() || !m_vao) {
		return false;
	}
	QOpenGLFunctions_Baseline* fn = context()->versionFunctions<QOpenGLFunctions_Baseline>();
	if (!fn) {
		return false;
	}
	fn->glGetError(); // Clear the error
	m_vao->bind();
	fn->glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	fn->glEnableVertexAttribArray(m_positionLocation);
	fn->glVertexAttribPointer(m_positionLocation, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	m_vao->release();
	if (fn->glGetError() == GL_NO_ERROR) {
		m_vaoDone = true;
	}
	return m_vaoDone;
}

void mGLWidget::reset() {
	m_vaoDone = false;
}

void mGLWidget::paintGL() {
	if (!m_vaoDone && !finalizeVAO()) {
		return;
	}
	if (!m_tex) {
		m_refresh.start(10);
		return;
	}
	QOpenGLFunctions_Baseline* fn = context()->versionFunctions<QOpenGLFunctions_Baseline>();
	m_program->bind();
	m_vao->bind();
	fn->glBindTexture(GL_TEXTURE_2D, m_tex);
	fn->glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	fn->glBindTexture(GL_TEXTURE_2D, 0);
	m_vao->release();
	m_program->release();

	// TODO: Better timing
	++m_refreshResidue;
	if (m_refreshResidue == 3) {
		m_refresh.start(16);
		m_refreshResidue = 0;
	} else {
		m_refresh.start(17);
	}

	if (m_showOSD && m_messagePainter) {
		qreal r = window()->devicePixelRatio();
		m_paintDev->setDevicePixelRatio(r);
		m_paintDev->setSize(size() * r);
		QPainter painter(m_paintDev.get());
		m_messagePainter->paint(&painter);
		painter.end();
	}
}

void mGLWidget::setMessagePainter(MessagePainter* messagePainter) {
	m_messagePainter = messagePainter;
}

void mGLWidget::setShowOSD(bool showOSD) {
	m_showOSD = showOSD;
}

DisplayGL::DisplayGL(const QSurfaceFormat& format, QWidget* parent)
	: Display(parent)
{
	setAttribute(Qt::WA_NativeWindow);
	window()->windowHandle()->setFormat(format);
	windowHandle()->create();

#ifdef USE_SHARE_WIDGET
	bool useShareWidget = true;
#else
	// TODO: Does using this on Wayland help?
	bool useShareWidget = false;
#endif

	if (useShareWidget) {
		m_gl = new mGLWidget;
		m_gl->setAttribute(Qt::WA_NativeWindow);
		m_gl->setFormat(format);
		m_gl->setMessagePainter(messagePainter());
		QBoxLayout* layout = new QVBoxLayout;
		layout->addWidget(m_gl);
		layout->setContentsMargins(0, 0, 0, 0);
		setLayout(layout);
	} else {
		m_gl = nullptr;
	}

	m_painter = std::make_unique<PainterGL>(windowHandle(), m_gl, format);
	m_drawThread.setObjectName("Painter Thread");
	m_painter->setThread(&m_drawThread);

	connect(&m_drawThread, &QThread::started, m_painter.get(), &PainterGL::create);
	connect(m_painter.get(), &PainterGL::started, this, [this] {
		m_hasStarted = true;
		resizePainter();
		emit drawingStarted();
	});
	m_drawThread.start();
}

DisplayGL::~DisplayGL() {
	stopDrawing();
	QMetaObject::invokeMethod(m_painter.get(), "destroy", Qt::BlockingQueuedConnection);
	m_drawThread.exit();
	m_drawThread.wait();
}

bool DisplayGL::supportsShaders() const {
	return m_painter->supportsShaders();
}

VideoShader* DisplayGL::shaders() {
	VideoShader* shaders = nullptr;
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
		videoProxy()->moveToThread(&m_drawThread);
	}

	lockAspectRatio(isAspectRatioLocked());
	lockIntegerScaling(isIntegerScalingLocked());
	interframeBlending(hasInterframeBlending());
	showOSDMessages(isShowOSD());
	showFrameCounter(isShowFrameCounter());
	filter(isFiltered());

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
	messagePainter()->resize(size(), devicePixelRatioF());
#else
	messagePainter()->resize(size(), devicePixelRatio());
#endif

	CoreController::Interrupter interrupter(m_context);
	QMetaObject::invokeMethod(m_painter.get(), "start");
	if (!m_gl) {
		if (shouldDisableUpdates()) {
			setUpdatesEnabled(false);
		}
	} else {
		show();
		m_gl->reset();
	}

	QTimer::singleShot(8, this, &DisplayGL::updateContentSize);
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
		m_painter->stop();
		if (m_gl) {
			hide();
		}
		setUpdatesEnabled(true);
	}
	m_context.reset();
}

void DisplayGL::pauseDrawing() {
	if (m_hasStarted) {
		m_isDrawing = false;
		QMetaObject::invokeMethod(m_painter.get(), "pause", Qt::BlockingQueuedConnection);
		if (!shouldDisableUpdates()) {
			setUpdatesEnabled(true);
		}
	}
}

void DisplayGL::unpauseDrawing() {
	if (m_hasStarted) {
		m_isDrawing = true;
		QMetaObject::invokeMethod(m_painter.get(), "unpause", Qt::BlockingQueuedConnection);
		if (!m_gl && shouldDisableUpdates()) {
			setUpdatesEnabled(false);
		}
		QMetaObject::invokeMethod(this, "updateContentSize", Qt::QueuedConnection);
	}
}

void DisplayGL::forceDraw() {
	if (m_hasStarted) {
		QMetaObject::invokeMethod(m_painter.get(), "forceDraw");
		QMetaObject::invokeMethod(this, "updateContentSize", Qt::QueuedConnection);
	}
}

void DisplayGL::lockAspectRatio(bool lock) {
	Display::lockAspectRatio(lock);
	QMetaObject::invokeMethod(m_painter.get(), "lockAspectRatio", Q_ARG(bool, lock));
}

void DisplayGL::lockIntegerScaling(bool lock) {
	Display::lockIntegerScaling(lock);
	QMetaObject::invokeMethod(m_painter.get(), "lockIntegerScaling", Q_ARG(bool, lock));
}

void DisplayGL::interframeBlending(bool enable) {
	Display::interframeBlending(enable);
	QMetaObject::invokeMethod(m_painter.get(), "interframeBlending", Q_ARG(bool, enable));
}

void DisplayGL::showOSDMessages(bool enable) {
	Display::showOSDMessages(enable);
	if (m_gl) {
		m_gl->setShowOSD(enable);
	}
	QMetaObject::invokeMethod(m_painter.get(), "showOSD", Q_ARG(bool, enable));
}

void DisplayGL::showFrameCounter(bool enable) {
	Display::showFrameCounter(enable);
	QMetaObject::invokeMethod(m_painter.get(), "showFrameCounter", Q_ARG(bool, enable));
}

void DisplayGL::filter(bool filter) {
	Display::filter(filter);
	QMetaObject::invokeMethod(m_painter.get(), "filter", Q_ARG(bool, filter));
}

void DisplayGL::swapInterval(int interval) {
	QMetaObject::invokeMethod(m_painter.get(), "swapInterval", Q_ARG(int, interval));
}

void DisplayGL::framePosted() {
	m_painter->enqueue(m_context->drawContext());
	QMetaObject::invokeMethod(m_painter.get(), "draw");
}

void DisplayGL::setShaders(struct VDir* shaders) {
	QMetaObject::invokeMethod(m_painter.get(), "setShaders", Qt::BlockingQueuedConnection, Q_ARG(struct VDir*, shaders));
}

void DisplayGL::clearShaders() {
	QMetaObject::invokeMethod(m_painter.get(), "clearShaders", Qt::BlockingQueuedConnection);
}

void DisplayGL::resizeContext() {
	m_painter->interrupt();
	QMetaObject::invokeMethod(m_painter.get(), "resizeContext");
}

void DisplayGL::setVideoScale(int scale) {
	if (m_context) {
		m_painter->interrupt();
		mCoreConfigSetIntValue(&m_context->thread()->core->config, "videoScale", scale);
	}
	QMetaObject::invokeMethod(m_painter.get(), "resizeContext");
}

void DisplayGL::setBackgroundImage(const QImage& image) {
	QMetaObject::invokeMethod(m_painter.get(), "setBackgroundImage", Q_ARG(const QImage&, image));
	QMetaObject::invokeMethod(this, "updateContentSize", Qt::QueuedConnection);
}

void DisplayGL::resizeEvent(QResizeEvent* event) {
	Display::resizeEvent(event);
	resizePainter();
}

void DisplayGL::resizePainter() {
	if (m_hasStarted) {
		QMetaObject::invokeMethod(m_painter.get(), "resize", Qt::BlockingQueuedConnection, Q_ARG(QSize, size()));
	}
}

bool DisplayGL::shouldDisableUpdates() {
	if (QGuiApplication::platformName() == "windows") {
		return true;
	}
	if (QGuiApplication::platformName() == "xcb") {
		return true;
	}
	return false;
}

void DisplayGL::setVideoProxy(std::shared_ptr<VideoProxy> proxy) {
	Display::setVideoProxy(proxy);
	if (proxy) {
		proxy->moveToThread(&m_drawThread);
	}
	m_painter->setVideoProxy(proxy);
}

void DisplayGL::updateContentSize() {
	QMetaObject::invokeMethod(m_painter.get(), "contentSize", Qt::BlockingQueuedConnection, Q_RETURN_ARG(QSize, m_cachedContentSize));
}

int DisplayGL::framebufferHandle() {
	return m_painter->glTex();
}

PainterGL::PainterGL(QWindow* window, mGLWidget* widget, const QSurfaceFormat& format)
	: m_window(window)
	, m_format(format)
	, m_widget(widget)
{
	if (widget) {
		m_format = widget->format();
		QOffscreenSurface* surface = new QOffscreenSurface;
		surface->setScreen(window->screen());
		surface->setFormat(m_format);
		surface->create();
		m_surface = surface;
	} else {
		m_surface = m_window;
	}
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
	if (m_widget) {
		m_gl->setShareContext(m_widget->context());
	}
	m_gl->create();
	makeCurrent();

#ifdef BUILD_GL
	mGLContext* glBackend;
#endif
#if defined(BUILD_GLES2) || defined(BUILD_GLES3)
	mGLES2Context* gl2Backend;
#endif

	if (!m_widget) {
		m_paintDev = std::make_unique<QOpenGLPaintDevice>();
	}

#if defined(BUILD_GLES2) || defined(BUILD_GLES3)
	if (m_supportsShaders) {
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

#if defined(BUILD_GLES2) || defined(BUILD_GLES3)
		mGLES2Context* gl2Backend = reinterpret_cast<mGLES2Context*>(painter->m_backend);
		if (painter->m_widget && painter->supportsShaders()) {
			QOpenGLFunctions* fn = painter->m_gl->functions();
			fn->glFinish();
			painter->m_widget->setTex(painter->m_finalTex[painter->m_finalTexIdx]);
			painter->m_finalTexIdx ^= 1;
			gl2Backend->finalShader.tex = painter->m_finalTex[painter->m_finalTexIdx];
			mGLES2ContextUseFramebuffer(gl2Backend);
		}
#endif
	};

	m_backend->init(m_backend, 0);
#if defined(BUILD_GLES2) || defined(BUILD_GLES3)
	if (m_supportsShaders) {
		if (m_widget) {
			m_widget->setVBO(gl2Backend->vbo);

			gl2Backend->finalShader.tex = 0;
			mGLES2ContextUseFramebuffer(gl2Backend);
			m_finalTex[0] = gl2Backend->finalShader.tex;

			gl2Backend->finalShader.tex = 0;
			mGLES2ContextUseFramebuffer(gl2Backend);
			m_finalTex[1] = gl2Backend->finalShader.tex;

			m_finalTexIdx = 0;
			gl2Backend->finalShader.tex = m_finalTex[m_finalTexIdx];
		}
		m_shader.preprocessShader = static_cast<void*>(&reinterpret_cast<mGLES2Context*>(m_backend)->initialShader);
	}
#endif

	m_backend->user = this;
	m_backend->filter = false;
	m_backend->lockAspectRatio = false;
	m_backend->interframeBlending = false;
	m_gl->doneCurrent();

	emit created();
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
	m_paintDev.reset();
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

	mRectangle dims = {0, 0, size.width(), size.height()};
	m_backend->setLayerDimensions(m_backend, VIDEO_LAYER_IMAGE, &dims);
	recenterLayers();
}

void PainterGL::setMessagePainter(MessagePainter* messagePainter) {
	m_messagePainter = messagePainter;
}

void PainterGL::recenterLayers() {
	if (!m_context) {
		return;
	}
	const static std::initializer_list<VideoLayer> centeredLayers{VIDEO_LAYER_BACKGROUND};
	int width, height;
	mRectangle frame = {0};
	m_backend->imageSize(m_backend, VIDEO_LAYER_IMAGE, &width, &height);
	frame.width = width;
	frame.height = height;
	unsigned scale = std::max(1U, m_context->videoScale());

	for (VideoLayer l : centeredLayers) {
		mRectangle dims{};
		m_backend->imageSize(m_backend, l, &width, &height);
		dims.width = width * scale;
		dims.height = height * scale;
		mRectangleCenter(&frame, &dims);
		m_backend->setLayerDimensions(m_backend, l, &dims);
	}
}

void PainterGL::resize(const QSize& size) {
	qreal r = m_window->devicePixelRatio();
	m_size = size;
	if (m_paintDev) {
		m_paintDev->setSize(m_size * r);
		m_paintDev->setDevicePixelRatio(r);
	}
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

void PainterGL::showFrameCounter(bool enable) {
	m_showFrameCounter = enable;
}

void PainterGL::filter(bool filter) {
	m_backend->filter = filter;
	if (m_started && !m_active) {
		forceDraw();
	}
}

void PainterGL::swapInterval(int interval) {
	if (!m_started) {
		return;
	}
	m_swapInterval = interval;
#ifdef Q_OS_WIN
	wglSwapIntervalEXT(interval);
#elif defined(Q_OS_MAC)
	CGLSetParameter(CGLGetCurrentContext(), kCGLCPSwapInterval, &interval);
#else
#ifdef USE_GLX
	if (QGuiApplication::platformName() == "xcb") {
		::Display* display = glXGetCurrentDisplay();
		GLXDrawable drawable = glXGetCurrentDrawable();
		glXSwapIntervalEXT(display, drawable, interval);
	}
#endif
#ifdef USE_EGL
	if (QGuiApplication::platformName().contains("egl") || QGuiApplication::platformName() == "wayland") {
		EGLDisplay display = eglGetCurrentDisplay();
		eglSwapInterval(display, interval);
	}
#endif
#endif
}

#ifndef GL_DEBUG_OUTPUT_SYNCHRONOUS
#define GL_DEBUG_OUTPUT_SYNCHRONOUS 0x8242
#endif

void PainterGL::start() {
	makeCurrent();
#if defined(BUILD_GLES3) && !defined(Q_OS_MAC)
	if (glContextHasBug(OpenGLBug::GLTHREAD_BLOCKS_SWAP)) {
		// Suggested on Discord as a way to strongly hint that glthread should be disabled
		// See https://gitlab.freedesktop.org/mesa/mesa/-/issues/8035
		QOpenGLFunctions* fn = m_gl->functions();
		fn->glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	}
#endif

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

	if (m_interrupter.held()) {
		// A resize event is pending; that needs to happen first
		if (!m_drawTimer.isActive()) {
			m_drawTimer.start(0);
		}
		return;
	}

	mCoreSync* sync = &m_context->thread()->impl->sync;
	if (!mCoreSyncWaitFrameStart(sync)) {
		mCoreSyncWaitFrameEnd(sync);
		if (!sync->audioWait && !sync->videoFrameWait) {
			return;
		}
		if (m_delayTimer.elapsed() >= 1000 / m_window->screen()->refreshRate()) {
			return;
		}
		if (!m_drawTimer.isActive()) {
			m_drawTimer.start(1);
		}
		return;
	}
	if (m_swapInterval != !!sync->videoFrameWait) {
		swapInterval(!!sync->videoFrameWait);
	}
	dequeue();
	bool forceRedraw = true;
	if (!m_delayTimer.isValid()) {
		m_delayTimer.start();
	} else {
		if (sync->audioWait || sync->videoFrameWait) {
			while (m_delayTimer.nsecsElapsed() + OVERHEAD_NSEC < 1000000000 / sync->fpsTarget) {
				QThread::usleep(500);
			}
			forceRedraw = sync->videoFrameWait;
		}
		if (!forceRedraw) {
			forceRedraw = m_delayTimer.nsecsElapsed() + OVERHEAD_NSEC >= 1000000000 / m_window->screen()->refreshRate();
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
		if (m_delayTimer.elapsed() < 1000 / m_window->screen()->refreshRate()) {
			return;
		}
		m_delayTimer.restart();
	}
	m_backend->swap(m_backend);
}

void PainterGL::stop() {
	m_started = false;
	QMetaObject::invokeMethod(this, "doStop", Qt::BlockingQueuedConnection);
}

void PainterGL::doStop() {
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
	m_backend->clear(m_backend);
	m_backend->swap(m_backend);
	if (m_videoProxy) {
		m_videoProxy->reset();
		m_videoProxy->moveToThread(m_window->thread());
		m_videoProxy.reset();
	}
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
	float r = m_window->devicePixelRatio();
	m_backend->contextResized(m_backend, m_size.width() * r, m_size.height() * r);
	if (m_buffer) {
		m_backend->setImage(m_backend, VIDEO_LAYER_IMAGE, m_buffer);
	}
	m_backend->drawFrame(m_backend);
	if (m_showOSD && m_messagePainter && m_paintDev && !glContextHasBug(OpenGLBug::IG4ICD_CRASH)) {
		m_painter.begin(m_paintDev.get());
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
	if (!m_queue.isEmpty()) {
		uint32_t* buffer = m_queue.dequeue();
		if (m_buffer) {
			m_free.append(m_buffer);
		}
		m_buffer = buffer;
	}
}

void PainterGL::dequeueAll(bool keep) {
	QMutexLocker locker(&m_mutex);
	uint32_t* buffer = nullptr;
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
	if (!m_started) {
		makeCurrent();
	}

	if (m_shader.passes) {
		mGLES2ShaderDetach(reinterpret_cast<mGLES2Context*>(m_backend));
		mGLES2ShaderFree(&m_shader);
	}
	if (mGLES2ShaderLoad(&m_shader, dir)) {
		mGLES2ShaderAttach(reinterpret_cast<mGLES2Context*>(m_backend), static_cast<mGLES2Shader*>(m_shader.passes), m_shader.nPasses);
	}

	if (!m_started) {
		m_gl->doneCurrent();
	}
#endif
}

void PainterGL::clearShaders() {
	if (!supportsShaders()) {
		return;
	}
#if defined(BUILD_GLES2) || defined(BUILD_GLES3)
	if (!m_started) {
		makeCurrent();
	}

	if (m_shader.passes) {
		mGLES2ShaderDetach(reinterpret_cast<mGLES2Context*>(m_backend));
		mGLES2ShaderFree(&m_shader);
	}

	if (!m_started) {
		m_gl->doneCurrent();
	}
#endif
}

VideoShader* PainterGL::shaders() {
	return &m_shader;
}

QSize PainterGL::contentSize() const {
	unsigned width, height;
	VideoBackendGetFrameSize(m_backend, &width, &height);
	return {static_cast<int>(width > static_cast<unsigned>(INT_MAX) ? INT_MAX : width),
	        static_cast<int>(height > static_cast<unsigned>(INT_MAX) ? INT_MAX : height)};
}

int PainterGL::glTex() {
#if defined(BUILD_GLES2) || defined(BUILD_GLES3)
	if (supportsShaders()) {
		mGLES2Context* gl2Backend = reinterpret_cast<mGLES2Context*>(m_backend);
		return gl2Backend->tex[VIDEO_LAYER_IMAGE];
	}
#endif
#ifdef BUILD_GL
	mGLContext* glBackend = reinterpret_cast<mGLContext*>(m_backend);
	return glBackend->tex[0];
#else
	return -1;
#endif
}

QOpenGLContext* PainterGL::shareContext() {
	if (m_widget) {
		return m_widget->context();
	} else {
		return m_gl.get();
	}
}

void PainterGL::setBackgroundImage(const QImage& image) {
	if (!m_started) {
		makeCurrent();
	}

	m_backend->setImageSize(m_backend, VIDEO_LAYER_BACKGROUND, image.width(), image.height());
	recenterLayers();

	if (!image.isNull()) {
		m_background = image.convertToFormat(QImage::Format_RGB32);
		m_background = m_background.rgbSwapped();
		m_backend->setImage(m_backend, VIDEO_LAYER_BACKGROUND, m_background.constBits());
	} else {
		m_background = QImage();
	}

	if (!m_started) {
		m_gl->doneCurrent();
	}
}

#endif
