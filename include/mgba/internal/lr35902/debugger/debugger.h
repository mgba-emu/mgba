/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef LR35902_DEBUGGER_H
#define LR35902_DEBUGGER_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/internal/debugger/debugger.h>

struct LR35902DebugBreakpoint {
	uint16_t address;
	int segment;
};

struct LR35902DebugWatchpoint {
	uint16_t address;
	int segment;
	enum mWatchpointType type;
};

DECLARE_VECTOR(LR35902DebugBreakpointList, struct LR35902DebugBreakpoint);
DECLARE_VECTOR(LR35902DebugWatchpointList, struct LR35902DebugWatchpoint);

struct LR35902Debugger {
	struct mDebuggerPlatform d;
	struct LR35902Core* cpu;

	struct LR35902DebugBreakpointList breakpoints;
	struct LR35902DebugWatchpointList watchpoints;
};

struct mDebuggerPlatform* LR35902DebuggerPlatformCreate(void);

CXX_GUARD_END

#endif
