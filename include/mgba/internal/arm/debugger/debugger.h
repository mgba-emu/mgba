/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef ARM_DEBUGGER_H
#define ARM_DEBUGGER_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/debugger/debugger.h>

#include <mgba/internal/arm/arm.h>
#include <mgba-util/vector.h>

struct ParseTree;
struct ARMDebugBreakpoint {
	struct mBreakpoint d;
	struct {
		uint32_t opcode;
		enum ExecutionMode mode;
	} sw;
};

DECLARE_VECTOR(ARMDebugBreakpointList, struct ARMDebugBreakpoint);

struct ARMDebugger {
	struct mDebuggerPlatform d;
	struct ARMCore* cpu;

	struct ARMDebugBreakpointList breakpoints;
	struct ARMDebugBreakpointList swBreakpoints;
	struct mWatchpointList watchpoints;
	struct ARMMemory originalMemory;

	ssize_t nextId;
	uint32_t stackTraceMode;

	void (*entered)(struct mDebugger*, enum mDebuggerEntryReason, struct mDebuggerEntryInfo*);

	bool (*setSoftwareBreakpoint)(struct ARMDebugger*, uint32_t address, enum ExecutionMode mode, uint32_t* opcode);
	void (*clearSoftwareBreakpoint)(struct ARMDebugger*, const struct ARMDebugBreakpoint*);
};

struct mDebuggerPlatform* ARMDebuggerPlatformCreate(void);
ssize_t ARMDebuggerSetSoftwareBreakpoint(struct mDebuggerPlatform* debugger, uint32_t address, enum ExecutionMode mode);

CXX_GUARD_END

#endif
