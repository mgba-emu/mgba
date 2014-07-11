#ifndef ARM_DECODER_INLINES_H
#define ARM_DECODER_INLINES_H

#include "decoder.h"

#include "arm.h"

#include <stdio.h>
#include <string.h>

#define LOAD_CYCLES \
	info->iCycles = 1; \
	info->nDataCycles = 1;

#define STORE_CYCLES \
	info->sInstructionCycles = 0; \
	info->nInstructionCycles = 1; \
	info->nDataCycles = 1;

#endif
