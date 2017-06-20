/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/mem-search.h>

#include <mgba/core/core.h>
#include <mgba/core/interface.h>

DEFINE_VECTOR(mCoreMemorySearchResults, struct mCoreMemorySearchResult);

static size_t _search32(const void* mem, size_t size, const struct mCoreMemoryBlock* block, uint32_t value32, struct mCoreMemorySearchResults* out, size_t limit) {
	const uint32_t* mem32 = mem;
	size_t found = 0;
	uint32_t start = block->start;
	uint32_t end = size; // TODO: Segments
	size_t i;
	// TODO: Big endian
	for (i = 0; (!limit || found < limit) && i < end; i += 16) {
		int mask = 0;
		mask |= (mem32[(i >> 2) + 0] == value32) << 0;
		mask |= (mem32[(i >> 2) + 1] == value32) << 1;
		mask |= (mem32[(i >> 2) + 2] == value32) << 2;
		mask |= (mem32[(i >> 2) + 3] == value32) << 3;
		if (!mask) {
			continue;
		}
		if ((mask & 1) && (!limit || found < limit)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i;
			res->type = mCORE_MEMORY_SEARCH_32;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			++found;
		}
		if ((mask & 2) && (!limit || found < limit)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i + 4;
			res->type = mCORE_MEMORY_SEARCH_32;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			++found;
		}
		if ((mask & 4) && (!limit || found < limit)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i + 8;
			res->type = mCORE_MEMORY_SEARCH_32;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			++found;
		}
		if ((mask & 8) && (!limit || found < limit)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i + 12;
			res->type = mCORE_MEMORY_SEARCH_32;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			++found;
		}
	}
	// TODO: last 12 bytes
	return found;
}

static size_t _search16(const void* mem, size_t size, const struct mCoreMemoryBlock* block, uint16_t value16, struct mCoreMemorySearchResults* out, size_t limit) {
	const uint16_t* mem16 = mem;
	size_t found = 0;
	uint32_t start = block->start;
	uint32_t end = size; // TODO: Segments
	size_t i;
	// TODO: Big endian
	for (i = 0; (!limit || found < limit) && i < end; i += 16) {
		int mask = 0;
		mask |= (mem16[(i >> 1) + 0] == value16) << 0;
		mask |= (mem16[(i >> 1) + 1] == value16) << 1;
		mask |= (mem16[(i >> 1) + 2] == value16) << 2;
		mask |= (mem16[(i >> 1) + 3] == value16) << 3;
		mask |= (mem16[(i >> 1) + 4] == value16) << 4;
		mask |= (mem16[(i >> 1) + 5] == value16) << 5;
		mask |= (mem16[(i >> 1) + 6] == value16) << 6;
		mask |= (mem16[(i >> 1) + 7] == value16) << 7;
		if (!mask) {
			continue;
		}
		if ((mask & 1) && (!limit || found < limit)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i;
			res->type = mCORE_MEMORY_SEARCH_16;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			++found;
		}
		if ((mask & 2) && (!limit || found < limit)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i + 2;
			res->type = mCORE_MEMORY_SEARCH_16;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			++found;
		}
		if ((mask & 4) && (!limit || found < limit)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i + 4;
			res->type = mCORE_MEMORY_SEARCH_16;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			++found;
		}
		if ((mask & 8) && (!limit || found < limit)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i + 6;
			res->type = mCORE_MEMORY_SEARCH_16;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			++found;
		}
		if ((mask & 16) && (!limit || found < limit)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i + 8;
			res->type = mCORE_MEMORY_SEARCH_16;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			++found;
		}
		if ((mask & 32) && (!limit || found < limit)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i + 10;
			res->type = mCORE_MEMORY_SEARCH_16;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			++found;
		}
		if ((mask & 64) && (!limit || found < limit)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i + 12;
			res->type = mCORE_MEMORY_SEARCH_16;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			++found;
		}
		if ((mask & 128) && (!limit || found < limit)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i + 14;
			res->type = mCORE_MEMORY_SEARCH_16;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			++found;
		}
	}
	// TODO: last 14 bytes
	return found;
}

