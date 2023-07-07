/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SFO_H
#define SFO_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba-util/table.h>

void SfoInit(struct Table* sfo);

static inline void SfoDeinit(struct Table* sfo) {
	HashTableDeinit(sfo);
}

struct VFile;
bool SfoWrite(struct Table* sfo, struct VFile* vf);

bool SfoAddU32Value(struct Table* sfo, const char* name, uint32_t value);
bool SfoAddStrValue(struct Table* sfo, const char* name, const char* value);
bool SfoSetTitle(struct Table* sfo, const char* title);

CXX_GUARD_END

#endif
