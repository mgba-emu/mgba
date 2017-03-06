/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/log.h>

#include <mgba/core/config.h>
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
	return _category - 1;
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
		ivalue = INT_MIN; // Zero is reserved
	}
	if (!end) {
		return;
	}
	mLogFilterSet(filter, key, ivalue);
}

void mLogFilterLoad(struct mLogFilter* filter, const struct mCoreConfig* config) {
	mCoreConfigEnumerate(config, "logLevel.", _setFilterLevel, filter);
	filter->defaultLevels = mLOG_ALL;
	mCoreConfigGetIntValue(config, "logLevel", &filter->defaultLevels);
}

void mLogFilterSet(struct mLogFilter* filter, const char* category, int levels) {
	HashTableInsert(&filter->categories, category, (void*)(intptr_t) levels);
	// Can't do this eagerly because not all categories are initialized immediately
	int cat = mLogCategoryById(category);
	if (cat >= 0) {
		TableInsert(&filter->levels, cat, (void*)(intptr_t) levels);
	}

}
bool mLogFilterTest(struct mLogFilter* filter, int category, enum mLogLevel level) {
	int value = (int) TableLookup(&filter->levels, category);
	if (value) {
		return value & level;
	}
	const char* cat = mLogCategoryId(category);
	if (cat) {
		value = (int) HashTableLookup(&filter->categories, cat);
		if (value) {
			TableInsert(&filter->levels, category, (void*)(intptr_t) value);
			return value & level;
		}
	}
	return level & filter->defaultLevels;
}

mLOG_DEFINE_CATEGORY(STATUS, "Status", "core.status")
