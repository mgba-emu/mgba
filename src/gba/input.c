/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "input.h"

#include "util/configuration.h"
#include "util/table.h"

#include <inttypes.h>

#define SECTION_NAME_MAX 128
#define KEY_NAME_MAX 32
#define KEY_VALUE_MAX 16
#define AXIS_INFO_MAX 12

struct GBAInputMapImpl {
	int* map;
	uint32_t type;

	struct Table axes;
};

struct GBAAxisSave {
	struct Configuration* config;
	const char* sectionName;
};

struct GBAAxisEnumerate {
	void (*handler)(int axis, const struct GBAAxis* description, void* user);
	void* user;
};

const char* GBAKeyNames[] = {
	"A",
	"B",
	"Select",
	"Start",
	"Right",
	"Left",
	"Up",
	"Down",
	"R",
	"L"
};

static void _makeSectionName(char* sectionName, size_t len, uint32_t type) {
	snprintf(sectionName, len, "input.%c%c%c%c", type >> 24, type >> 16, type >> 8, type);
	sectionName[len - 1] = '\0';
}

static bool _getIntValue(const struct Configuration* config, const char* section, const char* key, int* value) {
	const char* strValue = ConfigurationGetValue(config, section, key);
	if (!strValue) {
		return false;
	}
	char* end;
	long intValue = strtol(strValue, &end, 10);
	if (*end) {
		return false;
	}
	*value = intValue;
	return true;
}

static struct GBAInputMapImpl* _lookupMap(struct GBAInputMap* map, uint32_t type) {
	size_t m;
	struct GBAInputMapImpl* impl = 0;
	for (m = 0; m < map->numMaps; ++m) {
		if (map->maps[m].type == type) {
			impl = &map->maps[m];
			break;
		}
	}
	return impl;
}

static const struct GBAInputMapImpl* _lookupMapConst(const struct GBAInputMap* map, uint32_t type) {
	size_t m;
	const struct GBAInputMapImpl* impl = 0;
	for (m = 0; m < map->numMaps; ++m) {
		if (map->maps[m].type == type) {
			impl = &map->maps[m];
			break;
		}
	}
	return impl;
}

static struct GBAInputMapImpl* _guaranteeMap(struct GBAInputMap* map, uint32_t type) {
	struct GBAInputMapImpl* impl = 0;
	if (map->numMaps == 0) {
		map->maps = malloc(sizeof(*map->maps));
		map->numMaps = 1;
		impl = &map->maps[0];
		impl->type = type;
		impl->map = calloc(GBA_KEY_MAX, sizeof(int));
		TableInit(&impl->axes, 2, free);
	} else {
		impl = _lookupMap(map, type);
	}
	if (!impl) {
		size_t m;
		for (m = 0; m < map->numMaps; ++m) {
			if (!map->maps[m].type) {
				impl = &map->maps[m];
				break;
			}
		}
		if (impl) {
			impl->type = type;
			impl->map = calloc(GBA_KEY_MAX, sizeof(int));
		} else {
			map->maps = realloc(map->maps, sizeof(*map->maps) * map->numMaps * 2);
			for (m = map->numMaps * 2 - 1; m > map->numMaps; --m) {
				map->maps[m].type = 0;
				map->maps[m].map = 0;
			}
			map->numMaps *= 2;
			impl = &map->maps[m];
			impl->type = type;
			impl->map = calloc(GBA_KEY_MAX, sizeof(int));
		}
		TableInit(&impl->axes, 2, free);
	}
	return impl;
}

static void _loadKey(struct GBAInputMap* map, uint32_t type, const char* sectionName, const struct Configuration* config, enum GBAKey key, const char* keyName) {
	char keyKey[KEY_NAME_MAX];
	snprintf(keyKey, KEY_NAME_MAX, "key%s", keyName);
	keyKey[KEY_NAME_MAX - 1] = '\0';

	int value;
	if (!_getIntValue(config, sectionName, keyKey, &value)) {
		return;
	}
	GBAInputBindKey(map, type, value, key);
}

