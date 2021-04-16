/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "Display.h"

#include "DisplayGL.h"
#include "DisplayQt.h"
#include "LogController.h"

using namespace QGBA;

#if defined(BUILD_GL) || defined(BUILD_GLES2) || defined(USE_EPOXY)
Display::Driver Display::s_driver = Display::Driver::OPENGL;
#else
Display::Driver Display::s_driver = Display::Driver::QT;
#endif

Display* Display::create(QWidget* parent) {
#if defined(BUILD_GL) || defined(BUILD_GLES2) || defined(USE_EPOXY)
	QSurfaceFormat format;
	format.setSwapInterval(1);
	format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
#endif

	switch (s_driver) {
#if defined(BUILD_GL) || defined(BUILD_GLES2) || defined(USE_EPOXY)
	case Driver::OPENGL:
#if defined(BUILD_GLES2) || defined(USE_EPOXY)
		if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES) {
			format.setVersion(2, 0);
		} else {
			format.setVersion(3, 2);
		}
		format.setProfile(QSurfaceFormat::CoreProfile);
		if (!DisplayGL::supportsFormat(format)) {
#ifdef BUILD_GL
			LOG(QT, WARN) << ("Failed to create an OpenGL Core context, trying old-style...");
			format.setVersion(1, 4);
			format.setOption(QSurfaceFormat::DeprecatedFunctions);
			if (!DisplayGL::supportsFormat(format)) {
				return nullptr;
			}
#else
			return nullptr;
#endif
		}
		return new DisplayGL(format, parent);
#endif
#endif
#ifdef BUILD_GL
	case Driver::OPENGL1:
		format.setVersion(1, 4);
		if (!DisplayGL::supportsFormat(format)) {
			return nullptr;
		}
		return new DisplayGL(format, parent);
#endif

	case Driver::QT:
		return new DisplayQt(parent);

	default:
#if defined(BUILD_GL) || defined(BUILD_GLES2) || defined(USE_EPOXY)
		return new DisplayGL(format, parent);
#else
		return new DisplayQt(parent);
#endif
	}
}

Display::Display(QWidget* parent)
	: QWidget(parent)
{
	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	connect(&m_mouseTimer, &QTimer::timeout, this, &Display::hideCursor);
	m_mouseTimer.setSingleShot(true);
	m_mouseTimer.setInterval(MOUSE_DISAPPEAR_TIMER);
	setMouseTracking(true);
}

void Display::attach(std::shared_ptr<CoreController> controller) {
	connect(controller.get(), &CoreController::stateLoaded, this, &Display::resizeContext);
	connect(controller.get(), &CoreController::stateLoaded, this, &Display::forceDraw);
	connect(controller.get(), &CoreController::rewound, this, &Display::forceDraw);
	connect(controller.get(), &CoreController::paused, this, &Display::pauseDrawing);
	connect(controller.get(), &CoreController::unpaused, this, &Display::unpauseDrawing);
	connect(controller.get(), &CoreController::frameAvailable, this, &Display::framePosted);
	connect(controller.get(), &CoreController::statusPosted, this, &Display::showMessage);
	connect(controller.get(), &CoreController::didReset, this, &Display::resizeContext);
}

void Display::resizeEvent(QResizeEvent*) {
	m_messagePainter.resize(size(), m_lockAspectRatio, devicePixelRatio());
}

void Display::lockAspectRatio(bool lock) {
	m_lockAspectRatio = lock;
	m_messagePainter.resize(size(), m_lockAspectRatio, devicePixelRatio());
}

void Display::lockIntegerScaling(bool lock) {
	m_lockIntegerScaling = lock;
}

void Display::interframeBlending(bool lock) {
	m_interframeBlending = lock;
}

void Display::showOSDMessages(bool enable) {
	m_showOSD = enable;
}

void Display::filter(bool filter) {
	m_filter = filter;
}

void Display::showMessage(const QString& message) {
	m_messagePainter.showMessage(message);
	if (!isDrawing()) {
		forceDraw();
	}
}

void Display::mouseMoveEvent(QMouseEvent*) {
	emit showCursor();
	m_mouseTimer.stop();
	m_mouseTimer.start();
}
