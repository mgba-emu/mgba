/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef ARM_DECODER_INLINES_H
#define ARM_DECODER_INLINES_H

#include "decoder.h"

#include "arm.h"

#include <stdio.h>
#include <string.h>

#define LOAD_CYCLES    \
	info->iCycles = 1; \
	info->nDataCycles = 1;

#define STORE_CYCLES              \
	info->sInstructionCycles = 0; \
	info->nInstructionCycles = 1; \
	info->nDataCycles = 1;

static inline bool ARMInstructionIsBranch(enum ARMMnemonic mnemonic) {
	switch (mnemonic) {
		case ARM_MN_B:
		case ARM_MN_BL:
		case ARM_MN_BX:
			// TODO: case: ARM_MN_BLX:
			return true;
		default:
			return false;
	}
}

#endif