static void _loadAxis(struct GBAInputMap* map, uint32_t type, const char* sectionName, const struct Configuration* config, enum GBAKey direction, const char* axisName) {
	char axisKey[KEY_NAME_MAX];
	snprintf(axisKey, KEY_NAME_MAX, "axis%sValue", axisName);
	axisKey[KEY_NAME_MAX - 1] = '\0';
	int value;
	if (!_getIntValue(config, sectionName, axisKey, &value)) {
		return;
	}

	snprintf(axisKey, KEY_NAME_MAX, "axis%sAxis", axisName);
	axisKey[KEY_NAME_MAX - 1] = '\0';
	int axis;
	const char* strValue = ConfigurationGetValue(config, sectionName, axisKey);
	if (!strValue || !strValue[0]) {
		return;
	}
	char* end;
	axis = strtoul(&strValue[1], &end, 10);
	if (*end) {
		return;
	}

	const struct GBAAxis* description = GBAInputQueryAxis(map, type, axis);
	struct GBAAxis realDescription = { GBA_KEY_NONE, GBA_KEY_NONE, 0, 0 };
	if (description) {
		realDescription = *description;
	}
	if (strValue[0] == '+') {
		realDescription.deadHigh = value;
		realDescription.highDirection = direction;
	} else if (strValue[0] == '-') {
		realDescription.deadLow = value;
		realDescription.lowDirection = direction;
	}
	GBAInputBindAxis(map, type, axis, &realDescription);
}

static void _saveKey(const struct GBAInputMap* map, uint32_t type, const char* sectionName, struct Configuration* config, enum GBAKey key, const char* keyName) {
	char keyKey[KEY_NAME_MAX];
	snprintf(keyKey, KEY_NAME_MAX, "key%s", keyName);
	keyKey[KEY_NAME_MAX - 1] = '\0';

	int value = GBAInputQueryBinding(map, type, key);
	char keyValue[KEY_VALUE_MAX];
	snprintf(keyValue, KEY_VALUE_MAX, "%" PRIi32, value);

	ConfigurationSetValue(config, sectionName, keyKey, keyValue);
}

static void _clearAxis(const char* sectionName, struct Configuration* config, const char* axisName) {
	char axisKey[KEY_NAME_MAX];
	snprintf(axisKey, KEY_NAME_MAX, "axis%sValue", axisName);
	axisKey[KEY_NAME_MAX - 1] = '\0';
	ConfigurationClearValue(config, sectionName, axisKey);

	snprintf(axisKey, KEY_NAME_MAX, "axis%sAxis", axisName);
	axisKey[KEY_NAME_MAX - 1] = '\0';
	ConfigurationClearValue(config, sectionName, axisKey);
}

static void _saveAxis(uint32_t axis, void* dp, void* up) {
	struct GBAAxisSave* user = up;
	const struct GBAAxis* description = dp;

	const char* sectionName = user->sectionName;

	if (description->lowDirection != GBA_KEY_NONE) {
		const char* keyName = GBAKeyNames[description->lowDirection];

		char axisKey[KEY_NAME_MAX];
		snprintf(axisKey, KEY_NAME_MAX, "axis%sValue", keyName);
		axisKey[KEY_NAME_MAX - 1] = '\0';
		ConfigurationSetIntValue(user->config, sectionName, axisKey, description->deadLow);

		snprintf(axisKey, KEY_NAME_MAX, "axis%sAxis", keyName);
		axisKey[KEY_NAME_MAX - 1] = '\0';

		char axisInfo[AXIS_INFO_MAX];
		snprintf(axisInfo, AXIS_INFO_MAX, "-%u", axis);
		axisInfo[AXIS_INFO_MAX - 1] = '\0';
		ConfigurationSetValue(user->config, sectionName, axisKey, axisInfo);
	}
	if (description->highDirection != GBA_KEY_NONE) {
		const char* keyName = GBAKeyNames[description->highDirection];

		char axisKey[KEY_NAME_MAX];
		snprintf(axisKey, KEY_NAME_MAX, "axis%sValue", keyName);
		axisKey[KEY_NAME_MAX - 1] = '\0';
		ConfigurationSetIntValue(user->config, sectionName, axisKey, description->deadHigh);

		snprintf(axisKey, KEY_NAME_MAX, "axis%sAxis", keyName);
		axisKey[KEY_NAME_MAX - 1] = '\0';

		char axisInfo[AXIS_INFO_MAX];
		snprintf(axisInfo, AXIS_INFO_MAX, "+%u", axis);
		axisInfo[AXIS_INFO_MAX - 1] = '\0';
		ConfigurationSetValue(user->config, sectionName, axisKey, axisInfo);
	}
}

