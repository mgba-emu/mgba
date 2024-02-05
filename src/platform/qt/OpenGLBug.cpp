/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "OpenGLBug.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions>

namespace QGBA {

bool glContextHasBug(OpenGLBug bug) {
	QOpenGLContext* context = QOpenGLContext::currentContext();
	if (!context) {
		abort();
	}
	QOpenGLFunctions* fn = context->functions();
	QString vendor(reinterpret_cast<const char*>(fn->glGetString(GL_VENDOR)));
	QString renderer(reinterpret_cast<const char*>(fn->glGetString(GL_RENDERER)));
	QString version(reinterpret_cast<const char*>(fn->glGetString(GL_VERSION)));

	switch (bug) {
	case OpenGLBug::CROSS_THREAD_FLUSH:
#ifndef Q_OS_WIN
		return false;
#else
		return vendor == "Intel";
#endif

	case OpenGLBug::GLTHREAD_BLOCKS_SWAP:
		return version.contains(" Mesa ");

	case OpenGLBug::IG4ICD_CRASH:
#ifdef Q_OS_WIN
		if (vendor != "Intel") {
			return false;
		}
		if (renderer == "Intel Pineview Platform") {
			return true;
		}
#endif
		return false;

	default:
		return false;
	}
}

}
