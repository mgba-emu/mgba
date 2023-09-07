/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef ACCESS_LOGGER_H
#define ACCESS_LOGGER_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/debugger/debugger.h>
#include <mgba-util/vector.h>

DECL_BITFIELD(mDebuggerAccessLogRegionFlags, uint64_t);
DECL_BIT(mDebuggerAccessLogRegionFlags, HasExBlock, 0);

struct mDebuggerAccessLogRegion {
	uint32_t start;
	uint32_t end;
	uint32_t size;
	uint32_t segmentStart;
	mDebuggerAccessLogFlags* block;
	mDebuggerAccessLogFlagsEx* blockEx;
};

DECLARE_VECTOR(mDebuggerAccessLogRegionList, struct mDebuggerAccessLogRegion);

struct mDebuggerAccessLog;
struct mDebuggerAccessLogger {
	struct mDebuggerModule d;
	struct VFile* backing;
	struct mDebuggerAccessLog* mapped;
	struct mDebuggerAccessLogRegionList regions;
};

void mDebuggerAccessLoggerInit(struct mDebuggerAccessLogger*);
void mDebuggerAccessLoggerDeinit(struct mDebuggerAccessLogger*);

bool mDebuggerAccessLoggerOpen(struct mDebuggerAccessLogger*, struct VFile*, int mode);
bool mDebuggerAccessLoggerClose(struct mDebuggerAccessLogger*);

int mDebuggerAccessLoggerWatchMemoryBlockId(struct mDebuggerAccessLogger*, size_t id, mDebuggerAccessLogRegionFlags);
int mDebuggerAccessLoggerWatchMemoryBlockName(struct mDebuggerAccessLogger*, const char* internalName, mDebuggerAccessLogRegionFlags);

bool mDebuggerAccessLoggerCreateShadowFile(struct mDebuggerAccessLogger*, int region, struct VFile*, uint8_t fill);

CXX_GUARD_END

#endif