void _enumerateAxis(uint32_t axis, void* dp, void* ep) {
	struct GBAAxisEnumerate* enumUser = ep;
	const struct GBAAxis* description = dp;
	enumUser->handler(axis, description, enumUser->user);
}

void _unbindAxis(uint32_t axis, void* dp, void* user) {
	UNUSED(axis);
	enum GBAKey* key = user;
	struct GBAAxis* description = dp;
	if (description->highDirection == *key) {
		description->highDirection = GBA_KEY_NONE;
	}
	if (description->lowDirection == *key) {
		description->lowDirection = GBA_KEY_NONE;
	}
}

static bool _loadAll(struct GBAInputMap* map, uint32_t type, const char* sectionName, const struct Configuration* config) {
	if (!ConfigurationHasSection(config, sectionName)) {
		return false;
	}
	_loadKey(map, type, sectionName, config, GBA_KEY_A, "A");
	_loadKey(map, type, sectionName, config, GBA_KEY_B, "B");
	_loadKey(map, type, sectionName, config, GBA_KEY_L, "L");
	_loadKey(map, type, sectionName, config, GBA_KEY_R, "R");
	_loadKey(map, type, sectionName, config, GBA_KEY_START, "Start");
	_loadKey(map, type, sectionName, config, GBA_KEY_SELECT, "Select");
	_loadKey(map, type, sectionName, config, GBA_KEY_UP, "Up");
	_loadKey(map, type, sectionName, config, GBA_KEY_DOWN, "Down");
	_loadKey(map, type, sectionName, config, GBA_KEY_LEFT, "Left");
	_loadKey(map, type, sectionName, config, GBA_KEY_RIGHT, "Right");

	_loadAxis(map, type, sectionName, config, GBA_KEY_A, "A");
	_loadAxis(map, type, sectionName, config, GBA_KEY_B, "B");
	_loadAxis(map, type, sectionName, config, GBA_KEY_L, "L");
	_loadAxis(map, type, sectionName, config, GBA_KEY_R, "R");
	_loadAxis(map, type, sectionName, config, GBA_KEY_START, "Start");
	_loadAxis(map, type, sectionName, config, GBA_KEY_SELECT, "Select");
	_loadAxis(map, type, sectionName, config, GBA_KEY_UP, "Up");
	_loadAxis(map, type, sectionName, config, GBA_KEY_DOWN, "Down");
	_loadAxis(map, type, sectionName, config, GBA_KEY_LEFT, "Left");
	_loadAxis(map, type, sectionName, config, GBA_KEY_RIGHT, "Right");
	return true;
}

static void _saveAll(const struct GBAInputMap* map, uint32_t type, const char* sectionName, struct Configuration* config) {
	_saveKey(map, type, sectionName, config, GBA_KEY_A, "A");
	_saveKey(map, type, sectionName, config, GBA_KEY_B, "B");
	_saveKey(map, type, sectionName, config, GBA_KEY_L, "L");
	_saveKey(map, type, sectionName, config, GBA_KEY_R, "R");
	_saveKey(map, type, sectionName, config, GBA_KEY_START, "Start");
	_saveKey(map, type, sectionName, config, GBA_KEY_SELECT, "Select");
	_saveKey(map, type, sectionName, config, GBA_KEY_UP, "Up");
	_saveKey(map, type, sectionName, config, GBA_KEY_DOWN, "Down");
	_saveKey(map, type, sectionName, config, GBA_KEY_LEFT, "Left");
	_saveKey(map, type, sectionName, config, GBA_KEY_RIGHT, "Right");

	_clearAxis(sectionName, config, "A");
	_clearAxis(sectionName, config, "B");
	_clearAxis(sectionName, config, "L");
	_clearAxis(sectionName, config, "R");
	_clearAxis(sectionName, config, "Start");
	_clearAxis(sectionName, config, "Select");
	_clearAxis(sectionName, config, "Up");
	_clearAxis(sectionName, config, "Down");
	_clearAxis(sectionName, config, "Left");
	_clearAxis(sectionName, config, "Right");

	const struct GBAInputMapImpl* impl = _lookupMapConst(map, type);
	if (!impl) {
		return;
	}
	struct GBAAxisSave save = {
		config,
		sectionName
	};
	TableEnumerate(&impl->axes, _saveAxis, &save);
}

