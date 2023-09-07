/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/debugger/access-logger.h>

#include <mgba/core/core.h>
#include <mgba-util/vfs.h>

#define DEFAULT_MAX_REGIONS 20

const char mAL_MAGIC[] = "mAL\1";

DEFINE_VECTOR(mDebuggerAccessLogRegionList, struct mDebuggerAccessLogRegion);

DECL_BITFIELD(mDebuggerAccessLogHeaderFlags, uint64_t);

struct mDebuggerAccessLogRegionInfo {
	uint32_t start;
	uint32_t end;
	uint32_t size;
	uint32_t segmentStart;
	uint64_t fileOffset;
	uint64_t fileOffsetEx;
	mDebuggerAccessLogRegionFlags flags;
	uint64_t reserved;
};
static_assert(sizeof(struct mDebuggerAccessLogRegionInfo) == 0x30, "mDebuggerAccessLogRegionInfo struct sized wrong");

struct mDebuggerAccessLogHeader {
	char magic[4];
	uint32_t version;
	mDebuggerAccessLogHeaderFlags flags;
	uint8_t nRegions;
	uint8_t regionCapacity;
	uint16_t padding;
	uint32_t platform;
	uint8_t reserved[0x28];
};
static_assert(sizeof(struct mDebuggerAccessLogHeader) == 0x40, "mDebuggerAccessLogHeader struct sized wrong");

struct mDebuggerAccessLog {
	struct mDebuggerAccessLogHeader header;
	struct mDebuggerAccessLogRegionInfo regionInfo[];
};

static void _mDebuggerAccessLoggerEntered(struct mDebuggerModule* debugger, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info) {
	struct mDebuggerAccessLogger* logger = (struct mDebuggerAccessLogger*) debugger;
	logger->d.isPaused = false;

	switch (reason) {
	case DEBUGGER_ENTER_MANUAL:
	case DEBUGGER_ENTER_ATTACHED:
		return;
	case DEBUGGER_ENTER_BREAKPOINT:
	case DEBUGGER_ENTER_STACK:
		mLOG(DEBUGGER, WARN, "Hit unexpected access logger entry type %i", reason);
		return;
	case DEBUGGER_ENTER_WATCHPOINT:
	case DEBUGGER_ENTER_ILLEGAL_OP:
		break;
	}

	size_t i;
	for (i = 0; i < mDebuggerAccessLogRegionListSize(&logger->regions); ++i) {
		struct mDebuggerAccessLogRegion* region = mDebuggerAccessLogRegionListGetPointer(&logger->regions, i);
		if (info->address < region->start || info->address >= region->end) {
			continue;
		}
		size_t offset = info->address - region->start;
		if (info->segment > 0) {
			uint32_t segmentSize = region->end - region->segmentStart;
			offset %= segmentSize;
			offset += segmentSize * info->segment;
		}

		if (offset >= region->size) {
			continue;
		}

		offset &= -info->width;

		int j;
		switch (reason) {
		case DEBUGGER_ENTER_WATCHPOINT:
			for (j = 0; j < info->width; ++j) {
				if (info->type.wp.accessType & WATCHPOINT_WRITE) {
					region->block[offset + j] = mDebuggerAccessLogFlagsFillWrite(region->block[offset + j]);
				}
				if (info->type.wp.accessType & WATCHPOINT_READ) {
					region->block[offset + j] = mDebuggerAccessLogFlagsFillRead(region->block[offset + j]);
				}
			}
			switch (info->width) {
			case 1:
				region->block[offset] = mDebuggerAccessLogFlagsFillAccess8(region->block[offset]);
				break;
			case 2:
				region->block[offset] = mDebuggerAccessLogFlagsFillAccess16(region->block[offset]);
				region->block[offset + 1] = mDebuggerAccessLogFlagsFillAccess16(region->block[offset + 1]);
				break;
			case 4:
				region->block[offset] = mDebuggerAccessLogFlagsFillAccess32(region->block[offset]);
				region->block[offset + 1] = mDebuggerAccessLogFlagsFillAccess32(region->block[offset + 1]);
				region->block[offset + 2] = mDebuggerAccessLogFlagsFillAccess32(region->block[offset + 2]);
				region->block[offset + 3] = mDebuggerAccessLogFlagsFillAccess32(region->block[offset + 3]);
				break;
			case 8:
				region->block[offset] = mDebuggerAccessLogFlagsFillAccess64(region->block[offset]);
				region->block[offset + 1] = mDebuggerAccessLogFlagsFillAccess64(region->block[offset + 1]);
				region->block[offset + 2] = mDebuggerAccessLogFlagsFillAccess64(region->block[offset + 2]);
				region->block[offset + 3] = mDebuggerAccessLogFlagsFillAccess64(region->block[offset + 3]);
				region->block[offset + 4] = mDebuggerAccessLogFlagsFillAccess64(region->block[offset + 4]);
				region->block[offset + 5] = mDebuggerAccessLogFlagsFillAccess64(region->block[offset + 5]);
				region->block[offset + 6] = mDebuggerAccessLogFlagsFillAccess64(region->block[offset + 6]);
				region->block[offset + 7] = mDebuggerAccessLogFlagsFillAccess64(region->block[offset + 7]);
				break;
			}
			break;
		case DEBUGGER_ENTER_ILLEGAL_OP:
			region->block[offset] = mDebuggerAccessLogFlagsFillExecute(region->block[offset]);
			if (region->blockEx) {
				uint16_t ex;
				LOAD_16LE(ex, 0, &region->blockEx[offset]);
				ex = mDebuggerAccessLogFlagsExFillErrorIllegalOpcode(ex);
				STORE_16LE(ex, 0, &region->blockEx[offset]);
			}
			break;
		default:
			break;
		}
	}
}

