/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef ARM_DYNAREC_PATCH_H
#define ARM_DYNAREC_PATCH_H

#include "util/common.h"

struct ARMCore;
struct ARMDynarecContext;
struct ARMDynarecTrace;

enum ARMDynarecPatchPointType {
    PATCH_POINT_B
};

bool ARMDynarecAddPatchPoint(struct ARMCore* cpu, struct ARMDynarecContext* ctx, enum ARMDynarecPatchPointType type, uint32_t targetAddress);
void ARMDynarecPerformPatching(struct ARMCore* cpu, struct ARMDynarecTrace* targetTrace);

#endif
