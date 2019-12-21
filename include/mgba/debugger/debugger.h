/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DEBUGGER_H
#define DEBUGGER_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/cpu.h>
#include <mgba/core/log.h>
#include <mgba-util/vector.h>

mLOG_DECLARE_CATEGORY(DEBUGGER);

extern const uint32_t DEBUGGER_ID;

enum mDebuggerType {
	DEBUGGER_NONE = 0,
	DEBUGGER_CLI,
#ifdef USE_GDB_STUB
	DEBUGGER_GDB,
#endif
	DEBUGGER_MAX
};

enum mDebuggerState {
	DEBUGGER_PAUSED,
	DEBUGGER_RUNNING,
	DEBUGGER_CUSTOM,
	DEBUGGER_SHUTDOWN
};

enum mWatchpointType {
	WATCHPOINT_WRITE = 1,
	WATCHPOINT_READ = 2,
	WATCHPOINT_RW = 3,
	WATCHPOINT_WRITE_CHANGE = 4,
};

enum mBreakpointType {
	BREAKPOINT_HARDWARE,
	BREAKPOINT_SOFTWARE
};

enum mDebuggerEntryReason {
	DEBUGGER_ENTER_MANUAL,
	DEBUGGER_ENTER_ATTACHED,
	DEBUGGER_ENTER_BREAKPOINT,
	DEBUGGER_ENTER_WATCHPOINT,
	DEBUGGER_ENTER_ILLEGAL_OP
};

struct mDebuggerEntryInfo {
	uint32_t address;
	union {
		struct {
			uint32_t oldValue;
			uint32_t newValue;
			enum mWatchpointType watchType;
			enum mWatchpointType accessType;
		} wp;

		struct {
			uint32_t opcode;
			enum mBreakpointType breakType;
		} bp;
	} type;
	ssize_t pointId;
};

struct mBreakpoint {
	ssize_t id;
	uint32_t address;
	int segment;
	enum mBreakpointType type;
	struct ParseTree* condition;
};

struct mWatchpoint {
	ssize_t id;
	uint32_t address;
	int segment;
	enum mWatchpointType type;
	struct ParseTree* condition;
};

DECLARE_VECTOR(mBreakpointList, struct mBreakpoint);
DECLARE_VECTOR(mWatchpointList, struct mWatchpoint);

struct mDebugger;
struct ParseTree;
struct mDebuggerPlatform {
	struct mDebugger* p;

	void (*init)(void* cpu, struct mDebuggerPlatform*);
	void (*deinit)(struct mDebuggerPlatform*);
	void (*entered)(struct mDebuggerPlatform*, enum mDebuggerEntryReason, struct mDebuggerEntryInfo*);

	bool (*hasBreakpoints)(struct mDebuggerPlatform*);
	void (*checkBreakpoints)(struct mDebuggerPlatform*);
	bool (*clearBreakpoint)(struct mDebuggerPlatform*, ssize_t id);

	ssize_t (*setBreakpoint)(struct mDebuggerPlatform*, const struct mBreakpoint*);
	void (*listBreakpoints)(struct mDebuggerPlatform*, struct mBreakpointList*);

	ssize_t (*setWatchpoint)(struct mDebuggerPlatform*, const struct mWatchpoint*);
	void (*listWatchpoints)(struct mDebuggerPlatform*, struct mWatchpointList*);

	void (*trace)(struct mDebuggerPlatform*, char* out, size_t* length);

	bool (*getRegister)(struct mDebuggerPlatform*, const char* name, int32_t* value);
	bool (*setRegister)(struct mDebuggerPlatform*, const char* name, int32_t value);
	bool (*lookupIdentifier)(struct mDebuggerPlatform*, const char* name, int32_t* value, int* segment);
};

struct mDebugger {
	struct mCPUComponent d;
	struct mDebuggerPlatform* platform;
	enum mDebuggerState state;
	enum mDebuggerType type;
	struct mCore* core;
	struct mScriptBridge* bridge;

	void (*init)(struct mDebugger*);
	void (*deinit)(struct mDebugger*);

	void (*paused)(struct mDebugger*);
	void (*entered)(struct mDebugger*, enum mDebuggerEntryReason, struct mDebuggerEntryInfo*);
	void (*custom)(struct mDebugger*);
};

struct mDebugger* mDebuggerCreate(enum mDebuggerType type, struct mCore*);
void mDebuggerAttach(struct mDebugger*, struct mCore*);
void mDebuggerRun(struct mDebugger*);
void mDebuggerRunFrame(struct mDebugger*);
void mDebuggerEnter(struct mDebugger*, enum mDebuggerEntryReason, struct mDebuggerEntryInfo*);

bool mDebuggerLookupIdentifier(struct mDebugger* debugger, const char* name, int32_t* value, int* segment);

CXX_GUARD_END

#endif
