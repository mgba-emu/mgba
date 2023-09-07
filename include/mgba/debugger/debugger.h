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
#include <mgba-util/table.h>
#include <mgba-util/vector.h>

mLOG_DECLARE_CATEGORY(DEBUGGER);

extern const uint32_t DEBUGGER_ID;

enum mDebuggerType {
	DEBUGGER_NONE = 0,
	DEBUGGER_CUSTOM,
	DEBUGGER_CLI,
	DEBUGGER_GDB,
	DEBUGGER_MAX
};

enum mDebuggerState {
	DEBUGGER_CREATED = 0,
	DEBUGGER_PAUSED,
	DEBUGGER_RUNNING,
	DEBUGGER_CALLBACK,
	DEBUGGER_SHUTDOWN
};

enum mWatchpointType {
	WATCHPOINT_WRITE = 1,
	WATCHPOINT_READ = 2,
	WATCHPOINT_RW = 3,
	WATCHPOINT_CHANGE = 4,
	WATCHPOINT_WRITE_CHANGE = 5,
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
	DEBUGGER_ENTER_ILLEGAL_OP,
	DEBUGGER_ENTER_STACK
};

enum mStackTraceMode {
	STACK_TRACE_DISABLED = 0,
	STACK_TRACE_ENABLED = 1,
	STACK_TRACE_BREAK_ON_RETURN = 2,
	STACK_TRACE_BREAK_ON_CALL = 4,
	STACK_TRACE_BREAK_ON_BOTH = STACK_TRACE_BREAK_ON_RETURN | STACK_TRACE_BREAK_ON_CALL
};

struct mDebuggerModule;
struct mDebuggerEntryInfo {
	uint32_t address;
	int segment;
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

		struct {
			enum mStackTraceMode traceType;
		} st;
	} type;
	ssize_t pointId;
	struct mDebuggerModule* target;
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
	int segment;
	uint32_t minAddress;
	uint32_t maxAddress;
	enum mWatchpointType type;
	struct ParseTree* condition;
};

DECLARE_VECTOR(mBreakpointList, struct mBreakpoint);
DECLARE_VECTOR(mWatchpointList, struct mWatchpoint);
DECLARE_VECTOR(mDebuggerModuleList, struct mDebuggerModule*);

struct mStackFrame {
	int callSegment;
	uint32_t callAddress;
	int entrySegment;
	uint32_t entryAddress;
	int frameBaseSegment;
	uint32_t frameBaseAddress;
	void* regs;
	bool finished;
	bool breakWhenFinished;
	bool interrupt;
};

DECLARE_VECTOR(mStackFrames, struct mStackFrame);

struct mStackTrace {
	struct mStackFrames stack;
	size_t registersSize;

	void (*formatRegisters)(struct mStackFrame* frame, char* out, size_t* length);
};

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

	ssize_t (*setBreakpoint)(struct mDebuggerPlatform*, struct mDebuggerModule*, const struct mBreakpoint*);
	void (*listBreakpoints)(struct mDebuggerPlatform*, struct mDebuggerModule*, struct mBreakpointList*);

	ssize_t (*setWatchpoint)(struct mDebuggerPlatform*, struct mDebuggerModule*, const struct mWatchpoint*);
	void (*listWatchpoints)(struct mDebuggerPlatform*, struct mDebuggerModule*, struct mWatchpointList*);

	void (*trace)(struct mDebuggerPlatform*, char* out, size_t* length);

	bool (*lookupIdentifier)(struct mDebuggerPlatform*, const char* name, int32_t* value, int* segment);

	enum mStackTraceMode (*getStackTraceMode)(struct mDebuggerPlatform*);
	void (*setStackTraceMode)(struct mDebuggerPlatform*, enum mStackTraceMode mode);
	bool (*updateStackTrace)(struct mDebuggerPlatform* d);
};

struct mDebugger {
	struct mCPUComponent d;
	struct mDebuggerPlatform* platform;
	enum mDebuggerState state;
	struct mCore* core;
	struct mScriptBridge* bridge;
	struct mStackTrace stackTrace;

	struct mDebuggerModuleList modules;
	struct Table pointOwner;
};

struct mDebuggerModule {
	struct mDebugger* p;
	enum mDebuggerType type;
	bool isPaused;
	bool needsCallback;

	void (*init)(struct mDebuggerModule*);
	void (*deinit)(struct mDebuggerModule*);

	void (*paused)(struct mDebuggerModule*, int32_t timeoutMs);
	void (*update)(struct mDebuggerModule*);
	void (*entered)(struct mDebuggerModule*, enum mDebuggerEntryReason, struct mDebuggerEntryInfo*);
	void (*custom)(struct mDebuggerModule*);

	void (*interrupt)(struct mDebuggerModule*);
};

void mDebuggerInit(struct mDebugger*);
void mDebuggerDeinit(struct mDebugger*);

void mDebuggerAttach(struct mDebugger*, struct mCore*);
void mDebuggerAttachModule(struct mDebugger*, struct mDebuggerModule*);
void mDebuggerDetachModule(struct mDebugger*, struct mDebuggerModule*);
void mDebuggerRunTimeout(struct mDebugger* debugger, int32_t timeoutMs);
void mDebuggerRun(struct mDebugger*);
void mDebuggerRunFrame(struct mDebugger*);
void mDebuggerEnter(struct mDebugger*, enum mDebuggerEntryReason, struct mDebuggerEntryInfo*);

void mDebuggerInterrupt(struct mDebugger*);
void mDebuggerUpdatePaused(struct mDebugger*);
void mDebuggerShutdown(struct mDebugger*);
void mDebuggerUpdate(struct mDebugger*);

bool mDebuggerIsShutdown(const struct mDebugger*);

struct mDebuggerModule* mDebuggerCreateModule(enum mDebuggerType type, struct mCore*);
void mDebuggerModuleSetNeedsCallback(struct mDebuggerModule*);

bool mDebuggerLookupIdentifier(struct mDebugger* debugger, const char* name, int32_t* value, int* segment);

CXX_GUARD_END

#endif
