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
	DEBUGGER_EXITING,
	DEBUGGER_SHUTDOWN
};

struct DebugBreakpoint {
	struct DebugBreakpoint* next;
	uint32_t address;
};

enum DebuggerEntryReason {
	DEBUGGER_ENTER_MANUAL,
	DEBUGGER_ENTER_ATTACHED,
	DEBUGGER_ENTER_BREAKPOINT,
	DEBUGGER_ENTER_WATCHPOINT,
	DEBUGGER_ENTER_ILLEGAL_OP
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
	struct DebugBreakpoint* watchpoints;
	struct ARMMemory originalMemory;

	void (*init)(struct ARMDebugger*);
	void (*deinit)(struct ARMDebugger*);
	void (*paused)(struct ARMDebugger*);
	void (*entered)(struct ARMDebugger*, enum DebuggerEntryReason);

	__attribute__((format (printf, 3, 4)))
	void (*log)(struct ARMDebugger*, enum DebuggerLogLevel, const char* format, ...);
};

void ARMDebuggerCreate(struct ARMDebugger*);
void ARMDebuggerRun(struct ARMDebugger*);
void ARMDebuggerEnter(struct ARMDebugger*, enum DebuggerEntryReason);
void ARMDebuggerSetBreakpoint(struct ARMDebugger* debugger, uint32_t address);
void ARMDebuggerClearBreakpoint(struct ARMDebugger* debugger, uint32_t address);
void ARMDebuggerSetWatchpoint(struct ARMDebugger* debugger, uint32_t address);
void ARMDebuggerClearWatchpoint(struct ARMDebugger* debugger, uint32_t address);

#endif
