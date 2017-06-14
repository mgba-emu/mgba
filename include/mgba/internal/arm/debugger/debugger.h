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

struct ARMDebugBreakpoint {
	uint32_t address;
	bool isSw;
	struct {
		uint32_t opcode;
		enum ExecutionMode mode;
	} sw;
};

struct ARMDebugWatchpoint {
	uint32_t address;
	enum mWatchpointType type;
};

DECLARE_VECTOR(ARMDebugBreakpointList, struct ARMDebugBreakpoint);
DECLARE_VECTOR(ARMDebugWatchpointList, struct ARMDebugWatchpoint);

struct ARMDebugger {
	struct mDebuggerPlatform d;
	struct ARMCore* cpu;

	struct ARMDebugBreakpointList breakpoints;
	struct ARMDebugBreakpointList swBreakpoints;
	struct ARMDebugWatchpointList watchpoints;
	struct ARMMemory originalMemory;

	void (*entered)(struct mDebugger*, enum mDebuggerEntryReason, struct mDebuggerEntryInfo*);

	bool (*setSoftwareBreakpoint)(struct ARMDebugger*, uint32_t address, enum ExecutionMode mode, uint32_t* opcode);
	bool (*clearSoftwareBreakpoint)(struct ARMDebugger*, uint32_t address, enum ExecutionMode mode, uint32_t opcode);
};

struct mDebuggerPlatform* ARMDebuggerPlatformCreate(void);
bool ARMDebuggerSetSoftwareBreakpoint(struct mDebuggerPlatform* debugger, uint32_t address, enum ExecutionMode mode);
void ARMDebuggerClearSoftwareBreakpoint(struct mDebuggerPlatform* debugger, uint32_t address);

CXX_GUARD_END

#endif