void GBAInputMapInit(struct GBAInputMap* map) {
	map->maps = 0;
	map->numMaps = 0;
}

void GBAInputMapDeinit(struct GBAInputMap* map) {
	size_t m;
	for (m = 0; m < map->numMaps; ++m) {
		if (map->maps[m].type) {
			free(map->maps[m].map);
			TableDeinit(&map->maps[m].axes);
		}
	}
	free(map->maps);
	map->maps = 0;
	map->numMaps = 0;
}

enum GBAKey GBAInputMapKey(const struct GBAInputMap* map, uint32_t type, int key) {
	size_t m;
	const struct GBAInputMapImpl* impl = _lookupMapConst(map, type);
	if (!impl || !impl->map) {
		return GBA_KEY_NONE;
	}

	for (m = 0; m < GBA_KEY_MAX; ++m) {
		if (impl->map[m] == key) {
			return m;
		}
	}
	return GBA_KEY_NONE;
}

int GBAInputMapKeyBits(const struct GBAInputMap* map, uint32_t type, uint32_t bits, unsigned offset) {
	int keys = 0;
	for (; bits; bits >>= 1, ++offset) {
		if (bits & 1) {
			enum GBAKey key = GBAInputMapKey(map, type, offset);
			if (key == GBA_KEY_NONE) {
				continue;
			}
			keys |= 1 << key;
		}
	}
	return keys;
}

void GBAInputBindKey(struct GBAInputMap* map, uint32_t type, int key, enum GBAKey input) {
	struct GBAInputMapImpl* impl = _guaranteeMap(map, type);
	GBAInputUnbindKey(map, type, input);
	impl->map[input] = key;
}

void GBAInputUnbindKey(struct GBAInputMap* map, uint32_t type, enum GBAKey input) {
	struct GBAInputMapImpl* impl = _lookupMap(map, type);
	if (input < 0 || input >= GBA_KEY_MAX) {
		return;
	}
	if (impl) {
		impl->map[input] = GBA_NO_MAPPING;
	}
}

int GBAInputQueryBinding(const struct GBAInputMap* map, uint32_t type, enum GBAKey input) {
	if (input >= GBA_KEY_MAX) {
		return 0;
	}

	const struct GBAInputMapImpl* impl = _lookupMapConst(map, type);
	if (!impl || !impl->map) {
		return 0;
	}

	return impl->map[input];
}

enum GBAKey GBAInputMapAxis(const struct GBAInputMap* map, uint32_t type, int axis, int value) {
	const struct GBAInputMapImpl* impl = _lookupMapConst(map, type);
	if (!impl) {
		return GBA_KEY_NONE;
	}
	struct GBAAxis* description = TableLookup(&impl->axes, axis);
	if (!description) {
		return GBA_KEY_NONE;
	}
	int state = 0;
	if (value < description->deadLow) {
		state = -1;
	} else if (value > description->deadHigh) {
		state = 1;
	}
	if (state > 0) {
		return description->highDirection;
	}
	if (state < 0) {
		return description->lowDirection;
	}
	return GBA_KEY_NONE;
}

int GBAInputClearAxis(const struct GBAInputMap* map, uint32_t type, int axis, int keys) {
	const struct GBAInputMapImpl* impl = _lookupMapConst(map, type);
	if (!impl) {
		return keys;
	}
	struct GBAAxis* description = TableLookup(&impl->axes, axis);
	if (!description) {
		return keys;
	}
	return keys &= ~((1 << description->highDirection) | (1 << description->lowDirection));
}

void GBAInputBindAxis(struct GBAInputMap* map, uint32_t type, int axis, const struct GBAAxis* description) {
	struct GBAInputMapImpl* impl = _guaranteeMap(map, type);
	TableEnumerate(&impl->axes, _unbindAxis, &description->highDirection);
	TableEnumerate(&impl->axes, _unbindAxis, &description->lowDirection);
	struct GBAAxis* dup = malloc(sizeof(struct GBAAxis));
	*dup = *description;
	TableInsert(&impl->axes, axis, dup);
}

void GBAInputUnbindAxis(struct GBAInputMap* map, uint32_t type, int axis) {
	struct GBAInputMapImpl* impl = _lookupMap(map, type);
	if (impl) {
		TableRemove(&impl->axes, axis);
	}
}

