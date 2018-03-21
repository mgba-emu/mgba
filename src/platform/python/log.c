/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "log.h"

static void _pyLogShim(struct mLogger* logger, int category, enum mLogLevel level, const char* format, va_list args) {
	struct mLoggerPy* pylogger = (struct mLoggerPy*) logger;
	char message[256] = {0};
	vsnprintf(message, sizeof(message) - 1, format, args);
	_pyLog(pylogger, category, level, message);
}

struct mLogger* mLoggerPythonCreate(void* pyobj) {
	struct mLoggerPy* logger = malloc(sizeof(*logger));
	logger->d.log = _pyLogShim;
	logger->d.filter = NULL;
	logger->pyobj = pyobj;
	return &logger->d;
}
