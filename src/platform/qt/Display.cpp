/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "Display.h"

#include "DisplayGL.h"
#include "DisplayQt.h"

extern "C" {
#include "gba/video.h"
}

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
		return new DisplayGL(format, false, parent);
#endif
#ifdef BUILD_GL
	case Driver::OPENGL1:
		return new DisplayGL(format, true, parent);
#endif

	case Driver::QT:
		return new DisplayQt(parent);

	default:
#if defined(BUILD_GL) || defined(BUILD_GLES2) || defined(USE_EPOXY)
		return new DisplayGL(format, false, parent);
#else
		return new DisplayQt(parent);
#endif
	}
}

Display::Display(QWidget* parent)
	: QWidget(parent)
	, m_lockAspectRatio(false)
	, m_filter(false)
{
	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	setMinimumSize(VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
	connect(&m_mouseTimer, SIGNAL(timeout()), this, SIGNAL(hideCursor()));
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