static void _mDebuggerAccessLoggerCallback(struct mDebuggerModule* debugger) {
	struct mDebuggerAccessLogger* logger = (struct mDebuggerAccessLogger*) debugger;

	struct mDebuggerInstructionInfo info;
	logger->d.p->platform->nextInstructionInfo(logger->d.p->platform, &info);

	size_t i;
	for (i = 0; i < mDebuggerAccessLogRegionListSize(&logger->regions); ++i) {
		struct mDebuggerAccessLogRegion* region = mDebuggerAccessLogRegionListGetPointer(&logger->regions, i);
		if (info.address < region->start || info.address >= region->end) {
			continue;
		}
		size_t offset = info.address - region->start;
		if (info.segment > 0) {
			uint32_t segmentSize = region->end - region->segmentStart;
			offset %= segmentSize;
			offset += segmentSize * info.segment;
		}

		if (offset >= region->size) {
			continue;
		}

		size_t j;
		for (j = 0; j < info.width; ++j) {
			uint16_t ex = 0;
			region->block[offset + j] = mDebuggerAccessLogFlagsFillExecute(region->block[offset + j]);
			region->block[offset + j] |= info.flags[j];

			if (region->blockEx) {
				LOAD_16LE(ex, 0, &region->blockEx[offset + j]);
				ex |= info.flagsEx[j];
				STORE_16LE(ex, 0, &region->blockEx[offset + j]);
			}
		}
	}
}

void mDebuggerAccessLoggerInit(struct mDebuggerAccessLogger* logger) {
	memset(logger, 0, sizeof(*logger));
	mDebuggerAccessLogRegionListInit(&logger->regions, 1);

	logger->d.type = DEBUGGER_ACCESS_LOGGER;
	logger->d.entered = _mDebuggerAccessLoggerEntered;
	logger->d.custom = _mDebuggerAccessLoggerCallback;
}

void mDebuggerAccessLoggerDeinit(struct mDebuggerAccessLogger* logger) {
	mDebuggerAccessLoggerClose(logger);
	mDebuggerAccessLogRegionListDeinit(&logger->regions);
}

