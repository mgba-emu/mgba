/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/mem-search.h>

#include <mgba/core/core.h>
#include <mgba/core/interface.h>

DEFINE_VECTOR(mCoreMemorySearchResults, struct mCoreMemorySearchResult);

static bool _op(int64_t value, int64_t refVal, enum mCoreMemorySearchOp op) {
	switch (op) {
	case mCORE_MEMORY_SEARCH_INCREASE:
	case mCORE_MEMORY_SEARCH_GREATER:
		return value > refVal;
	case mCORE_MEMORY_SEARCH_NOT_INCREASE:
	case mCORE_MEMORY_SEARCH_NOT_GREATER:
		return value <= refVal;
	case mCORE_MEMORY_SEARCH_DECREASE:
	case mCORE_MEMORY_SEARCH_LESS:
		return value < refVal;
	case mCORE_MEMORY_SEARCH_NOT_DECREASE:
	case mCORE_MEMORY_SEARCH_NOT_LESS:
		return value >= refVal;
	case mCORE_MEMORY_SEARCH_NOT_CHANGED:
	case mCORE_MEMORY_SEARCH_EQUAL:
	case mCORE_MEMORY_SEARCH_CHANGED_BY:
	case mCORE_MEMORY_SEARCH_INCREASE_BY:
	case mCORE_MEMORY_SEARCH_DECREASE_BY:
		return value == refVal;
	case mCORE_MEMORY_SEARCH_ANY:
		return true;
	case mCORE_MEMORY_SEARCH_CHANGED:
	case mCORE_MEMORY_SEARCH_NOT_EQUAL:
		return value != refVal;
	}
	return false;
}

static void _addResult(struct mCoreMemorySearchResults* out, uint32_t address, int segment,
                       enum mCoreMemorySearchType type, int width, int64_t curValue, bool signedNum) {
	struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
	res->address = address;
	res->type = type;
	res->width = width;
	res->segment = segment; 
	res->signedNum = signedNum;
	res->curValue = curValue;
	res->oldValue = res->curValue;
}

static size_t _searchInt(const void* mem, size_t size, const struct mCoreMemoryBlock* block,
                         const struct mCoreMemorySearchParams* params, struct mCoreMemorySearchResults* out,
                         size_t limit) {
	size_t found = 0;
	if (params->align == params->width || params->align == -1) {
		const uint32_t* mem32 = mem;
		const uint16_t* mem16 = mem;
		const uint8_t* mem8 = mem;
		const int32_t* mem32s = mem;
		const int16_t* mem16s = mem;
		const int8_t* mem8s = mem;

		uint32_t start = block->start;
		uint32_t end = size; // TODO: Segments
		size_t i;
		// TODO: Big endian
		uint32_t r_start = (int32_t) (params->start - start) < 0 ? 0 : (params->start - start);
		uint32_t r_end = (params->end - start) < end ? (params->end - start) : end;
		for (i = r_start; (!limit || found < limit) && i < r_end; i += 1) {
			if ((params->width & 4) && (i % 4 == 0) &&
			    (params->signedNum ? _op(mem32s[i >> 2], params->valueInt, params->op) : _op(mem32[i >> 2], params->valueInt, params->op))) { 
				_addResult(out, start + i, -1, mCORE_MEMORY_SEARCH_INT, 4,
				           params->signedNum ? (int64_t) mem32s[i >> 2] : (int64_t) mem32[i >> 2] , params->signedNum);
				++found;
			}
			if ((params->width & 2) && (i % 2 == 0) &&
			    (params->signedNum ? _op(mem16s[i >> 1], params->valueInt, params->op) : _op(mem16[i >> 1], params->valueInt, params->op))) {
				_addResult(out, start + i, -1, mCORE_MEMORY_SEARCH_INT, 2,
				           params->signedNum ? mem16s[i >> 1] : mem16[i >> 1], params->signedNum);
				++found;
			}
			if ((params->width & 1) && (params->signedNum ? _op(mem8s[i], params->valueInt, params->op) : _op(mem8[i], params->valueInt, params->op))) {
				_addResult(out, start + i, -1, mCORE_MEMORY_SEARCH_INT, 1,
				           params->signedNum ? mem8s[i] : mem8[i], params->signedNum);
				++found;
			}	
		}
	}
	return found;
}

static size_t _searchStr(const void* mem, size_t size, const struct mCoreMemoryBlock* block,
                         const struct mCoreMemorySearchParams* params, struct mCoreMemorySearchResults* out, size_t limit) {
	const char* memStr = mem;
	size_t found = 0;
	uint32_t start = block->start;
	uint32_t end = size; // TODO: Segments
	size_t i;
	uint32_t r_start = (int32_t) (params->start - start) < 0 ? 0 : (params->start - start);
	uint32_t r_end = (params->end-start) < end  ? (params->end-start) : end;
	r_end = r_end < (uint32_t)params->width ? 0 : r_end - params->width;
	for (i = r_start; (!limit || found < limit) && i < r_end; i += 1) {
		if (!memcmp(params->valueStr, &memStr[i], params->width)) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsAppend(out);
			res->address = start + i;
			res->type = mCORE_MEMORY_SEARCH_STRING;
			res->width = params->width;
			res->segment = -1; // TODO
			++found;
		}
	}
	return found;
}

