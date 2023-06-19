/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef CORE_MEM_SEARCH_H
#define CORE_MEM_SEARCH_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba-util/vector.h>

enum mCoreMemorySearchType {
	mCORE_MEMORY_SEARCH_INT,
	mCORE_MEMORY_SEARCH_STRING,
};

enum mCoreMemorySearchOp {
	mCORE_MEMORY_SEARCH_EQUAL,
	mCORE_MEMORY_SEARCH_NOT_EQUAL,
	mCORE_MEMORY_SEARCH_GREATER,
	mCORE_MEMORY_SEARCH_NOT_GREATER,
	mCORE_MEMORY_SEARCH_LESS,
	mCORE_MEMORY_SEARCH_NOT_LESS,
	mCORE_MEMORY_SEARCH_ANY,
	mCORE_MEMORY_SEARCH_CHANGED,
	mCORE_MEMORY_SEARCH_NOT_CHANGED,
	mCORE_MEMORY_SEARCH_CHANGED_BY,
	mCORE_MEMORY_SEARCH_INCREASE,
	mCORE_MEMORY_SEARCH_NOT_INCREASE,
	mCORE_MEMORY_SEARCH_INCREASE_BY,
	mCORE_MEMORY_SEARCH_DECREASE,
	mCORE_MEMORY_SEARCH_NOT_DECREASE,
	mCORE_MEMORY_SEARCH_DECREASE_BY,
};


struct mCoreMemorySearchParams {
	int memoryFlags;
	enum mCoreMemorySearchType type;
	enum mCoreMemorySearchOp op;
	int align;
	int width;
	union {
		const char* valueStr;
		int64_t valueInt;
	};
	uint32_t start;
	uint32_t end;
	bool signedNum;
};

struct mCoreMemorySearchResult {
	uint32_t address;
	int segment;
	enum mCoreMemorySearchType type;
	int width;
	int64_t curValue;
	int64_t oldValue;
	bool signedNum;
};

DECLARE_VECTOR(mCoreMemorySearchResults, struct mCoreMemorySearchResult);

struct mCore;
void mCoreMemorySearch(struct mCore* core, const struct mCoreMemorySearchParams* params, struct mCoreMemorySearchResults* out, size_t limit);
void mCoreMemorySearchRepeat(struct mCore* core, const struct mCoreMemorySearchParams* params, struct mCoreMemorySearchResults* inout);

CXX_GUARD_END

#endif