static bool _mapRegion(struct mDebuggerAccessLogger* logger, struct mDebuggerAccessLogRegion* region, const struct mDebuggerAccessLogRegionInfo* info) {
	uint64_t offset;
	mDebuggerAccessLogRegionFlags flags;

	LOAD_64LE(offset, 0, &info->fileOffset);
	LOAD_64LE(flags, 0, &info->flags);

#if __SIZEOF_SIZE_T__ <= 4
	if (offset >= 0x100000000ULL) {
		return false;
	}
#endif
	if (offset < sizeof(struct mDebuggerAccessLogHeader)) {
		return false;
	}
	ssize_t fileEnd = logger->backing->size(logger->backing);
	if (fileEnd < 0) {
		return false;
	}
	if ((size_t) fileEnd <= offset) {
		return false;
	}
	if ((size_t) fileEnd < offset + region->size * sizeof(mDebuggerAccessLogFlags)) {
		return false;
	}
	region->block = (mDebuggerAccessLogFlags*) ((uintptr_t) logger->mapped + offset);

	if (mDebuggerAccessLogRegionFlagsIsHasExBlock(flags)) {
		LOAD_64LE(offset, 0, &info->fileOffsetEx);
#if __SIZEOF_SIZE_T__ <= 4
		if (offset >= 0x100000000ULL) {
			return false;
		}
#endif
		if (offset) {
			if (offset < sizeof(struct mDebuggerAccessLogHeader)) {
				return false;
			}
			if ((size_t) fileEnd <= offset) {
				return false;
			}
			if ((size_t) fileEnd < offset + region->size * sizeof(mDebuggerAccessLogFlagsEx)) {
				return false;
			}
			region->blockEx = (mDebuggerAccessLogFlagsEx*) ((uintptr_t) logger->mapped + offset);
		}
	}
	return true;
}

static bool _setupRegion(struct mDebuggerAccessLogger* logger, struct mDebuggerAccessLogRegion* region, const struct mDebuggerAccessLogRegionInfo* info) {
	if (!_mapRegion(logger, region, info)) {
		return false;
	}

	struct mWatchpoint wp = {
		.segment = -1,
		.minAddress = region->start,
		.maxAddress = region->end,
		.type = WATCHPOINT_RW,
	};
	logger->d.p->platform->setWatchpoint(logger->d.p->platform, &logger->d, &wp);
	mDebuggerModuleSetNeedsCallback(&logger->d);
	return true;
}

static bool _remapAll(struct mDebuggerAccessLogger* logger) {
	size_t i;
	for (i = 0; i < mDebuggerAccessLogRegionListSize(&logger->regions); ++i) {
		struct mDebuggerAccessLogRegion* region = mDebuggerAccessLogRegionListGetPointer(&logger->regions, i);
		if (!_mapRegion(logger, region, &logger->mapped->regionInfo[i])) {
			return false;
		}
	}
	return true;
}

static bool mDebuggerAccessLoggerLoad(struct mDebuggerAccessLogger* logger) {
	if (memcmp(logger->mapped->header.magic, mAL_MAGIC, sizeof(logger->mapped->header.magic)) != 0) {
		return false;
	}
	uint32_t version;
	LOAD_32LE(version, 0, &logger->mapped->header.version);
	if (version != 1) {
		return false;
	}

	enum mPlatform platform;
	LOAD_32LE(platform, 0, &logger->mapped->header.platform);
	if (platform != logger->d.p->core->platform(logger->d.p->core)) {
		return false;
	}

	mDebuggerAccessLogRegionListClear(&logger->regions);
	mDebuggerAccessLogRegionListResize(&logger->regions, logger->mapped->header.nRegions);

	size_t i;
	for (i = 0; i < logger->mapped->header.nRegions; ++i) {
		struct mDebuggerAccessLogRegionInfo* info = &logger->mapped->regionInfo[i];
		struct mDebuggerAccessLogRegion* region = mDebuggerAccessLogRegionListGetPointer(&logger->regions, i);
		memset(region, 0, sizeof(*region));
		LOAD_32LE(region->start, 0, &info->start);
		LOAD_32LE(region->end, 0, &info->end);
		LOAD_32LE(region->size, 0, &info->size);
		LOAD_32LE(region->segmentStart, 0, &info->segmentStart);
		if (!_setupRegion(logger, region, info)) {
			mDebuggerAccessLogRegionListClear(&logger->regions);
			return false;
		}
	}
	return true;
}