static size_t _search(const void* mem, size_t size, const struct mCoreMemoryBlock* block,
                      const struct mCoreMemorySearchParams* params, struct mCoreMemorySearchResults* out,
                      size_t limit) {
	switch (params->type) {
	case mCORE_MEMORY_SEARCH_INT:
		return _searchInt(mem, size, block, params, out, limit);
	case mCORE_MEMORY_SEARCH_STRING:
		return _searchStr(mem, size, block, params, out, limit);
	default:
		return 0;
	}
}

void mCoreMemorySearch(struct mCore* core, const struct mCoreMemorySearchParams* params,
                       struct mCoreMemorySearchResults* out, size_t limit) {
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
		if ((block->start > params->end) || (block->end < params->start)) {
			continue;
		}
		found += _search(mem, size, block, params, out, limit ? limit - found : 0);
	}
}


void mCoreMemorySearchRepeat(struct mCore* core, const struct mCoreMemorySearchParams* params,
                             struct mCoreMemorySearchResults* inout) {
	if (params->type == mCORE_MEMORY_SEARCH_STRING) {
		if (params->op == mCORE_MEMORY_SEARCH_CHANGED){
			// TODO
		}
		return;
	} else {
		size_t i;
		for (i = 0; i < mCoreMemorySearchResultsSize(inout); ++i) {
			struct mCoreMemorySearchResult* res = mCoreMemorySearchResultsGetPointer(inout, i);
			if (res->address > params->end || res->address < params->start) {
				*res = *mCoreMemorySearchResultsGetPointer(inout, mCoreMemorySearchResultsSize(inout) - 1);
				mCoreMemorySearchResultsResize(inout, -1);
				--i;
			} else {
				res->oldValue = res->curValue;
				res->signedNum |= params->signedNum;
				int64_t value = 0;
				uint32_t rawVal;
				switch (res->width) {
				case 1:
					rawVal = core->rawRead8(core, res->address, res->segment);
					if (res->signedNum) {
						int8_t temp;
						memcpy(&temp, &rawVal, 1);
						value = temp;
					} else {
						value = rawVal;
					}
					break;
				case 2:
					rawVal = core->rawRead16(core, res->address, res->segment);
					if (res->signedNum) {
						int16_t temp;
						memcpy(&temp, &rawVal, 2);
						value = temp;
					} else {
						value = rawVal;
					}
					break;
				case 4:
					rawVal = core->rawRead32(core, res->address, res->segment);
					if (res->signedNum) {
						int32_t temp;
						memcpy(&temp, &rawVal, 4);
						value = temp;
					} else {
						value = rawVal;
					}
					break;
				default:
					break;
				}
				int64_t curVal;
				int64_t refVal;
				switch (params->op) {
				case mCORE_MEMORY_SEARCH_INCREASE_BY:
					curVal = value - res->oldValue;
					refVal = params->valueInt;
					break;
				case mCORE_MEMORY_SEARCH_DECREASE_BY:
					curVal = res->oldValue - value;
					refVal = params->valueInt;
					break;
				case mCORE_MEMORY_SEARCH_CHANGED_BY:
					curVal = llabs(value - res->oldValue);
					refVal = params->valueInt;
					break;
				case mCORE_MEMORY_SEARCH_EQUAL:
				case mCORE_MEMORY_SEARCH_NOT_EQUAL:
				case mCORE_MEMORY_SEARCH_GREATER:
				case mCORE_MEMORY_SEARCH_NOT_GREATER:
				case mCORE_MEMORY_SEARCH_LESS:
				case mCORE_MEMORY_SEARCH_NOT_LESS:
					curVal = value;
					refVal = params->valueInt;
					break;
				case mCORE_MEMORY_SEARCH_DECREASE:
				case mCORE_MEMORY_SEARCH_INCREASE:
				case mCORE_MEMORY_SEARCH_NOT_INCREASE:
				case mCORE_MEMORY_SEARCH_NOT_DECREASE:
				case mCORE_MEMORY_SEARCH_CHANGED:
				case mCORE_MEMORY_SEARCH_NOT_CHANGED:
				default:
					curVal = value;
					refVal = res->oldValue;
					break;
				}
				if (!_op(curVal, refVal, params->op)) {
					*res = *mCoreMemorySearchResultsGetPointer(inout, mCoreMemorySearchResultsSize(inout) - 1);
					mCoreMemorySearchResultsResize(inout, -1);
					--i;
				} else {
					res->curValue = value;
				}
			}
		}
	}
}
