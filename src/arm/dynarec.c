/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <assert.h>

#include "dynarec.h"

#include "arm/arm.h"
#include "util/memory.h"

void ARMDynarecEmitPrelude(struct ARMCore* cpu);

void ARMDynarecInit(struct ARMCore* cpu) {
	BumpAllocatorInit(&cpu->dynarec.traceAlloc, sizeof(struct ARMDynarecTrace));
	TableInit(&cpu->dynarec.armTraces, 0x2000, &ARMDynarecTraceDeinit);
	TableInit(&cpu->dynarec.thumbTraces, 0x2000, &ARMDynarecTraceDeinit);
	cpu->dynarec.bufferStart = cpu->dynarec.buffer = executableMemoryMap(0x200000);
	cpu->dynarec.temporaryMemory = anonymousMemoryMap(0x2000);
	ARMDynarecEmitPrelude(cpu);
}

void ARMDynarecDeinit(struct ARMCore* cpu) {
	BumpAllocatorDeinit(&cpu->dynarec.traceAlloc);
	TableDeinit(&cpu->dynarec.armTraces);
	TableDeinit(&cpu->dynarec.thumbTraces);
	mappedMemoryFree(cpu->dynarec.bufferStart, 0x200000);
	mappedMemoryFree(cpu->dynarec.temporaryMemory, 0x2000);
}

void ARMDynarecInvalidateCache(struct ARMCore* cpu) {
	if (cpu->dynarec.inDynarec) {
		cpu->nextEvent = cpu->cycles;
	}
	TableClear(&cpu->dynarec.armTraces);
	TableClear(&cpu->dynarec.thumbTraces);
	BumpAllocatorDeinit(&cpu->dynarec.traceAlloc);
	BumpAllocatorInit(&cpu->dynarec.traceAlloc, sizeof(struct ARMDynarecTrace));
	cpu->dynarec.buffer = cpu->dynarec.bufferStart;
	cpu->dynarec.currentTrace = NULL;
	ARMDynarecEmitPrelude(cpu);
}

struct ARMDynarecTrace* ARMDynarecFindTrace(struct ARMCore* cpu, uint32_t address, enum ExecutionMode mode) {
	struct ARMDynarecTrace* trace;
	if (mode == MODE_ARM) {
		trace = TableLookup(&cpu->dynarec.armTraces, address >> 2);
		if (!trace) {
			trace = BumpAllocatorAlloc(&cpu->dynarec.traceAlloc);
			ARMDynarecTraceInit(trace);
			TableInsert(&cpu->dynarec.armTraces, address >> 2, trace);
			trace->entry = NULL;
			trace->start = address;
			trace->mode = mode;
		}
	} else {
		trace = TableLookup(&cpu->dynarec.thumbTraces, address >> 1);
		if (!trace) {
			trace = BumpAllocatorAlloc(&cpu->dynarec.traceAlloc);
			ARMDynarecTraceInit(trace);
			TableInsert(&cpu->dynarec.thumbTraces, address >> 1, trace);
			trace->entry = NULL;
			trace->start = address;
			trace->mode = mode;
		}
	}
	return trace;
}

void ARMDynarecCountTrace(struct ARMCore* cpu, uint32_t address, enum ExecutionMode mode) {
	if (mode != MODE_THUMB)
		return;

	struct ARMDynarecTrace* tracePrediction = cpu->dynarec.tracePrediction;
	if (tracePrediction && tracePrediction->start == address) {
		if (!tracePrediction->entry) {
			ARMDynarecRecompileTrace(cpu, tracePrediction);
		}
		cpu->dynarec.currentTrace = tracePrediction;
		return;
	}

	struct ARMDynarecTrace* trace = ARMDynarecFindTrace(cpu, address, mode);
	if (!trace->entry) {
		ARMDynarecRecompileTrace(cpu, trace);
	}
	if (trace->entry) {
		if (!cpu->dynarec.inDynarec) {
			cpu->nextEvent = cpu->cycles;
		}
		cpu->dynarec.currentTrace = trace;
	} else {
		cpu->dynarec.currentTrace = NULL;
	}
}
