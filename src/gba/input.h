/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_INPUT_H
#define GBA_INPUT_H

#include "gba/gba.h"

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
void GBAInputUnbindKey(struct GBAInputMap*, uint32_t type, enum GBAKey input);
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

void GBAInputProfileLoad(struct GBAInputMap*, uint32_t type, const struct Configuration*, const char* profile);
void GBAInputProfileSave(const struct GBAInputMap*, uint32_t type, struct Configuration*, const char* profile);

const char* GBAInputGetPreferredDevice(const struct Configuration*, uint32_t type, int playerId);
void GBAInputSetPreferredDevice(struct Configuration*, uint32_t type, int playerId, const char* deviceName);

const char* GBAInputGetCustomValue(const struct Configuration* config, uint32_t type, const char* key,
                                   const char* profile);
void GBAInputSetCustomValue(struct Configuration* config, uint32_t type, const char* key, const char* value,
                            const char* profile);

#endif
