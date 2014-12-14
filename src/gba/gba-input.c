/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba-input.h"

#include "util/configuration.h"
#include "util/table.h"

#include <inttypes.h>

#define SECTION_NAME_MAX 128
#define KEY_NAME_MAX 32
#define KEY_VALUE_MAX 16

struct GBAInputMapImpl {
	int* map;
	uint32_t type;

	struct Table axes;
};

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
		impl->map = calloc(GBA_KEY_MAX, sizeof(enum GBAKey));
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
			impl->map = calloc(GBA_KEY_MAX, sizeof(enum GBAKey));
		} else {
			map->maps = realloc(map->maps, sizeof(*map->maps) * map->numMaps * 2);
			for (m = map->numMaps * 2 - 1; m > map->numMaps; --m) {
				map->maps[m].type = 0;
				map->maps[m].map = 0;
			}
			map->numMaps *= 2;
			impl = &map->maps[m];
			impl->type = type;
			impl->map = calloc(GBA_KEY_MAX, sizeof(enum GBAKey));
		}
	}
	TableInit(&impl->axes, 2, free);
	return impl;
}

static void _loadKey(struct GBAInputMap* map, uint32_t type, const struct Configuration* config, enum GBAKey key, const char* keyName) {
	char sectionName[SECTION_NAME_MAX];
	snprintf(sectionName, SECTION_NAME_MAX, "input.%c%c%c%c", type >> 24, type >> 16, type >> 8, type);
	sectionName[SECTION_NAME_MAX - 1] = '\0';

	char keyKey[KEY_NAME_MAX];
	snprintf(keyKey, KEY_NAME_MAX, "key%s", keyName);
	keyKey[KEY_NAME_MAX - 1] = '\0';

	int value;
	if (!_getIntValue(config, sectionName, keyKey, &value)) {
		return;
	}
	GBAInputBindKey(map, type, value, key);
}

static void _loadAxis(struct GBAInputMap* map, uint32_t type, const struct Configuration* config, enum GBAKey direction, const char* axisName) {
	char sectionName[SECTION_NAME_MAX];
	snprintf(sectionName, SECTION_NAME_MAX, "input.%c%c%c%c", type >> 24, type >> 16, type >> 8, type);
	sectionName[SECTION_NAME_MAX - 1] = '\0';

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
	if (!_getIntValue(config, sectionName, axisKey, &axis)) {
		return;
	}

	const struct GBAAxis* description = GBAInputQueryAxis(map, type, axis);
	struct GBAAxis realDescription;
	if (!description) {
		realDescription = (struct GBAAxis) { direction, GBA_KEY_NONE, value, 0 };
	} else {
		realDescription = *description;
		if (value >= realDescription.lowDirection) {
			realDescription.deadHigh = value;
			realDescription.highDirection = direction;
		} else if (value <= realDescription.highDirection) {
			realDescription.deadLow = value;
			realDescription.lowDirection = direction;
		}
	}
	GBAInputBindAxis(map, type, axis, &realDescription);
}

static void _saveKey(const struct GBAInputMap* map, uint32_t type, struct Configuration* config, enum GBAKey key, const char* keyName) {
	char sectionName[SECTION_NAME_MAX];
	snprintf(sectionName, SECTION_NAME_MAX, "input.%c%c%c%c", type >> 24, type >> 16, type >> 8, type);
	sectionName[SECTION_NAME_MAX - 1] = '\0';

	char keyKey[KEY_NAME_MAX];
	snprintf(keyKey, KEY_NAME_MAX, "key%s", keyName);
	keyKey[KEY_NAME_MAX - 1] = '\0';

	int value = GBAInputQueryBinding(map, type, key);
	char keyValue[KEY_VALUE_MAX];
	snprintf(keyValue, KEY_VALUE_MAX, "%" PRIi32, value);

	ConfigurationSetValue(config, sectionName, keyKey, keyValue);
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

void GBAInputBindKey(struct GBAInputMap* map, uint32_t type, int key, enum GBAKey input) {
	struct GBAInputMapImpl* impl = _guaranteeMap(map, type);
	impl->map[input] = key;
}

void GBAInputUnbindKey(struct GBAInputMap* map, uint32_t type, enum GBAKey input) {
	struct GBAInputMapImpl* impl = _lookupMap(map, type);
	if (impl) {
		impl->map[input] = -1;
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

void GBAInputMapLoad(struct GBAInputMap* map, uint32_t type, const struct Configuration* config) {
	_loadKey(map, type, config, GBA_KEY_A, "A");
	_loadKey(map, type, config, GBA_KEY_B, "B");
	_loadKey(map, type, config, GBA_KEY_L, "L");
	_loadKey(map, type, config, GBA_KEY_R, "R");
	_loadKey(map, type, config, GBA_KEY_START, "Start");
	_loadKey(map, type, config, GBA_KEY_SELECT, "Select");
	_loadKey(map, type, config, GBA_KEY_UP, "Up");
	_loadKey(map, type, config, GBA_KEY_DOWN, "Down");
	_loadKey(map, type, config, GBA_KEY_LEFT, "Left");
	_loadKey(map, type, config, GBA_KEY_RIGHT, "Right");
}

void GBAInputMapSave(const struct GBAInputMap* map, uint32_t type, struct Configuration* config) {
	_saveKey(map, type, config, GBA_KEY_A, "A");
	_saveKey(map, type, config, GBA_KEY_B, "B");
	_saveKey(map, type, config, GBA_KEY_L, "L");
	_saveKey(map, type, config, GBA_KEY_R, "R");
	_saveKey(map, type, config, GBA_KEY_START, "Start");
	_saveKey(map, type, config, GBA_KEY_SELECT, "Select");
	_saveKey(map, type, config, GBA_KEY_UP, "Up");
	_saveKey(map, type, config, GBA_KEY_DOWN, "Down");
	_saveKey(map, type, config, GBA_KEY_LEFT, "Left");
	_saveKey(map, type, config, GBA_KEY_RIGHT, "Right");
}
