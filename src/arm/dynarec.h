/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef ARM_DYNAREC_H
#define ARM_DYNAREC_H

#include "util/common.h"
#include "util/vector.h"

#include "arm/arm.h"

struct ARMCore;
enum ExecutionMode;
struct ARMDynarecPatchPoint;

DECLARE_VECTOR(ARMDynarecPatchPointList, struct ARMDynarecPatchPoint);

struct ARMDynarecTrace {
	enum ExecutionMode mode;
	uint32_t start;
	void* entry;
	void* entryPlus4;
	struct ARMDynarecPatchPointList patchPoints;
};

void ARMDynarecInit(struct ARMCore* cpu);
void ARMDynarecDeinit(struct ARMCore* cpu);
void ARMDynarecInvalidateCache(struct ARMCore* cpu);

void ARMDynarecTraceInit(struct ARMDynarecTrace* trace);
void ARMDynarecTraceDeinit(struct ARMDynarecTrace* trace);
struct ARMDynarecTrace* ARMDynarecFindTrace(struct ARMCore* cpu, uint32_t address, enum ExecutionMode mode);
void ARMDynarecCountTrace(struct ARMCore* cpu, uint32_t address, enum ExecutionMode mode);
void ARMDynarecRecompileTrace(struct ARMCore* cpu, struct ARMDynarecTrace* trace);
void ARMDynarecExecuteTrace(struct ARMCore* cpu, struct ARMDynarecTrace* execution_token);

#endif
