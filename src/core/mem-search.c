/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/mem-search.h>

#include <mgba/core/core.h>
#include <mgba/core/interface.h>

DEFINE_VECTOR(mCoreMemorySearchResults, struct mCoreMemorySearchResult);

static bool _op(int32_t value, int32_t match, enum mCoreMemorySearchOp op) {
	switch (op) {
	case mCORE_MEMORY_SEARCH_GREATER:
		return value > match;
	case mCORE_MEMORY_SEARCH_LESS:
		return value < match;
	case mCORE_MEMORY_SEARCH_EQUAL:
	case mCORE_MEMORY_SEARCH_DELTA:
		return value == match;
	case mCORE_MEMORY_SEARCH_DELTA_POSITIVE:
		return value > 0;
	case mCORE_MEMORY_SEARCH_DELTA_NEGATIVE:
		return value < 0;
	case mCORE_MEMORY_SEARCH_DELTA_ANY:
		return value != 0;
	case mCORE_MEMORY_SEARCH_ANY:
		return true;
	}
	return false;
}

static size_t _search32(const void* mem, size_t size, const struct mCoreMemoryBlock* block, uint32_t value32, enum mCoreMemorySearchOp op, struct mCoreMemorySearchResults* out, size_t limit) {
	const uint32_t* mem32 = mem;
	size_t found = 0;
	uint32_t start = block->start;
	uint32_t end = size; // TODO: Segments
	size_t i;
	// TODO: Big endian
	for (i = 0; (!limit || found < limit) && i < end; i += 4) {
		if (_op(mem32[i >> 2], value32, op)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i;
			res->type = mCORE_MEMORY_SEARCH_INT;
			res->width = 4;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			res->guessMultiplier = 1;
			res->oldValue = mem32[i >> 2];
			++found;
		}
	}
	return found;
}

static size_t _search16(const void* mem, size_t size, const struct mCoreMemoryBlock* block, uint16_t value16, enum mCoreMemorySearchOp op, struct mCoreMemorySearchResults* out, size_t limit) {
	const uint16_t* mem16 = mem;
	size_t found = 0;
	uint32_t start = block->start;
	uint32_t end = size; // TODO: Segments
	size_t i;
	// TODO: Big endian
	for (i = 0; (!limit || found < limit) && i < end; i += 2) {
		if (_op(mem16[i >> 1], value16, op)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i;
			res->type = mCORE_MEMORY_SEARCH_INT;
			res->width = 2;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			res->guessMultiplier = 1;
			res->oldValue = mem16[i >> 1];
			++found;
		}
	}
	return found;
}

static size_t _search8(const void* mem, size_t size, const struct mCoreMemoryBlock* block, uint8_t value8, enum mCoreMemorySearchOp op, struct mCoreMemorySearchResults* out, size_t limit) {
	const uint8_t* mem8 = mem;
	size_t found = 0;
	uint32_t start = block->start;
	uint32_t end = size; // TODO: Segments
	size_t i;
	for (i = 0; (!limit || found < limit) && i < end; ++i) {
		if (_op(mem8[i], value8, op)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i;
			res->type = mCORE_MEMORY_SEARCH_INT;
			res->width = 1;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			res->guessMultiplier = 1;
			res->oldValue = mem8[i];
			++found;
		}
	}
	return found;
}

static size_t _searchInt(const void* mem, size_t size, const struct mCoreMemoryBlock* block, const struct mCoreMemorySearchParams* params, struct mCoreMemorySearchResults* out, size_t limit) {
	if (params->align == params->width || params->align == -1) {
		switch (params->width) {
		case 4:
			return _search32(mem, size, block, params->valueInt, params->op, out, limit);
		case 2:
			return _search16(mem, size, block, params->valueInt, params->op, out, limit);
		case 1:
			return _search8(mem, size, block, params->valueInt, params->op, out, limit);
		}
	}
	return 0;
}

static size_t _searchStr(const void* mem, size_t size, const struct mCoreMemoryBlock* block, const char* valueStr, int len, struct mCoreMemorySearchResults* out, size_t limit) {
	const char* memStr = mem;
	size_t found = 0;
	uint32_t start = block->start;
	uint32_t end = size; // TODO: Segments
	size_t i;
	for (i = 0; (!limit || found < limit) && i < end - len; ++i) {
		if (!memcmp(valueStr, &memStr[i], len)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i;
			res->type = mCORE_MEMORY_SEARCH_STRING;
			res->width = len;
			res->segment = -1; // TODO
			++found;
		}
	}
	return found;
}

