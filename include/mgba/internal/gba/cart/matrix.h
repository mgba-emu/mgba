/* Copyright (c) 2013-2018 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_MATRIX_H
#define GBA_MATRIX_H

#include <mgba-util/common.h>

CXX_GUARD_START

#define GBA_MATRIX_MAPPINGS_MAX 16

struct GBAMatrix {
	uint32_t cmd;
	uint32_t paddr;
	uint32_t vaddr;
	uint32_t size;

	uint32_t mappings[GBA_MATRIX_MAPPINGS_MAX];
};

struct GBA;
void GBAMatrixReset(struct GBA*);
void GBAMatrixWrite(struct GBA*, uint32_t address, uint32_t value);
void GBAMatrixWrite16(struct GBA*, uint32_t address, uint16_t value);

struct GBASerializedState;
void GBAMatrixSerialize(const struct GBA* memory, struct GBASerializedState* state);
void GBAMatrixDeserialize(struct GBA* memory, const struct GBASerializedState* state);

CXX_GUARD_END

#endif
