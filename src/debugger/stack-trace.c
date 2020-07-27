/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/debugger/stack-trace.h>

#include <mgba/core/core.h>

DEFINE_VECTOR(mStackFrames, struct mStackFrame);

void mStackTraceInit(struct mStackTrace* stack, size_t registersSize) {
	mStackFramesInit(&stack->stack, 0);
}

void mStackTraceDeinit(struct mStackTrace* stack) {
	mStackTraceClear(stack);
	mStackFramesDeinit(&stack->stack);
}

void mStackTraceClear(struct mStackTrace* stack) {
	ssize_t i = mStackTraceGetDepth(stack) - 1;
	while (i >= 0) {
		free(mStackTraceGetFrame(stack, i)->regs);
		--i;
	}
}

size_t mStackTraceGetDepth(struct mStackTrace* stack) {
	return mStackFramesSize(&stack->stack);
}

struct mStackFrame* mStackTracePush(struct mStackTrace* stack, uint32_t instruction, uint32_t pc, uint32_t destAddress, uint32_t sp, void* regs) {
	struct mStackFrame* frame = mStackFramesAppend(&stack->stack);
	frame->instruction = instruction;
	frame->callAddress = pc;
	frame->entryAddress = destAddress;
	frame->frameBaseAddress = sp;
	frame->regs = malloc(stack->registersSize);
	frame->finished = false;
	frame->breakWhenFinished = false;
	memcpy(frame->regs, regs, stack->registersSize);
	return frame;
}

struct mStackFrame* mStackTraceGetFrame(struct mStackTrace* stack, size_t frame) {
	size_t depth = mStackTraceGetDepth(stack);
	if (frame >= depth) {
		return NULL;
	}
	return mStackFramesGetPointer(&stack->stack, depth - frame - 1);
}

void mStackTracePop(struct mStackTrace* stack) {
	size_t depth = mStackTraceGetDepth(stack);
	if (depth == 0) {
		return;
	}
	struct mStackFrame* frame = mStackFramesGetPointer(&stack->stack, depth - 1);
	free(frame->regs);
	mStackFramesResize(&stack->stack, depth - 1);
}
