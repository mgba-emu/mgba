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

struct GBAAxis {
	enum GBAKey highDirection;
	enum GBAKey lowDirection;
	int32_t deadHigh;
	int32_t deadLow;
};

#define GBA_NO_MAPPING -1

extern const char* GBAKeyNames[];

void GBAInputMapInit(struct GBAInputMap*);
void GBAInputMapDeinit(struct GBAInputMap*);

enum GBAKey GBAInputMapKey(const struct GBAInputMap*, uint32_t type, int key);
void GBAInputBindKey(struct GBAInputMap*, uint32_t type, int key, enum GBAKey input);
void GBAInputUnbindKey(struct GBAInputMap*, uint32_t type, int key);
int GBAInputQueryBinding(const struct GBAInputMap*, uint32_t type, enum GBAKey input);

enum GBAKey GBAInputMapAxis(const struct GBAInputMap*, uint32_t type, int axis, int value);
int GBAInputClearAxis(const struct GBAInputMap*, uint32_t type, int axis, int keys);
void GBAInputBindAxis(struct GBAInputMap*, uint32_t type, int axis, const struct GBAAxis* description);
void GBAInputUnbindAxis(struct GBAInputMap*, uint32_t type, int axis);
void GBAInputUnbindAllAxes(struct GBAInputMap*, uint32_t type);
const struct GBAAxis* GBAInputQueryAxis(const struct GBAInputMap*, uint32_t type, int axis);
void GBAInputEnumerateAxes(const struct GBAInputMap*, uint32_t type, void (handler(int axis, const struct GBAAxis* description, void* user)), void* user);

void GBAInputMapLoad(struct GBAInputMap*, uint32_t type, const struct Configuration*);
void GBAInputMapSave(const struct GBAInputMap*, uint32_t type, struct Configuration*);

#endif
