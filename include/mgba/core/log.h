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

struct mStandardLogger {
	struct mLogger d;
	bool logToStdout;
	struct VFile* logFile;
};

struct mLogger* mLogGetContext(void);
void mLogSetDefaultLogger(struct mLogger*);
int mLogGenerateCategory(const char*, const char*);
const char* mLogCategoryName(int);
const char* mLogCategoryId(int);
int mLogCategoryById(const char*);

struct mCoreConfig;
void mStandardLoggerInit(struct mStandardLogger*);
void mStandardLoggerDeinit(struct mStandardLogger*);
void mStandardLoggerConfig(struct mStandardLogger*, struct mCoreConfig* config);

void mLogFilterInit(struct mLogFilter*);
void mLogFilterDeinit(struct mLogFilter*);
void mLogFilterLoad(struct mLogFilter*, const struct mCoreConfig*);
void mLogFilterSave(const struct mLogFilter*, struct mCoreConfig*);
void mLogFilterSet(struct mLogFilter*, const char* category, int levels);
void mLogFilterReset(struct mLogFilter*, const char* category);
bool mLogFilterTest(const struct mLogFilter*, int category, enum mLogLevel level);
int mLogFilterLevels(const struct mLogFilter*, int category);

ATTRIBUTE_FORMAT(printf, 3, 4)
void mLog(int category, enum mLogLevel level, const char* format, ...);

ATTRIBUTE_FORMAT(printf, 4, 5)
void mLogExplicit(struct mLogger*, int category, enum mLogLevel level, const char* format, ...);

#define mLOG(CATEGORY, LEVEL, ...) mLog(_mLOG_CAT_ ## CATEGORY, mLOG_ ## LEVEL, __VA_ARGS__)

#define mLOG_DECLARE_CATEGORY(CATEGORY) extern int _mLOG_CAT_ ## CATEGORY;
#define mLOG_DEFINE_CATEGORY(CATEGORY, NAME, ID) \
	int _mLOG_CAT_ ## CATEGORY; \
	CONSTRUCTOR(_mLOG_CAT_ ## CATEGORY ## _INIT) { \
		_mLOG_CAT_ ## CATEGORY = mLogGenerateCategory(NAME, ID); \
	}

MGBA_EXPORT mLOG_DECLARE_CATEGORY(STATUS)

CXX_GUARD_END

#endif