void GBAInputUnbindAllAxes(struct GBAInputMap* map, uint32_t type) {
	struct GBAInputMapImpl* impl = _lookupMap(map, type);
	if (impl) {
		TableClear(&impl->axes);
	}
}

const struct GBAAxis* GBAInputQueryAxis(const struct GBAInputMap* map, uint32_t type, int axis) {
	const struct GBAInputMapImpl* impl = _lookupMapConst(map, type);
	if (!impl) {
		return 0;
	}
	return TableLookup(&impl->axes, axis);
}

void GBAInputEnumerateAxes(const struct GBAInputMap* map, uint32_t type, void (handler(int axis, const struct GBAAxis* description, void* user)), void* user) {
	const struct GBAInputMapImpl* impl = _lookupMapConst(map, type);
	if (!impl) {
		return;
	}
	struct GBAAxisEnumerate enumUser = {
		handler,
		user
	};
	TableEnumerate(&impl->axes, _enumerateAxis, &enumUser);
}

void GBAInputMapLoad(struct GBAInputMap* map, uint32_t type, const struct Configuration* config) {
	char sectionName[SECTION_NAME_MAX];
	_makeSectionName(sectionName, SECTION_NAME_MAX, type);
	_loadAll(map, type, sectionName, config);
}

void GBAInputMapSave(const struct GBAInputMap* map, uint32_t type, struct Configuration* config) {
	char sectionName[SECTION_NAME_MAX];
	_makeSectionName(sectionName, SECTION_NAME_MAX, type);
	_saveAll(map, type, sectionName, config);
}

bool GBAInputProfileLoad(struct GBAInputMap* map, uint32_t type, const struct Configuration* config, const char* profile) {
	char sectionName[SECTION_NAME_MAX];
	snprintf(sectionName, SECTION_NAME_MAX, "input-profile.%s", profile);
	sectionName[SECTION_NAME_MAX - 1] = '\0';
	return _loadAll(map, type, sectionName, config);
}

void GBAInputProfileSave(const struct GBAInputMap* map, uint32_t type, struct Configuration* config, const char* profile) {
	char sectionName[SECTION_NAME_MAX];
	snprintf(sectionName, SECTION_NAME_MAX, "input-profile.%s", profile);
	sectionName[SECTION_NAME_MAX - 1] = '\0';
	_saveAll(map, type, sectionName, config);
}

const char* GBAInputGetPreferredDevice(const struct Configuration* config, uint32_t type, int playerId) {
	char sectionName[SECTION_NAME_MAX];
	_makeSectionName(sectionName, SECTION_NAME_MAX, type);

	char deviceId[KEY_NAME_MAX];
	snprintf(deviceId, sizeof(deviceId), "device%i", playerId);
	return ConfigurationGetValue(config, sectionName, deviceId);
}

void GBAInputSetPreferredDevice(struct Configuration* config, uint32_t type, int playerId, const char* deviceName) {
	char sectionName[SECTION_NAME_MAX];
	_makeSectionName(sectionName, SECTION_NAME_MAX, type);

	char deviceId[KEY_NAME_MAX];
	snprintf(deviceId, sizeof(deviceId), "device%i", playerId);
	return ConfigurationSetValue(config, sectionName, deviceId, deviceName);
}

const char* GBAInputGetCustomValue(const struct Configuration* config, uint32_t type, const char* key, const char* profile) {
	char sectionName[SECTION_NAME_MAX];
	if (profile) {
		snprintf(sectionName, SECTION_NAME_MAX, "input-profile.%s", profile);
		const char* value = ConfigurationGetValue(config, sectionName, key);
		if (value) {
			return value;
		}
	}
	_makeSectionName(sectionName, SECTION_NAME_MAX, type);
	return ConfigurationGetValue(config, sectionName, key);
}

void GBAInputSetCustomValue(struct Configuration* config, uint32_t type, const char* key, const char* value, const char* profile) {
	char sectionName[SECTION_NAME_MAX];
	if (profile) {
		snprintf(sectionName, SECTION_NAME_MAX, "input-profile.%s", profile);
		ConfigurationSetValue(config, sectionName, key, value);
	}
	_makeSectionName(sectionName, SECTION_NAME_MAX, type);
	ConfigurationSetValue(config, sectionName, key, value);
}
