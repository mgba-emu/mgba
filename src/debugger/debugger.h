/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DEBUGGER_H
#define DEBUGGER_H

#include "util/common.h"

#include "arm/arm.h"
#include "core/log.h"
#include "util/vector.h"

mLOG_DECLARE_CATEGORY(DEBUGGER);

extern const uint32_t DEBUGGER_ID;

enum DebuggerType {
	DEBUGGER_NONE = 0,
#ifdef USE_CLI_DEBUGGER
	DEBUGGER_CLI,
#endif
#ifdef USE_GDB_STUB
	DEBUGGER_GDB,
#endif
	DEBUGGER_MAX
};

enum DebuggerState {
	DEBUGGER_PAUSED,
	DEBUGGER_RUNNING,
	DEBUGGER_CUSTOM,
	DEBUGGER_SHUTDOWN
};

struct DebugBreakpoint {
	uint32_t address;
	bool isSw;
	struct {
		uint32_t opcode;
		enum ExecutionMode mode;
	} sw;
};

enum WatchpointType {
	WATCHPOINT_WRITE = 1,
	WATCHPOINT_READ = 2,
	WATCHPOINT_RW = WATCHPOINT_WRITE | WATCHPOINT_READ
};

struct DebugWatchpoint {
	uint32_t address;
	enum WatchpointType type;
};

DECLARE_VECTOR(DebugBreakpointList, struct DebugBreakpoint);
DECLARE_VECTOR(DebugWatchpointList, struct DebugWatchpoint);

enum DebuggerEntryReason {
	DEBUGGER_ENTER_MANUAL,
	DEBUGGER_ENTER_ATTACHED,
	DEBUGGER_ENTER_BREAKPOINT,
	DEBUGGER_ENTER_WATCHPOINT,
	DEBUGGER_ENTER_ILLEGAL_OP
};

struct DebuggerEntryInfo {
	uint32_t address;
	union {
		struct {
			uint32_t oldValue;
			uint32_t newValue;
			enum WatchpointType watchType;
			enum WatchpointType accessType;
		} b;

		struct {
			uint32_t opcode;
		} c;
	} a;
};

struct Debugger {
	struct mCPUComponent d;
	enum DebuggerState state;
	struct ARMCore* cpu;

	struct DebugBreakpointList breakpoints;
	struct DebugBreakpointList swBreakpoints;
	struct DebugWatchpointList watchpoints;
	struct ARMMemory originalMemory;

	struct DebugBreakpoint* currentBreakpoint;

	void (*init)(struct Debugger*);
	void (*deinit)(struct Debugger*);
	void (*paused)(struct Debugger*);
	void (*entered)(struct Debugger*, enum DebuggerEntryReason, struct DebuggerEntryInfo*);
	void (*custom)(struct Debugger*);

	bool (*setSoftwareBreakpoint)(struct Debugger*, uint32_t address, enum ExecutionMode mode, uint32_t* opcode);
	bool (*clearSoftwareBreakpoint)(struct Debugger*, uint32_t address, enum ExecutionMode mode, uint32_t opcode);
};

void DebuggerCreate(struct Debugger*);
void DebuggerRun(struct Debugger*);
void DebuggerEnter(struct Debugger*, enum DebuggerEntryReason, struct DebuggerEntryInfo*);
void DebuggerSetBreakpoint(struct Debugger* debugger, uint32_t address);
bool DebuggerSetSoftwareBreakpoint(struct Debugger* debugger, uint32_t address, enum ExecutionMode mode);
void DebuggerClearBreakpoint(struct Debugger* debugger, uint32_t address);
void DebuggerSetWatchpoint(struct Debugger* debugger, uint32_t address, enum WatchpointType type);
void DebuggerClearWatchpoint(struct Debugger* debugger, uint32_t address);

#endif
