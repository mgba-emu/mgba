/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DEBUGGER_H
#define DEBUGGER_H

#include "util/common.h"

#include "arm.h"

extern const uint32_t ARM_DEBUGGER_ID;

enum DebuggerState {
	DEBUGGER_PAUSED,
	DEBUGGER_RUNNING,
	DEBUGGER_CUSTOM,
	DEBUGGER_SHUTDOWN
};

struct DebugBreakpoint {
	struct DebugBreakpoint* next;
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
	struct DebugWatchpoint* next;
	uint32_t address;
	enum WatchpointType type;
};

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
			enum WatchpointType watchType;
		};

		struct {
			uint32_t opcode;
		};
	};
};

enum DebuggerLogLevel {
	DEBUGGER_LOG_DEBUG = 0x01,
	DEBUGGER_LOG_INFO = 0x02,
	DEBUGGER_LOG_WARN = 0x04,
	DEBUGGER_LOG_ERROR = 0x08
};

struct ARMDebugger {
	struct ARMComponent d;
	enum DebuggerState state;
	struct ARMCore* cpu;

	struct DebugBreakpoint* breakpoints;
	struct DebugBreakpoint* swBreakpoints;
	struct DebugWatchpoint* watchpoints;
	struct ARMMemory originalMemory;

	struct DebugBreakpoint* currentBreakpoint;

	void (*init)(struct ARMDebugger*);
	void (*deinit)(struct ARMDebugger*);
	void (*paused)(struct ARMDebugger*);
	void (*entered)(struct ARMDebugger*, enum DebuggerEntryReason, struct DebuggerEntryInfo*);
	void (*custom)(struct ARMDebugger*);

	bool (*setSoftwareBreakpoint)(struct ARMDebugger*, uint32_t address, enum ExecutionMode mode, uint32_t* opcode);
	bool (*clearSoftwareBreakpoint)(struct ARMDebugger*, uint32_t address, enum ExecutionMode mode, uint32_t opcode);

	ATTRIBUTE_FORMAT(printf, 3, 4)
	void (*log)(struct ARMDebugger*, enum DebuggerLogLevel, const char* format, ...);
};

void ARMDebuggerCreate(struct ARMDebugger*);
void ARMDebuggerRun(struct ARMDebugger*);
void ARMDebuggerEnter(struct ARMDebugger*, enum DebuggerEntryReason, struct DebuggerEntryInfo*);
void ARMDebuggerSetBreakpoint(struct ARMDebugger* debugger, uint32_t address);
bool ARMDebuggerSetSoftwareBreakpoint(struct ARMDebugger* debugger, uint32_t address, enum ExecutionMode mode);
void ARMDebuggerClearBreakpoint(struct ARMDebugger* debugger, uint32_t address);
void ARMDebuggerSetWatchpoint(struct ARMDebugger* debugger, uint32_t address, enum WatchpointType type);
void ARMDebuggerClearWatchpoint(struct ARMDebugger* debugger, uint32_t address);

#endif
