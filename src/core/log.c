/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/log.h>

#include <mgba/core/thread.h>

#define MAX_CATEGORY 64

static struct mLogger* _defaultLogger = NULL;

struct mLogger* mLogGetContext(void) {
	struct mLogger* logger = NULL;
#ifndef DISABLE_THREADING
	logger = mCoreThreadLogger();
#endif
	if (logger) {
		return logger;
	}
	return _defaultLogger;
}

void mLogSetDefaultLogger(struct mLogger* logger) {
	_defaultLogger = logger;
}

static int _category = 0;
static const char* _categoryNames[MAX_CATEGORY];
static const char* _categoryIds[MAX_CATEGORY];

int mLogGenerateCategory(const char* name, const char* id) {
	if (_category < MAX_CATEGORY) {
		_categoryNames[_category] = name;
		_categoryIds[_category] = id;
	}
	++_category;
	return _category;
}

const char* mLogCategoryName(int category) {
	if (category < MAX_CATEGORY) {
		return _categoryNames[category];
	}
	return NULL;
}

const char* mLogCategoryId(int category) {
	if (category < MAX_CATEGORY) {
		return _categoryIds[category];
	}
	return NULL;
}

int mLogCategoryById(const char* id) {
	int i;
	for (i = 0; i < _category; ++i) {
		if (strcmp(_categoryIds[i], id) == 0) {
			return i;
		}
	}
	return -1;
}

void mLog(int category, enum mLogLevel level, const char* format, ...) {
	struct mLogger* context = mLogGetContext();
	va_list args;
	va_start(args, format);
	if (context) {
		context->log(context, category, level, format, args);
	} else {
		printf("%s: ", mLogCategoryName(category));
		vprintf(format, args);
		printf("\n");
	}
	va_end(args);
}

mLOG_DEFINE_CATEGORY(STATUS, "Status", "core.status")
