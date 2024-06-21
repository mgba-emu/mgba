/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/log.h>

#include <mgba/core/config.h>
#include <mgba/core/thread.h>
#include <mgba-util/vfs.h>

#define MAX_CATEGORY 64
#define MAX_LOG_BUF 1024

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
	return _category - 1;
}

const char* mLogCategoryName(int category) {
	if (category < MAX_CATEGORY) {
		return _categoryNames[category];
	}
	return NULL;
}

const char* mLogCategoryId(int category) {
	if (category >= 0 && category < MAX_CATEGORY) {
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
		if (!context->filter || mLogFilterTest(context->filter, category, level)) {
			context->log(context, category, level, format, args);
		}
	} else {
		printf("%s: ", mLogCategoryName(category));
		vprintf(format, args);
		printf("\n");
	}
	va_end(args);
}

void mLogExplicit(struct mLogger* context, int category, enum mLogLevel level, const char* format, ...) {
	va_list args;
	va_start(args, format);
	if (!context->filter || mLogFilterTest(context->filter, category, level)) {
		context->log(context, category, level, format, args);
	}
	va_end(args);
}

void mLogFilterInit(struct mLogFilter* filter) {
	HashTableInit(&filter->categories, 8, NULL);
	TableInit(&filter->levels, 8, NULL);
}

void mLogFilterDeinit(struct mLogFilter* filter) {
	HashTableDeinit(&filter->categories);
	TableDeinit(&filter->levels);
}

static void _setFilterLevel(const char* key, const char* value, enum mCoreConfigLevel level, void* user) {
	UNUSED(level);
	struct mLogFilter* filter = user;
	key = strchr(key, '.');
	if (!key || !key[1]) {
		return;
	}
	if (!value) {
		return;
	}
	++key;
	char* end;
	int ivalue = strtol(value, &end, 10);
	if (ivalue == 0) {
		ivalue = 0x80; // Zero is reserved
	}
	if (!end) {
		return;
	}
	mLogFilterSet(filter, key, ivalue);
}

void mLogFilterLoad(struct mLogFilter* filter, const struct mCoreConfig* config) {
	HashTableClear(&filter->categories);
	TableClear(&filter->levels);

	mCoreConfigEnumerate(config, "logLevel.", _setFilterLevel, filter);
	filter->defaultLevels = mLOG_ALL;
	mCoreConfigGetIntValue(config, "logLevel", &filter->defaultLevels);
}

void mLogFilterSave(const struct mLogFilter* filter, struct mCoreConfig* config) {
	mCoreConfigSetIntValue(config, "logLevel", filter->defaultLevels);
	int i;
	for (i = 0; i < _category; ++i) {
		char configName[128] = {0};
		snprintf(configName, sizeof(configName) - 1, "logLevel.%s", mLogCategoryId(i));
		int levels = mLogFilterLevels(filter, i);
		if (levels) {
			mCoreConfigSetIntValue(config, configName, levels & ~0x80);
		} else {
			mCoreConfigSetValue(config, configName, NULL);
		}
	}
}

void mLogFilterSet(struct mLogFilter* filter, const char* category, int levels) {
	levels |= 0x80;
	HashTableInsert(&filter->categories, category, (void*)(intptr_t) levels);
	// Can't do this eagerly because not all categories are initialized immediately
	int cat = mLogCategoryById(category);
	if (cat >= 0) {
		TableInsert(&filter->levels, cat, (void*)(intptr_t) levels);
	}
}

void mLogFilterReset(struct mLogFilter* filter, const char* category) {
	HashTableRemove(&filter->categories, category);
	// Can't do this eagerly because not all categories are initialized immediately
	int cat = mLogCategoryById(category);
	if (cat >= 0) {
		TableRemove(&filter->levels, cat);
	}
}

bool mLogFilterTest(const struct mLogFilter* filter, int category, enum mLogLevel level) {
	int value = mLogFilterLevels(filter, category);
	if (value) {
		return value & level;
	}
	return level & filter->defaultLevels;
}

int mLogFilterLevels(const struct mLogFilter* filter , int category) {
	int value = (intptr_t) TableLookup(&filter->levels, category);
	if (value) {
		return value;
	}
	const char* cat = mLogCategoryId(category);
	if (cat) {
		value = (intptr_t) HashTableLookup(&filter->categories, cat);
	}
	return value;
}

void _mCoreStandardLog(struct mLogger* logger, int category, enum mLogLevel level, const char* format, va_list args) {
	struct mStandardLogger* stdlog = (struct mStandardLogger*) logger;

	if (!mLogFilterTest(logger->filter, category, level)) {
		return;
	}

	char buffer[MAX_LOG_BUF];

	// Prepare the string
	size_t length = snprintf(buffer, sizeof(buffer), "%s: ", mLogCategoryName(category));
	if (length < sizeof(buffer)) {
		length += vsnprintf(buffer + length, sizeof(buffer) - length, format, args);
	}
	if (length < sizeof(buffer)) {
		length += snprintf(buffer + length, sizeof(buffer) - length, "\n");
	}

	// Make sure the length doesn't exceed the size of the buffer when actually writing
	if (length > sizeof(buffer)) {
		length = sizeof(buffer);
	}

	if (stdlog->logToStdout) {
		printf("%s", buffer);
	}

	if (stdlog->logFile) {
		stdlog->logFile->write(stdlog->logFile, buffer, length);
	}
}

void mStandardLoggerInit(struct mStandardLogger* logger) {
	logger->d.log = _mCoreStandardLog;
	logger->d.filter = malloc(sizeof(struct mLogFilter));
	mLogFilterInit(logger->d.filter);
}

void mStandardLoggerDeinit(struct mStandardLogger* logger) {
	if (logger->d.filter) {
		mLogFilterDeinit(logger->d.filter);
		free(logger->d.filter);
		logger->d.filter = NULL;
	}
}

void mStandardLoggerConfig(struct mStandardLogger* logger, struct mCoreConfig* config) {
	bool logToFile = false;
	const char* logFile = mCoreConfigGetValue(config, "logFile");
	mCoreConfigGetBoolValue(config, "logToStdout", &logger->logToStdout);
	mCoreConfigGetBoolValue(config, "logToFile", &logToFile);

	if (logToFile && logFile) {
		logger->logFile = VFileOpen(logFile, O_WRONLY | O_CREAT | O_APPEND);
	}

	mLogFilterLoad(logger->d.filter, config);
}

mLOG_DEFINE_CATEGORY(STATUS, "Status", "core.status")