static size_t _searchGuess(const void* mem, size_t size, const struct mCoreMemoryBlock* block, const struct mCoreMemorySearchParams* params, struct mCoreMemorySearchResults* out, size_t limit) {
	// TODO: As str

	char* end;
	int64_t value;

	size_t found = 0;

	struct mCoreMemorySearchResults tmp;
	mCoreMemorySearchResultsInit(&tmp, 0);

	// Decimal:
	value = strtoll(params->valueStr, &end, 10);
	if (end && !end[0]) {
		if ((params->width == -1 && value > 0x10000) || params->width == 4) {
			found += _search32(mem, size, block, value, params->op, out, limit ? limit - found : 0);
		} else if ((params->width == -1 && value > 0x100) || params->width == 2) {
			found += _search16(mem, size, block, value, params->op, out, limit ? limit - found : 0);
		} else {
			found += _search8(mem, size, block, value, params->op, out, limit ? limit - found : 0);
		}

		uint32_t divisor = 1;
		while (value && !(value % 10)) {
			mCoreMemorySearchResultsClear(&tmp);
			value /= 10;
			divisor *= 10;

			if ((params->width == -1 && value > 0x10000) || params->width == 4) {
				found += _search32(mem, size, block, value, params->op, &tmp, limit ? limit - found : 0);
			} else if ((params->width == -1 && value > 0x100) || params->width == 2) {
				found += _search16(mem, size, block, value, params->op, &tmp, limit ? limit - found : 0);
			} else {
				found += _search8(mem, size, block, value, params->op, &tmp, limit ? limit - found : 0);
			}
			size_t i;
			for (i = 0; i < mCoreMemorySearchResultsSize(&tmp); ++i) {
				struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsGetPointer(&tmp, i);
				res->guessDivisor = divisor;
				*mCoreMemorySearchResultsAppend(out) = *res;
			}
		}
	}

	// Hex:
	value = strtoll(params->valueStr, &end, 16);
	if (end && !end[0]) {
		if ((params->width == -1 && value > 0x10000) || params->width == 4) {
			found += _search32(mem, size, block, value, params->op, out, limit ? limit - found : 0);
		} else if ((params->width == -1 && value > 0x100) || params->width == 2) {
			found += _search16(mem, size, block, value, params->op, out, limit ? limit - found : 0);
		} else {
			found += _search8(mem, size, block, value, params->op, out, limit ? limit - found : 0);
		}

		uint32_t divisor = 1;
		while (value && !(value & 0xF)) {
			mCoreMemorySearchResultsClear(&tmp);
			value >>= 4;
			divisor <<= 4;

			if ((params->width == -1 && value > 0x10000) || params->width == 4) {
				found += _search32(mem, size, block, value, params->op, &tmp, limit ? limit - found : 0);
			} else if ((params->width == -1 && value > 0x100) || params->width == 2) {
				found += _search16(mem, size, block, value, params->op, &tmp, limit ? limit - found : 0);
			} else {
				found += _search8(mem, size, block, value, params->op, &tmp, limit ? limit - found : 0);
			}
			size_t i;
			for (i = 0; i < mCoreMemorySearchResultsSize(&tmp); ++i) {
				struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsGetPointer(&tmp, i);
				res->guessDivisor = divisor;
				*mCoreMemorySearchResultsAppend(out) = *res;
			}
		}
	}

	mCoreMemorySearchResultsDeinit(&tmp);
	return found;
}

static size_t _search(const void* mem, size_t size, const struct mCoreMemoryBlock* block, const struct mCoreMemorySearchParams* params, struct mCoreMemorySearchResults* out, size_t limit) {
	switch (params->type) {
	case mCORE_MEMORY_SEARCH_INT:
		return _searchInt(mem, size, block, params, out, limit);
	case mCORE_MEMORY_SEARCH_STRING:
		return _searchStr(mem, size, block, params->valueStr, params->width, out, limit);
	case mCORE_MEMORY_SEARCH_GUESS:
		return _searchGuess(mem, size, block, params, out, limit);
	}
	return 0;
}

