/* Copyright (c) 2013-2018 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_MATRIX_H
#define GBA_MATRIX_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct GBAMatrix {
	uint32_t cmd;
	uint32_t paddr;
	uint32_t vaddr;
	uint32_t size;
};

struct GBA;
struct GBAMemory;
void GBAMatrixReset(struct GBA*);
void GBAMatrixWrite(struct GBA*, uint32_t address, uint32_t value);
void GBAMatrixWrite16(struct GBA*, uint32_t address, uint16_t value);

CXX_GUARD_END

#endif
