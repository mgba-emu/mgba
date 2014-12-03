/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_INPUT_H
#define GBA_INPUT_H

#include "gba.h"

struct Configuration;

struct GBAInputMap {
	struct GBAInputMapImpl* maps;
	size_t numMaps;
};

void GBAInputMapInit(struct GBAInputMap*);
void GBAInputMapDeinit(struct GBAInputMap*);

enum GBAKey GBAInputMapKey(const struct GBAInputMap*, uint32_t type, int key);
void GBAInputBindKey(struct GBAInputMap*, uint32_t type, int key, enum GBAKey input);
int GBAInputQueryBinding(const struct GBAInputMap*, uint32_t type, enum GBAKey input);

void GBAInputMapLoad(struct GBAInputMap*, uint32_t type, const struct Configuration*);
void GBAInputMapSave(const struct GBAInputMap*, uint32_t type, struct Configuration*);

#endif