void mCoreMemorySearch(struct mCore* core, const struct mCoreMemorySearchParams* params, struct mCoreMemorySearchResults* out, size_t limit) {
	const struct mCoreMemoryBlock* blocks;
	size_t nBlocks = core->listMemoryBlocks(core, &blocks);
	size_t found = 0;

	size_t b;
	for (b = 0; (!limit || found < limit) && b < nBlocks; ++b) {
		size_t size;
		const struct mCoreMemoryBlock* block = &blocks[b];
		if (!(block->flags & params->memoryFlags)) {
			continue;
		}
		void* mem = core->getMemoryBlock(core, block->id, &size);
		if (!mem) {
			continue;
		}
		if (size > block->end - block->start) {
			size = block->end - block->start; // TOOD: Segments
		}
		found += _search(mem, size, block, params, out, limit ? limit - found : 0);
	}
}

bool _testSpecificGuess(struct mCore* core, struct mCoreMemorySearchResult* res, int64_t opValue, enum mCoreMemorySearchOp op) {
	int32_t offset = 0;
	if (op >= mCORE_MEMORY_SEARCH_DELTA) {
		offset = res->oldValue;
	}

	res->oldValue += opValue;
	int64_t value = core->rawRead8(core, res->address, res->segment);
	if (_op(value * res->guessDivisor / res->guessMultiplier - offset, opValue, op)) {
		res->oldValue = value;
		return true;
	}
	if (!(res->address & 1) && (res->width >= 2 || res->width == -1)) {
		value = core->rawRead16(core, res->address, res->segment);
		if (_op(value * res->guessDivisor / res->guessMultiplier - offset, opValue, op)) {
			res->oldValue = value;
			return true;
		}
	}
	if (!(res->address & 3) && (res->width >= 4 || res->width == -1)) {
		value = core->rawRead32(core, res->address, res->segment);
		if (_op(value * res->guessDivisor / res->guessMultiplier - offset, opValue, op)) {
			res->oldValue = value;
			return true;
		}
	}
	res->oldValue -= opValue;
	return false;
}

bool _testGuess(struct mCore* core, struct mCoreMemorySearchResult* res, const struct mCoreMemorySearchParams* params) {
	char* end;
	int64_t value = strtoll(params->valueStr, &end, 10);
	if (end && _testSpecificGuess(core, res, value, params->op)) {
		return true;
	}

	value = strtoll(params->valueStr, &end, 16);
	if (end && _testSpecificGuess(core, res, value, params->op)) {
		return true;
	}
	return false;
}

void mCoreMemorySearchRepeat(struct mCore* core, const struct mCoreMemorySearchParams* params, struct mCoreMemorySearchResults* inout) {
	size_t i;
	for (i = 0; i < mCoreMemorySearchResultsSize(inout); ++i) {
		struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsGetPointer(inout, i);
		switch (res->type) {
		case mCORE_MEMORY_SEARCH_INT:
			if (params->type == mCORE_MEMORY_SEARCH_GUESS) {
				if (!_testGuess(core, res, params)) {
					*res = *mCoreMemorySearchResultsGetPointer(inout, mCoreMemorySearchResultsSize(inout) - 1);
					mCoreMemorySearchResultsResize(inout, -1);
					--i;
				}
			} else if (params->type == mCORE_MEMORY_SEARCH_INT) {
				int32_t match = params->valueInt;
				int32_t value = 0;
				switch (params->width) {
				case 1:
					value = core->rawRead8(core, res->address, res->segment);
					break;
				case 2:
					value = core->rawRead16(core, res->address, res->segment);
					break;
				case 4:
					value = core->rawRead32(core, res->address, res->segment);
					break;
				default:
					break;
				}
				int32_t opValue = value;
				if (params->op >= mCORE_MEMORY_SEARCH_DELTA) {
					opValue -= res->oldValue;
				}
				if (!_op(opValue, match, params->op)) {
					*res = *mCoreMemorySearchResultsGetPointer(inout, mCoreMemorySearchResultsSize(inout) - 1);
					mCoreMemorySearchResultsResize(inout, -1);
					--i;
				} else {
					res->oldValue = value;
				}
			}
			break;
		case mCORE_MEMORY_SEARCH_STRING:
		case mCORE_MEMORY_SEARCH_GUESS:
			// TODO
			break;
		}
	}
}
