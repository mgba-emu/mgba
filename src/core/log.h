/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_LOG_H
#define M_LOG_H

#include "util/common.h"

enum mLogLevel {
	mLOG_FATAL = 0x01,
	mLOG_ERROR = 0x02,
	mLOG_WARN = 0x04,
	mLOG_INFO = 0x08,
	mLOG_DEBUG = 0x10,
	mLOG_STUB = 0x20,
	mLOG_GAME_ERROR = 0x40
};

struct mLogger {
	ATTRIBUTE_FORMAT(printf, 4, 5)
	void (*log)(struct mLogger*, int category, enum mLogLevel level, const char* format, ...);
};

struct mLogger* mLogGetContext(void);
int mLogGenerateCategory(void);

ATTRIBUTE_FORMAT(printf, 3, 4)
static inline void _mLog(int (*category)(void), enum mLogLevel level, const char* format, ...) {
	struct mLogger* context = mLogGetContext();
	va_list args;
	va_start(args, format);
	if (context) {
		context->log(context, category(), level, format, args);
	} else {
		vprintf(format, args);
		printf("\n");
	}
	va_end(args);
}

#define mLOG(CATEGORY, LEVEL, ...) _mLog(_mLOG_CAT_ ## CATEGORY, mLOG_ ## LEVEL, __VA_ARGS__)

#define mLOG_DECLARE_CATEGORY(CATEGORY) int _mLOG_CAT_ ## CATEGORY (void);
#define mLOG_DEFINE_CATEGORY(CATEGORY) \
	int _mLOG_CAT_ ## CATEGORY (void) { \
		static int category = 0; \
		if (!category) { \
			category = mLogGenerateCategory(); \
		} \
		return category; \
	}

#endif
