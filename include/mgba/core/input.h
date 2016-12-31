/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_INPUT_H
#define M_INPUT_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct Configuration;

struct mInputPlatformInfo {
	const char* platformName;
	const char** keyId;
	size_t nKeys;
};

struct mInputMap {
	struct mInputMapImpl* maps;
	size_t numMaps;
	const struct mInputPlatformInfo* info;
};

struct mInputAxis {
	int highDirection;
	int lowDirection;
	int32_t deadHigh;
	int32_t deadLow;
};

void mInputMapInit(struct mInputMap*, const struct mInputPlatformInfo* info);
void mInputMapDeinit(struct mInputMap*);

int mInputMapKey(const struct mInputMap*, uint32_t type, int key);
int mInputMapKeyBits(const struct mInputMap* map, uint32_t type, uint32_t bits, unsigned offset);
void mInputBindKey(struct mInputMap*, uint32_t type, int key, int input);
int mInputQueryBinding(const struct mInputMap*, uint32_t type, int input);
void mInputUnbindKey(struct mInputMap*, uint32_t type, int input);

int mInputMapAxis(const struct mInputMap*, uint32_t type, int axis, int value);
int mInputClearAxis(const struct mInputMap*, uint32_t type, int axis, int keys);
void mInputBindAxis(struct mInputMap*, uint32_t type, int axis, const struct mInputAxis* description);
void mInputUnbindAxis(struct mInputMap*, uint32_t type, int axis);
void mInputUnbindAllAxes(struct mInputMap*, uint32_t type);
const struct mInputAxis* mInputQueryAxis(const struct mInputMap*, uint32_t type, int axis);
void mInputEnumerateAxes(const struct mInputMap*, uint32_t type, void (handler(int axis, const struct mInputAxis* description, void* user)), void* user);

void mInputMapLoad(struct mInputMap*, uint32_t type, const struct Configuration*);
void mInputMapSave(const struct mInputMap*, uint32_t type, struct Configuration*);

bool mInputProfileLoad(struct mInputMap*, uint32_t type, const struct Configuration*, const char* profile);
void mInputProfileSave(const struct mInputMap*, uint32_t type, struct Configuration*, const char* profile);

const char* mInputGetPreferredDevice(const struct Configuration*, const char* platformName, uint32_t type, int playerId);
void mInputSetPreferredDevice(struct Configuration*, const char* platformName, uint32_t type, int playerId, const char* deviceName);

const char* mInputGetCustomValue(const struct Configuration* config, const char* platformName, uint32_t type, const char* key,
                                 const char* profile);
void mInputSetCustomValue(struct Configuration* config, const char* platformName, uint32_t type, const char* key, const char* value,
                          const char* profile);

CXX_GUARD_END

#endif