bool mDebuggerAccessLoggerOpen(struct mDebuggerAccessLogger* logger, struct VFile* vf, int mode) {
	if (logger->backing && !mDebuggerAccessLoggerClose(logger)) {
		return false;
	}

	ssize_t size = vf->size(vf);
	if (size < 0) {
		return false;
	}
	if ((size_t) size < sizeof(struct mDebuggerAccessLogHeader)) {
		if (!(mode & O_CREAT)) {
			return false;
		}
		vf->truncate(vf, sizeof(struct mDebuggerAccessLogHeader) + DEFAULT_MAX_REGIONS * sizeof(struct mDebuggerAccessLogRegionInfo));
		size = sizeof(struct mDebuggerAccessLogHeader);
	}

	logger->mapped = vf->map(vf, size, MAP_WRITE);
	if (!logger->mapped) {
		return false;
	}
	logger->backing = vf;
	bool loaded = false;
	if (!(mode & O_TRUNC)) {
		loaded = mDebuggerAccessLoggerLoad(logger);
	}
	if ((mode & O_CREAT) && ((mode & O_TRUNC) || !loaded)) {
		memset(logger->mapped, 0, sizeof(*logger->mapped));
		memcpy(logger->mapped->header.magic, mAL_MAGIC, sizeof(logger->mapped->header.magic));
		STORE_32LE(1, 0, &logger->mapped->header.version);
		STORE_32LE(DEFAULT_MAX_REGIONS, 0, &logger->mapped->header.regionCapacity);
		vf->sync(vf, NULL, 0);
		loaded = true;
	}
	return loaded;
}

static int _mDebuggerAccessLoggerWatchMemoryBlock(struct mDebuggerAccessLogger* logger, const struct mCoreMemoryBlock* block, mDebuggerAccessLogRegionFlags flags) {
	if (mDebuggerAccessLogRegionListSize(&logger->regions) >= logger->mapped->header.regionCapacity) {
		return -1;
	}

	if (!(block->flags & mCORE_MEMORY_MAPPED)) {
		return -1;
	}

	ssize_t fileEnd = logger->backing->size(logger->backing);
	if (fileEnd < 0) {
		return -1;
	}

	size_t i;
	for (i = 0; i < mDebuggerAccessLogRegionListSize(&logger->regions); ++i) {
		struct mDebuggerAccessLogRegion* region = mDebuggerAccessLogRegionListGetPointer(&logger->regions, i);
		if (region->start != block->start) {
			continue;
		}
		if (region->end != block->end) {
			continue;
		}
		if (region->size != block->size) {
			continue;
		}
		if (!region->blockEx && mDebuggerAccessLogRegionFlagsIsHasExBlock(flags)) {
			size_t size = block->size * sizeof(mDebuggerAccessLogFlagsEx);

			logger->backing->unmap(logger->backing, logger->mapped, fileEnd);
			logger->backing->truncate(logger->backing, fileEnd + size);
			logger->mapped = logger->backing->map(logger->backing, fileEnd + size, MAP_WRITE);

			struct mDebuggerAccessLogRegionInfo* info = &logger->mapped->regionInfo[i];
			mDebuggerAccessLogRegionFlags oldFlags;
			LOAD_64LE(oldFlags, 0, &info->flags);
			oldFlags = mDebuggerAccessLogRegionFlagsFillHasExBlock(oldFlags);

			STORE_64LE(fileEnd, 0, &info->fileOffsetEx);
			STORE_64LE(oldFlags, 0, &info->flags);

			logger->backing->sync(logger->backing, logger->mapped, sizeof(struct mDebuggerAccessLogHeader) + logger->mapped->header.regionCapacity * sizeof(struct mDebuggerAccessLogRegionInfo));

			_remapAll(logger);
		}
		return i;
	}

#if __SIZEOF_SIZE_T__ <= 4
	if (block->size >= 0x80000000) {
		return -1;
	}
#endif
	size_t size = block->size * sizeof(mDebuggerAccessLogFlags);
	if (mDebuggerAccessLogRegionFlagsIsHasExBlock(flags)) {
		size += block->size * sizeof(mDebuggerAccessLogFlagsEx);
	}
	logger->backing->unmap(logger->backing, logger->mapped, fileEnd);
	logger->backing->truncate(logger->backing, fileEnd + size);
	logger->mapped = logger->backing->map(logger->backing, fileEnd + size, MAP_WRITE);

	_remapAll(logger);

	int id = mDebuggerAccessLogRegionListSize(&logger->regions);
	struct mDebuggerAccessLogRegion* region = mDebuggerAccessLogRegionListAppend(&logger->regions);
	memset(region, 0, sizeof(*region));
	region->start = block->start;
	region->end = block->end;
	region->size = block->size;
	region->segmentStart = block->segmentStart;
	region->block = (mDebuggerAccessLogFlags*) ((uintptr_t) logger->backing + fileEnd);

	struct mDebuggerAccessLogRegionInfo* info = &logger->mapped->regionInfo[id];
	STORE_32LE(region->start, 0, &info->start);
	STORE_32LE(region->end, 0, &info->end);
	STORE_32LE(region->segmentStart, 0, &info->segmentStart);
	STORE_64LE(fileEnd, 0, &info->fileOffset);
	if (mDebuggerAccessLogRegionFlagsIsHasExBlock(flags)) {
		STORE_64LE(fileEnd + block->size * sizeof(mDebuggerAccessLogFlags), 0, &info->fileOffsetEx);
	} else {
		STORE_64LE(0, 0, &info->fileOffsetEx);
	}
	STORE_64LE(flags, 0, &info->flags);
	STORE_64LE(0, 0, &info->reserved);
	STORE_32LE(region->size, 0, &info->size);

	logger->mapped->header.nRegions = id + 1;

	logger->backing->sync(logger->backing, logger->mapped, sizeof(struct mDebuggerAccessLogHeader) + logger->mapped->header.regionCapacity * sizeof(struct mDebuggerAccessLogRegionInfo));
	if (!_setupRegion(logger, region, info)) {
		return -1;
	}
	return id;
}

