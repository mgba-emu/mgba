/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_LOG_H
#define M_LOG_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba-util/table.h>

enum mLogLevel {
	mLOG_FATAL = 0x01,
	mLOG_ERROR = 0x02,
	mLOG_WARN = 0x04,
	mLOG_INFO = 0x08,
	mLOG_DEBUG = 0x10,
	mLOG_STUB = 0x20,
	mLOG_GAME_ERROR = 0x40,

	mLOG_ALL = 0x7F
};

struct Table;
struct mLogFilter {
	int defaultLevels;
	struct Table categories;
	struct Table levels;
};

struct mLogger {
	void (*log)(struct mLogger*, int category, enum mLogLevel level, const char* format, va_list args);
	struct mLogFilter* filter;
};

struct mLogger* mLogGetContext(void);
void mLogSetDefaultLogger(struct mLogger*);
int mLogGenerateCategory(const char*, const char*);
const char* mLogCategoryName(int);
const char* mLogCategoryId(int);
int mLogCategoryById(const char*);

struct mCoreConfig;
void mLogFilterInit(struct mLogFilter*);
void mLogFilterDeinit(struct mLogFilter*);
void mLogFilterLoad(struct mLogFilter*, const struct mCoreConfig*);
void mLogFilterSet(struct mLogFilter*, const char* category, int levels);
bool mLogFilterTest(struct mLogFilter*, int category, enum mLogLevel level);

ATTRIBUTE_FORMAT(printf, 3, 4)
void mLog(int category, enum mLogLevel level, const char* format, ...);

#define mLOG(CATEGORY, LEVEL, ...) mLog(_mLOG_CAT_ ## CATEGORY (), mLOG_ ## LEVEL, __VA_ARGS__)

#define mLOG_DECLARE_CATEGORY(CATEGORY) int _mLOG_CAT_ ## CATEGORY (void); extern const char* _mLOG_CAT_ ## CATEGORY ## _ID;
#define mLOG_DEFINE_CATEGORY(CATEGORY, NAME, ID) \
	int _mLOG_CAT_ ## CATEGORY (void) { \
		static int category = 0; \
		if (!category) { \
			category = mLogGenerateCategory(NAME, ID); \
		} \
		return category; \
	} \
	const char* _mLOG_CAT_ ## CATEGORY ## _ID = ID;

mLOG_DECLARE_CATEGORY(STATUS)

CXX_GUARD_END

#endif
