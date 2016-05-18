/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "dynarec.h"

#include "arm/arm.h"
#include "util/memory.h"

void ARMDynarecInit(struct ARMCore* cpu) {
	BumpAllocatorInit(&cpu->dynarec.traceAlloc, sizeof(struct ARMDynarecTrace));
	TableInit(&cpu->dynarec.armTraces, 0x2000, 0);
	TableInit(&cpu->dynarec.thumbTraces, 0x2000, 0);
	cpu->dynarec.buffer = executableMemoryMap(0x100000);
	cpu->dynarec.temporaryMemory = anonymousMemoryMap(0x2000);
}

void ARMDynarecDeinit(struct ARMCore* cpu) {
	BumpAllocatorDeinit(&cpu->dynarec.traceAlloc);
	TableDeinit(&cpu->dynarec.armTraces);
	TableDeinit(&cpu->dynarec.thumbTraces);
	mappedMemoryFree(cpu->dynarec.buffer, 0x100000);
	mappedMemoryFree(cpu->dynarec.temporaryMemory, 0x2000);
}

static struct ARMDynarecTrace* ARMDynarecFindTrace(struct ARMCore* cpu, uint32_t address, enum ExecutionMode mode) {
	struct ARMDynarecTrace* trace;
	if (mode == MODE_ARM) {
		trace = TableLookup(&cpu->dynarec.armTraces, address >> 2);
		if (!trace) {
			trace = BumpAllocatorAlloc(&cpu->dynarec.traceAlloc);
			TableInsert(&cpu->dynarec.armTraces, address >> 2, trace);
			trace->entry = NULL;
			trace->start = address;
			trace->mode = mode;
		}
	} else {
		trace = TableLookup(&cpu->dynarec.thumbTraces, address >> 1);
		if (!trace) {
			trace = BumpAllocatorAlloc(&cpu->dynarec.traceAlloc);
			TableInsert(&cpu->dynarec.thumbTraces, address >> 1, trace);
			trace->entry = NULL;
			trace->start = address;
			trace->mode = mode;
		}
	}
	return trace;
}

void ARMDynarecCountTrace(struct ARMCore* cpu, uint32_t address, enum ExecutionMode mode) {
	struct ARMDynarecTrace* trace = ARMDynarecFindTrace(cpu, address, mode);
	if (!trace->entry) {
		ARMDynarecRecompileTrace(cpu, trace);
	}
	if (trace->entry) {
		if (!cpu->dynarec.inDynarec) {
			cpu->nextEvent = cpu->cycles;
		}
		cpu->dynarec.currentEntry = trace->entry;
	}
}