bool mDebuggerAccessLoggerClose(struct mDebuggerAccessLogger* logger) {
	if (!logger->backing) {
		return true;
	}
	mDebuggerAccessLogRegionListClear(&logger->regions);
	logger->backing->unmap(logger->backing, logger->mapped, logger->backing->size(logger->backing));
	logger->mapped = NULL;
	logger->backing->close(logger->backing);
	logger->backing = NULL;
	return true;
}

int mDebuggerAccessLoggerWatchMemoryBlockId(struct mDebuggerAccessLogger* logger, size_t id, mDebuggerAccessLogRegionFlags flags) {
	struct mCore* core = logger->d.p->core;
	const struct mCoreMemoryBlock* blocks;
	size_t nBlocks = core->listMemoryBlocks(core, &blocks);

	size_t i;
	for (i = 0; i < nBlocks; ++i) {
		if (blocks[i].id == id) {
			return _mDebuggerAccessLoggerWatchMemoryBlock(logger, &blocks[i], flags);
		}
	}
	return -1;
}

int mDebuggerAccessLoggerWatchMemoryBlockName(struct mDebuggerAccessLogger* logger, const char* internalName, mDebuggerAccessLogRegionFlags flags) {
	struct mCore* core = logger->d.p->core;
	const struct mCoreMemoryBlock* blocks;
	size_t nBlocks = core->listMemoryBlocks(core, &blocks);

	size_t i;
	for (i = 0; i < nBlocks; ++i) {
		if (strcmp(blocks[i].internalName, internalName) == 0) {
			return _mDebuggerAccessLoggerWatchMemoryBlock(logger, &blocks[i], flags);
		}
	}
	return -1;
}

bool mDebuggerAccessLoggerCreateShadowFile(struct mDebuggerAccessLogger* logger, int regionId, struct VFile* vf, uint8_t fill) {
	if (regionId < 0) {
		return false;
	}
	if ((unsigned) regionId >= mDebuggerAccessLogRegionListSize(&logger->regions)) {
		return false;
	}

	if (vf->seek(vf, 0, SEEK_SET) < 0) {
		return false;
	}
	struct mCore* core = logger->d.p->core;
	struct mDebuggerAccessLogRegion* region = mDebuggerAccessLogRegionListGetPointer(&logger->regions, regionId);
	vf->truncate(vf, region->size);

	uint8_t buffer[0x800];
	int segment = 0;
	uint32_t segmentAddress = region->start;
	uint32_t segmentEnd = region->end;

	uint64_t i;
	for (i = 0; i < region->size; ++i) {
		if (segmentAddress == region->segmentStart && segmentAddress != region->start) {
			++segment;
		}
		if (segmentAddress == segmentEnd) {
			segmentAddress = region->segmentStart;
			++segment;
		}
		if (region->block[i]) {
			buffer[i & 0x7FF] = core->rawRead8(core, segmentAddress, segment);
		} else {
			buffer[i & 0x7FF] = fill;
		}
		if (i && (i & 0x7FF) == 0x7FF) {
			if (vf->write(vf, buffer, 0x800) < 0) {
				return false;
			}
		}
		++segmentAddress;
	}
	if (i & 0x7FF) {
		if (vf->write(vf, buffer, i & 0x7FF) < 0) {
			return false;
		}
	}
	return true;
}
