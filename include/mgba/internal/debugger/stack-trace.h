/* Copyright (c) 2013-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef STACK_TRACE_H
#define STACK_TRACE_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/cpu.h>
#include <mgba/core/log.h>
#include <mgba-util/vector.h>

struct mDebuggerSymbols;

enum mStackTraceMode {
	STACK_TRACE_DISABLED = 0,
	STACK_TRACE_ENABLED = 1,
	STACK_TRACE_BREAK_ON_RETURN = 2,
	STACK_TRACE_BREAK_ON_CALL = 4,
	STACK_TRACE_BREAK_ON_BOTH = STACK_TRACE_BREAK_ON_RETURN | STACK_TRACE_BREAK_ON_CALL
};

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

void mStackTraceInit(struct mStackTrace* stack, size_t registersSize);
void mStackTraceDeinit(struct mStackTrace* stack);

void mStackTraceClear(struct mStackTrace* stack);
size_t mStackTraceGetDepth(struct mStackTrace* stack);
struct mStackFrame* mStackTracePush(struct mStackTrace* stack, uint32_t pc, uint32_t destAddress, uint32_t sp, void* regs);
struct mStackFrame* mStackTracePushSegmented(struct mStackTrace* stack, int pcSegment, uint32_t pc, int destSegment, uint32_t destAddress, int spSegment, uint32_t sp, void* regs);
struct mStackFrame* mStackTraceGetFrame(struct mStackTrace* stack, uint32_t frame);
void mStackTraceFormatFrame(struct mStackTrace* stack, struct mDebuggerSymbols* st, uint32_t frame, char* out, size_t* length);
void mStackTracePop(struct mStackTrace* stack);

CXX_GUARD_END

#endif