static size_t _search8(const void* mem, size_t size, const struct mCoreMemoryBlock* block, uint8_t value8, struct mCoreMemorySearchResults* out, size_t limit) {
	const uint8_t* mem8 = mem;
	size_t found = 0;
	uint32_t start = block->start;
	uint32_t end = size; // TODO: Segments
	size_t i;
	for (i = 0; (!limit || found < limit) && i < end; i += 8) {
		int mask = 0;
		mask |= (mem8[i + 0] == value8) << 0;
		mask |= (mem8[i + 1] == value8) << 1;
		mask |= (mem8[i + 2] == value8) << 2;
		mask |= (mem8[i + 3] == value8) << 3;
		mask |= (mem8[i + 4] == value8) << 4;
		mask |= (mem8[i + 5] == value8) << 5;
		mask |= (mem8[i + 6] == value8) << 6;
		mask |= (mem8[i + 7] == value8) << 7;
		if (!mask) {
			continue;
		}
		if ((mask & 1) && (!limit || found < limit)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i;
			res->type = mCORE_MEMORY_SEARCH_8;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			++found;
		}
		if ((mask & 2) && (!limit || found < limit)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i + 1;
			res->type = mCORE_MEMORY_SEARCH_8;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			++found;
		}
		if ((mask & 4) && (!limit || found < limit)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i + 2;
			res->type = mCORE_MEMORY_SEARCH_8;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			++found;
		}
		if ((mask & 8) && (!limit || found < limit)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i + 3;
			res->type = mCORE_MEMORY_SEARCH_8;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			++found;
		}
		if ((mask & 16) && (!limit || found < limit)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i + 4;
			res->type = mCORE_MEMORY_SEARCH_8;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			++found;
		}
		if ((mask & 32) && (!limit || found < limit)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i + 5;
			res->type = mCORE_MEMORY_SEARCH_8;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			++found;
		}
		if ((mask & 64) && (!limit || found < limit)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i + 6;
			res->type = mCORE_MEMORY_SEARCH_8;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			++found;
		}
		if ((mask & 128) && (!limit || found < limit)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i + 7;
			res->type = mCORE_MEMORY_SEARCH_8;
			res->segment = -1; // TODO
			res->guessDivisor = 1;
			++found;
		}
	}
	// TODO: last 7 bytes
	return found;
}

static size_t _searchStr(const void* mem, size_t size, const struct mCoreMemoryBlock* block, const char* valueStr, struct mCoreMemorySearchResults* out, size_t limit) {
	const char* memStr = mem;
	size_t found = 0;
	size_t len = strlen(valueStr);
	uint32_t start = block->start;
	uint32_t end = size; // TODO: Segments
	size_t i;
	for (i = 0; (!limit || found < limit) && i < end - len; ++i) {
		if (!strncmp(valueStr, &memStr[i], len)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i;
			res->type = mCORE_MEMORY_SEARCH_STRING;
			res->segment = -1; // TODO
			++found;
		}
	}
	return found;
}

