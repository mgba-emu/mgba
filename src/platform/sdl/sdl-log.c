/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "sdl-log.h"
#include <mgba-util/vfs.h>
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/core/thread.h>
#include <mgba/core/config.h>

static bool _logToStdout;
static struct VFile* _logFile;

struct mLogger _getLogger(bool logToStdout, bool logToFile, const char* logFile, int filterLevels);
static void _mCoreLog(struct mLogger* logger, int category, enum mLogLevel level, const char* format, va_list args);

bool getBoolConfig(struct mCore* core, const char* key, bool defaultValue) {
	int value = 0;
	bool searched = mCoreConfigGetValue(&core->config, key);
	if(searched) {
		mCoreConfigGetIntValue(&core->config, key, &value);
		return value != 0 ? true: false;
	}
	return defaultValue;
}

struct mLogger getLogger(struct mCore* core) {
	bool logToStdout = getBoolConfig(core, "logToStdout", true);
	bool logToFile = getBoolConfig(core, "logToFile", false);
	const char* logFile = mCoreConfigGetValue(&core->config, "logFile");
	int filterLevels = core->opts.logLevel;
	return _getLogger(logToStdout, logToFile, logFile, filterLevels);
}

struct mLogger _getLogger(bool logToStdout, bool logToFile, const char* logFile, int filterLevels) {
	// Assign basic static variables
	_logToStdout = logToStdout;
	_logFile = NULL;
	
	if(logToFile && logFile)
		_logFile = VFileOpen(logFile, O_WRONLY | O_CREAT | O_TRUNC);

	// Create the filter
	struct mLogFilter* filter = (struct mLogFilter*)malloc(sizeof(struct mLogFilter));
	mLogFilterInit(filter);
	mLogFilterSet(filter, "gba.bios", mLOG_STUB | mLOG_FATAL);
	mLogFilterSet(filter, "core.status", mLOG_ALL & ~mLOG_DEBUG);
	filter->defaultLevels = filterLevels;

	// Create the logger
	struct mLogger logger;
	logger.log = _mCoreLog;
	logger.filter = filter;

	return logger;
}

static void _mCoreLog(struct mLogger* logger, int category, enum mLogLevel level, const char* format, va_list args) {

	struct mCoreThread* thread = mCoreThreadGet();
	if (thread && level == mLOG_FATAL) {
		mCoreThreadMarkCrashed(thread);
	}

	if (!mLogFilterTest(logger->filter, category, level)) {
		return;
	}

	const int MAX_BUF = 1024;
	char* buffer = malloc(MAX_BUF);

	int length = 0;
	length += snprintf(buffer+length, MAX_BUF-length, "%s: ", mLogCategoryName(category));
	length += vsnprintf(buffer+length, MAX_BUF-length, format, args);
	length += snprintf(buffer+length, MAX_BUF-length, "\n");

	if(_logToStdout)
		printf("%s", buffer);

	if (_logFile)
		_logFile->write(_logFile, buffer, length);

	free(buffer);
}

