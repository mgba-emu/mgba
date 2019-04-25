/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef LR35902_DEBUGGER_H
#define LR35902_DEBUGGER_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/debugger/debugger.h>

#include <mgba/internal/lr35902/lr35902.h>

struct LR35902Segment {
	uint16_t start;
	uint16_t end;
	const char* name;
};

struct CLIDebuggerSystem;
struct LR35902Debugger {
	struct mDebuggerPlatform d;
	struct LR35902Core* cpu;

	struct mBreakpointList breakpoints;
	struct mWatchpointList watchpoints;
	struct LR35902Memory originalMemory;

	ssize_t nextId;

	const struct LR35902Segment* segments;

	void (*printStatus)(struct CLIDebuggerSystem*);
};

struct mDebuggerPlatform* LR35902DebuggerPlatformCreate(void);

CXX_GUARD_END

#endif