static size_t _searchGuess(const void* mem, size_t size, const struct mCoreMemoryBlock* block, const char* valueStr, struct mCoreMemorySearchResults* out, size_t limit) {
	// TODO: As str

	char* end;
	uint64_t value;

	size_t found = 0;

	struct mCoreMemorySearchResults tmp;
	mCoreMemorySearchResultsInit(&tmp, 0);

	// Decimal:
	value = strtoull(valueStr, &end, 10);
	if (end && !end[0]) {
		if (value > 0x10000) {
			found += _search32(mem, size, block, value, out, limit ? limit - found : 0);
		} else if (value > 0x100) {
			found += _search16(mem, size, block, value, out, limit ? limit - found : 0);
		} else {
			found += _search8(mem, size, block, value, out, limit ? limit - found : 0);
		}

		uint32_t divisor = 1;
		while (value && !(value % 10)) {
			mCoreMemorySearchResultsClear(&tmp);
			value /= 10;
			divisor *= 10;

			if (value > 0x10000) {
				found += _search32(mem, size, block, value, &tmp, limit ? limit - found : 0);
			} else if (value > 0x100) {
				found += _search16(mem, size, block, value, &tmp, limit ? limit - found : 0);
			} else {
				found += _search8(mem, size, block, value, &tmp, limit ? limit - found : 0);
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
	value = strtoull(valueStr, &end, 16);
	if (end && !end[0]) {
		if (value > 0x10000) {
			found += _search32(mem, size, block, value, out, limit ? limit - found : 0);
		} else if (value > 0x100) {
			found += _search16(mem, size, block, value, out, limit ? limit - found : 0);
		} else {
			found += _search8(mem, size, block, value, out, limit ? limit - found : 0);
		}

		uint32_t divisor = 1;
		while (value && !(value & 0xF)) {
			mCoreMemorySearchResultsClear(&tmp);
			value >>= 4;
			divisor <<= 4;

			if (value > 0x10000) {
				found += _search32(mem, size, block, value, &tmp, limit ? limit - found : 0);
			} else if (value > 0x100) {
				found += _search16(mem, size, block, value, &tmp, limit ? limit - found : 0);
			} else {
				found += _search8(mem, size, block, value, &tmp, limit ? limit - found : 0);
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
	case mCORE_MEMORY_SEARCH_32:
		return _search32(mem, size, block, params->value32, out, limit);
	case mCORE_MEMORY_SEARCH_16:
		return _search16(mem, size, block, params->value16, out, limit);
	case mCORE_MEMORY_SEARCH_8:
		return _search8(mem, size, block, params->value8, out, limit);
	case mCORE_MEMORY_SEARCH_STRING:
		return _searchStr(mem, size, block, params->valueStr, out, limit);
	case mCORE_MEMORY_SEARCH_GUESS:
		return _searchGuess(mem, size, block, params->valueStr, out, limit);
	}
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

bool _testGuess(struct mCore* core, const struct mCoreMemorySearchResult* res, const struct mCoreMemorySearchParams* params) {
	uint64_t value;
	char* end;

	value = strtoull(params->valueStr, &end, 10);
	if (end) {
		if (core->rawRead8(core, res->address, res->segment) * res->guessDivisor == value) {
			return true;
		}
		if (!(res->address & 1) && core->rawRead16(core, res->address, res->segment) * res->guessDivisor == value) {
			return true;
		}
		if (!(res->address & 3) && core->rawRead32(core, res->address, res->segment) * res->guessDivisor == value) {
			return true;
		}
	}

	value = strtoull(params->valueStr, &end, 16);
	if (end) {
		if (core->rawRead8(core, res->address, res->segment) * res->guessDivisor == value) {
			return true;
		}
		if (!(res->address & 1) && core->rawRead16(core, res->address, res->segment) * res->guessDivisor == value) {
			return true;
		}
		if (!(res->address & 3) && core->rawRead32(core, res->address, res->segment) * res->guessDivisor == value) {
			return true;
		}
	}
	return false;
}

void mCoreMemorySearchRepeat(struct mCore* core, const struct mCoreMemorySearchParams* params, struct mCoreMemorySearchResults* inout) {
	size_t i;
	for (i = 0; i < mCoreMemorySearchResultsSize(inout); ++i) {
		struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsGetPointer(inout, i);
		switch (res->type) {
		case mCORE_MEMORY_SEARCH_8:
			switch (params->type) {
			case mCORE_MEMORY_SEARCH_8:
				if (core->rawRead8(core, res->address, res->segment) != params->value8) {
					mCoreMemorySearchResultsShift(inout, i, 1);
					--i;
				}
				break;
			case mCORE_MEMORY_SEARCH_16:
				if (core->rawRead8(core, res->address, res->segment) != params->value16) {
					mCoreMemorySearchResultsShift(inout, i, 1);
					--i;
				}
				break;
			case mCORE_MEMORY_SEARCH_32:
				if (core->rawRead32(core, res->address, res->segment) != params->value32) {
					mCoreMemorySearchResultsShift(inout, i, 1);
					--i;
				}
				break;
			case mCORE_MEMORY_SEARCH_GUESS:
				if (!_testGuess(core, res, params)) {
					mCoreMemorySearchResultsShift(inout, i, 1);
					--i;
				}
				break;
			default:
				break;
			}
			break;
		case mCORE_MEMORY_SEARCH_16:
			switch (params->type) {
			case mCORE_MEMORY_SEARCH_16:
				if (core->rawRead16(core, res->address, res->segment) != params->value16) {
					mCoreMemorySearchResultsShift(inout, i, 1);
					--i;
				}
				break;
			case mCORE_MEMORY_SEARCH_32:
				if (core->rawRead32(core, res->address, res->segment) != params->value32) {
					mCoreMemorySearchResultsShift(inout, i, 1);
					--i;
				}
				break;
			case mCORE_MEMORY_SEARCH_GUESS:
				if (!_testGuess(core, res, params)) {
					mCoreMemorySearchResultsShift(inout, i, 1);
					--i;
				}
				break;
			default:
				break;
			}
			break;
		case mCORE_MEMORY_SEARCH_32:
			switch (params->type) {
			case mCORE_MEMORY_SEARCH_32:
				if (core->rawRead32(core, res->address, res->segment) != params->value32) {
					mCoreMemorySearchResultsShift(inout, i, 1);
					--i;
				}
				break;
			case mCORE_MEMORY_SEARCH_GUESS:
				if (!_testGuess(core, res, params)) {
					mCoreMemorySearchResultsShift(inout, i, 1);
					--i;
				}
				break;
			default:
				break;
			}
			break;
		case mCORE_MEMORY_SEARCH_STRING:
		case mCORE_MEMORY_SEARCH_GUESS:
			// TOOD
			break;
		}
	}
}
