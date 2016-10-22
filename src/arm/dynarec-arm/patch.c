/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "arm/dynarec-arm/patch.h"
#include "arm/dynarec-arm/emitter.h"
#include "arm/dynarec.h"
#include "arm/macros.h"
#include "util/vector.h"

struct ARMDynarecPatchPoint {
	code_t* location;
	enum ARMDynarecPatchPointType type;
};

DEFINE_VECTOR(ARMDynarecPatchPointList, struct ARMDynarecPatchPoint);

void ARMDynarecTraceInit(struct ARMDynarecTrace* trace) {
	ARMDynarecPatchPointListInit(&trace->patchPoints, 0);
}

void ARMDynarecTraceDeinit(struct ARMDynarecTrace* trace) {
	ARMDynarecPatchPointListDeinit(&trace->patchPoints);
}

static bool patchPatchPoint(struct ARMCore* cpu, struct ARMDynarecTrace* targetTrace, struct ARMDynarecPatchPoint* patchPoint, code_t** write_back) {
	code_t* code = patchPoint->location;

	uint32_t prefetch[2];
	LOAD_16(prefetch[0], (targetTrace->start + 0 * WORD_SIZE_THUMB) & cpu->memory.activeMask, cpu->memory.activeRegion);
	LOAD_16(prefetch[1], (targetTrace->start + 1 * WORD_SIZE_THUMB) & cpu->memory.activeMask, cpu->memory.activeRegion);

	switch (patchPoint->type) {
	default:
		abort();
	}

	__clear_cache(patchPoint->location, code);
	if (write_back) {
		*write_back = code;
	}
	return targetTrace->entry;
}

bool ARMDynarecAddPatchPoint(struct ARMCore* cpu, struct ARMDynarecContext* ctx, enum ARMDynarecPatchPointType type, uint32_t targetAddress) {
	struct ARMDynarecTrace* targetTrace = ARMDynarecFindTrace(cpu, targetAddress, MODE_THUMB);
	struct ARMDynarecPatchPoint* patchPoint = ARMDynarecPatchPointListAppend(&targetTrace->patchPoints);
	patchPoint->location = ctx->code;
	patchPoint->type = type;
	return patchPatchPoint(cpu, targetTrace, patchPoint, &ctx->code);
}

void ARMDynarecPerformPatching(struct ARMCore* cpu, struct ARMDynarecTrace* trace) {
	for (unsigned index = 0; index < ARMDynarecPatchPointListSize(&trace->patchPoints); ++index) {
		patchPatchPoint(cpu, trace, ARMDynarecPatchPointListGetPointer(&trace->patchPoints, index), 0);
	}
}
