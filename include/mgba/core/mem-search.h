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
	mCORE_MEMORY_SEARCH_GUESS,
};

struct mCoreMemorySearchParams {
	int memoryFlags;
	enum mCoreMemorySearchType type;
	int align;
	int width;
	union {
		const char* valueStr;
		uint32_t valueInt;
	};
};

struct mCoreMemorySearchResult {
	uint32_t address;
	int segment;
	uint64_t guessDivisor;
	uint64_t guessMultiplier;
	enum mCoreMemorySearchType type;
	int width;
};

DECLARE_VECTOR(mCoreMemorySearchResults, struct mCoreMemorySearchResult);

struct mCore;
void mCoreMemorySearch(struct mCore* core, const struct mCoreMemorySearchParams* params, struct mCoreMemorySearchResults* out, size_t limit);
void mCoreMemorySearchRepeat(struct mCore* core, const struct mCoreMemorySearchParams* params, struct mCoreMemorySearchResults* inout);

CXX_GUARD_END

#endif
