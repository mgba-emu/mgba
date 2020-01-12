/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SM83_DEBUGGER_H
#define SM83_DEBUGGER_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/debugger/debugger.h>

#include <mgba/internal/sm83/sm83.h>

struct SM83Segment {
	uint16_t start;
	uint16_t end;
	const char* name;
};

struct CLIDebuggerSystem;
struct SM83Debugger {
	struct mDebuggerPlatform d;
	struct SM83Core* cpu;

	struct mBreakpointList breakpoints;
	struct mWatchpointList watchpoints;
	struct SM83Memory originalMemory;

	ssize_t nextId;

	const struct SM83Segment* segments;

	void (*printStatus)(struct CLIDebuggerSystem*);
};

struct mDebuggerPlatform* SM83DebuggerPlatformCreate(void);

CXX_GUARD_END

#endif
