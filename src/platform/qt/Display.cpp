/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "Display.h"

#include "DisplayGL.h"
#include "DisplayQt.h"

using namespace QGBA;

#if defined(BUILD_GL) || defined(BUILD_GLES2) || defined(USE_EPOXY)
Display::Driver Display::s_driver = Display::Driver::OPENGL;
#else
Display::Driver Display::s_driver = Display::Driver::QT;
#endif

Display* Display::create(QWidget* parent) {
#if defined(BUILD_GL) || defined(BUILD_GLES2) || defined(USE_EPOXY)
	QGLFormat format(QGLFormat(QGL::Rgba | QGL::DoubleBuffer));
	format.setSwapInterval(1);
#endif

	switch (s_driver) {
#if defined(BUILD_GL) || defined(BUILD_GLES2) || defined(USE_EPOXY)
	case Driver::OPENGL:
		return new DisplayGL(format, parent);
#endif
#ifdef BUILD_GL
	case Driver::OPENGL1:
		format.setVersion(1, 4);
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
