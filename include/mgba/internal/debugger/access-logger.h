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

DECL_BITFIELD(mDebuggerAccessLogFlags, uint8_t);
DECL_BIT(mDebuggerAccessLogFlags, Read, 0);
DECL_BIT(mDebuggerAccessLogFlags, Write, 1);
DECL_BIT(mDebuggerAccessLogFlags, Execute, 2);
DECL_BIT(mDebuggerAccessLogFlags, Abort, 3);
DECL_BIT(mDebuggerAccessLogFlags, Access8, 4);
DECL_BIT(mDebuggerAccessLogFlags, Access16, 5);
DECL_BIT(mDebuggerAccessLogFlags, Access32, 6);
DECL_BIT(mDebuggerAccessLogFlags, Access64, 7);

DECL_BITFIELD(mDebuggerAccessLogFlagsEx, uint16_t);
DECL_BIT(mDebuggerAccessLogFlagsEx, AccessProgram, 0);
DECL_BIT(mDebuggerAccessLogFlagsEx, AccessDMA, 1);
DECL_BIT(mDebuggerAccessLogFlagsEx, AccessSystem, 2);
DECL_BIT(mDebuggerAccessLogFlagsEx, AccessDecompress, 3);
DECL_BIT(mDebuggerAccessLogFlagsEx, AccessCopy, 4);
DECL_BIT(mDebuggerAccessLogFlagsEx, ErrorIllegalOpcode, 8);
DECL_BIT(mDebuggerAccessLogFlagsEx, ErrorAccessRead, 9);
DECL_BIT(mDebuggerAccessLogFlagsEx, ErrorAccessWrite, 10);
DECL_BIT(mDebuggerAccessLogFlagsEx, ErrorAccessExecute, 11);

DECL_BIT(mDebuggerAccessLogFlagsEx, ExecuteARM, 14);
DECL_BIT(mDebuggerAccessLogFlagsEx, ExecuteThumb, 15);

DECL_BIT(mDebuggerAccessLogFlagsEx, ExecuteOpcode, 14);
DECL_BIT(mDebuggerAccessLogFlagsEx, ExecuteOperand, 15);

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
