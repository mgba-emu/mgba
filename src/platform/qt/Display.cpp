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

QSize Display::viewportSize() {
	QSize s = size();
	QSize ds = s;
	if (isAspectRatioLocked()) {
		if (s.width() * m_coreHeight > s.height() * m_coreWidth) {
			ds.setWidth(s.height() * m_coreWidth / m_coreHeight);
		} else if (s.width() * m_coreHeight < s.height() * m_coreWidth) {
			ds.setHeight(s.width() * m_coreHeight / m_coreWidth);
		}
	}
	if (isIntegerScalingLocked()) {
		ds.setWidth(ds.width() - ds.width() % m_coreWidth);
		ds.setHeight(ds.height() - ds.height() % m_coreHeight);
	}
	return ds;
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

void Display::mouseMoveEvent(QMouseEvent* event) {
	emit showCursor();
	m_mouseTimer.stop();
	m_mouseTimer.start();
	event->ignore();
}

void Display::setSystemDimensions(int width, int height) {
	m_coreWidth = width;
	m_coreHeight = height;
}
