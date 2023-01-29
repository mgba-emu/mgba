/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/input.h>

#include <mgba-util/configuration.h>
#include <mgba-util/table.h>
#include <mgba-util/vector.h>

#include <inttypes.h>

#define SECTION_NAME_MAX 128
#define KEY_NAME_MAX 32
#define KEY_VALUE_MAX 16
#define AXIS_INFO_MAX 12

DECLARE_VECTOR(mInputHatList, struct mInputHatBindings);
DEFINE_VECTOR(mInputHatList, struct mInputHatBindings);

struct mInputMapImpl {
	int* map;
	uint32_t type;

	struct Table axes;
	struct mInputHatList hats;
};

struct mInputAxisSave {
	struct Configuration* config;
	const char* sectionName;
	const struct mInputPlatformInfo* info;
};

struct mInputAxisEnumerate {
	void (*handler)(int axis, const struct mInputAxis* description, void* user);
	void* user;
};

struct mInputHatEnumerate {
	void (*handler)(int axis, const struct mInputHatBindings* bindings, void* user);
	void* user;
};

static void _makeSectionName(const char* platform, char* sectionName, size_t len, uint32_t type) {
	snprintf(sectionName, len, "%s.input.%c%c%c%c", platform, type >> 24, type >> 16, type >> 8, type);
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

static struct mInputMapImpl* _lookupMap(struct mInputMap* map, uint32_t type) {
	size_t m;
	struct mInputMapImpl* impl = 0;
	for (m = 0; m < map->numMaps; ++m) {
		if (map->maps[m].type == type) {
			impl = &map->maps[m];
			break;
		}
	}
	return impl;
}

static const struct mInputMapImpl* _lookupMapConst(const struct mInputMap* map, uint32_t type) {
	size_t m;
	const struct mInputMapImpl* impl = 0;
	for (m = 0; m < map->numMaps; ++m) {
		if (map->maps[m].type == type) {
			impl = &map->maps[m];
			break;
		}
	}
	return impl;
}

static struct mInputMapImpl* _guaranteeMap(struct mInputMap* map, uint32_t type) {
	struct mInputMapImpl* impl = 0;
	if (map->numMaps == 0) {
		map->maps = malloc(sizeof(*map->maps));
		map->numMaps = 1;
		impl = &map->maps[0];
		impl->type = type;
		impl->map = calloc(map->info->nKeys, sizeof(int));
		size_t i;
		for (i = 0; i < map->info->nKeys; ++i) {
			impl->map[i] = -1;
		}
		TableInit(&impl->axes, 2, free);
		mInputHatListInit(&impl->hats, 1);
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
			impl->map = calloc(map->info->nKeys, sizeof(int));
			size_t i;
			for (i = 0; i < map->info->nKeys; ++i) {
				impl->map[i] = -1;
			}
		} else {
			map->maps = realloc(map->maps, sizeof(*map->maps) * map->numMaps * 2);
			for (m = map->numMaps * 2 - 1; m > map->numMaps; --m) {
				map->maps[m].type = 0;
				map->maps[m].map = 0;
			}
			map->numMaps *= 2;
			impl = &map->maps[m];
			impl->type = type;
			impl->map = calloc(map->info->nKeys, sizeof(int));
			size_t i;
			for (i = 0; i < map->info->nKeys; ++i) {
				impl->map[i] = -1;
			}
		}
		TableInit(&impl->axes, 2, free);
		mInputHatListInit(&impl->hats, 1);
	}
	return impl;
}

static void _loadKey(struct mInputMap* map, uint32_t type, const char* sectionName, const struct Configuration* config, int key, const char* keyName) {
	char keyKey[KEY_NAME_MAX];
	snprintf(keyKey, KEY_NAME_MAX, "key%s", keyName);
	keyKey[KEY_NAME_MAX - 1] = '\0';

	int value;
	if (!_getIntValue(config, sectionName, keyKey, &value)) {
		return;
	}
	mInputBindKey(map, type, value, key);
}

static void _loadAxis(struct mInputMap* map, uint32_t type, const char* sectionName, const struct Configuration* config, int direction, const char* axisName) {
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

	const struct mInputAxis* description = mInputQueryAxis(map, type, axis);
	struct mInputAxis realDescription = { -1, -1, 0, 0 };
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
	mInputBindAxis(map, type, axis, &realDescription);
}

static bool _loadHat(struct mInputMap* map, uint32_t type, const char* sectionName, const struct Configuration* config, int hatId) {
	char hatKey[KEY_NAME_MAX];

	struct mInputHatBindings hatBindings = { -1, -1, -1, -1 };

	bool found = false;
	snprintf(hatKey, KEY_NAME_MAX, "hat%iUp", hatId);
	found = _getIntValue(config, sectionName, hatKey, &hatBindings.up) || found;
	snprintf(hatKey, KEY_NAME_MAX, "hat%iRight", hatId);
	found = _getIntValue(config, sectionName, hatKey, &hatBindings.right) || found;
	snprintf(hatKey, KEY_NAME_MAX, "hat%iDown", hatId);
	found = _getIntValue(config, sectionName, hatKey, &hatBindings.down) || found;
	snprintf(hatKey, KEY_NAME_MAX, "hat%iLeft", hatId);
	found = _getIntValue(config, sectionName, hatKey, &hatBindings.left) || found;

	if (!found) {
		return false;
	}
	mInputBindHat(map, type, hatId, &hatBindings);
	return true;
}

static void _saveKey(const struct mInputMap* map, uint32_t type, const char* sectionName, struct Configuration* config, int key, const char* keyName) {
	char keyKey[KEY_NAME_MAX];
	snprintf(keyKey, KEY_NAME_MAX, "key%s", keyName);
	keyKey[KEY_NAME_MAX - 1] = '\0';

	int value = mInputQueryBinding(map, type, key);
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
	struct mInputAxisSave* user = up;
	const struct mInputAxis* description = dp;

	const char* sectionName = user->sectionName;

	if (description->lowDirection != -1) {
		const char* keyName = user->info->keyId[description->lowDirection];

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
	if (description->highDirection != -1) {
		const char* keyName = user->info->keyId[description->highDirection];

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

static void _saveHat(const char* sectionName, struct Configuration* config, int hatId, const struct mInputHatBindings* hat) {
	char hatKey[KEY_NAME_MAX];
	char hatValue[KEY_VALUE_MAX];

	snprintf(hatKey, KEY_NAME_MAX, "hat%iUp", hatId);
	snprintf(hatValue, KEY_VALUE_MAX, "%i", hat->up);
	ConfigurationSetValue(config, sectionName, hatKey, hatValue);
	snprintf(hatKey, KEY_NAME_MAX, "hat%iRight", hatId);
	snprintf(hatValue, KEY_VALUE_MAX, "%i", hat->right);
	ConfigurationSetValue(config, sectionName, hatKey, hatValue);
	snprintf(hatKey, KEY_NAME_MAX, "hat%iDown", hatId);
	snprintf(hatValue, KEY_VALUE_MAX, "%i", hat->down);
	ConfigurationSetValue(config, sectionName, hatKey, hatValue);
	snprintf(hatKey, KEY_NAME_MAX, "hat%iLeft", hatId);
	snprintf(hatValue, KEY_VALUE_MAX, "%i", hat->left);
	ConfigurationSetValue(config, sectionName, hatKey, hatValue);
}

void _enumerateAxis(uint32_t axis, void* dp, void* ep) {
	struct mInputAxisEnumerate* enumUser = ep;
	const struct mInputAxis* description = dp;
	enumUser->handler(axis, description, enumUser->user);
}

void _unbindAxis(uint32_t axis, void* dp, void* user) {
	UNUSED(axis);
	int* key = user;
	struct mInputAxis* description = dp;
	if (description->highDirection == *key) {
		description->highDirection = -1;
	}
	if (description->lowDirection == *key) {
		description->lowDirection = -1;
	}
}

void _enumerateHat(uint32_t axis, void* dp, void* ep) {
	struct mInputHatEnumerate* enumUser = ep;
	const struct mInputHatBindings* description = dp;
	enumUser->handler(axis, description, enumUser->user);
}

static bool _loadAll(struct mInputMap* map, uint32_t type, const char* sectionName, const struct Configuration* config) {
	if (!ConfigurationHasSection(config, sectionName)) {
		return false;
	}
	size_t i;
	for (i = 0; i < map->info->nKeys; ++i) {
		_loadKey(map, type, sectionName, config, i, map->info->keyId[i]);
		_loadAxis(map, type, sectionName, config, i, map->info->keyId[i]);
	}
	i = 0;
	while (_loadHat(map, type, sectionName, config, i)) {
		++i;
	}
	return true;
}

static void _saveAll(const struct mInputMap* map, uint32_t type, const char* sectionName, struct Configuration* config) {
	size_t i;
	for (i = 0; i < map->info->nKeys; ++i) {
		if (!map->info->keyId[i]) {
			continue;
		}
		_saveKey(map, type, sectionName, config, i, map->info->keyId[i]);
		_clearAxis(sectionName, config, map->info->keyId[i]);
	}

	const struct mInputMapImpl* impl = _lookupMapConst(map, type);
	if (!impl) {
		return;
	}
	struct mInputAxisSave save = {
		config,
		sectionName,
		map->info
	};
	TableEnumerate(&impl->axes, _saveAxis, &save);

	for (i = 0; i < mInputHatListSize(&impl->hats); ++i) {
		const struct mInputHatBindings* hat = mInputHatListGetConstPointer(&impl->hats, i);
		_saveHat(sectionName, config, i, hat);
	}
}

void mInputMapInit(struct mInputMap* map, const struct mInputPlatformInfo* info) {
	map->maps = 0;
	map->numMaps = 0;
	map->info = info;
}

void mInputMapDeinit(struct mInputMap* map) {
	size_t m;
	for (m = 0; m < map->numMaps; ++m) {
		if (map->maps[m].type) {
			free(map->maps[m].map);
			TableDeinit(&map->maps[m].axes);
			mInputHatListDeinit(&map->maps[m].hats);
		}
	}
	free(map->maps);
	map->maps = 0;
	map->numMaps = 0;
}

int mInputMapKey(const struct mInputMap* map, uint32_t type, int key) {
	size_t m;
	const struct mInputMapImpl* impl = _lookupMapConst(map, type);
	if (!impl || !impl->map) {
		return -1;
	}

	for (m = 0; m < map->info->nKeys; ++m) {
		if (impl->map[m] == key) {
			return m;
		}
	}
	return -1;
}

int mInputMapKeyBits(const struct mInputMap* map, uint32_t type, uint32_t bits, unsigned offset) {
	int keys = 0;
	for (; bits; bits >>= 1, ++offset) {
		if (bits & 1) {
			int key = mInputMapKey(map, type, offset);
			if (key == -1) {
				continue;
			}
			keys |= 1 << key;
		}
	}
	return keys;
}

void mInputBindKey(struct mInputMap* map, uint32_t type, int key, int input) {
	struct mInputMapImpl* impl = _guaranteeMap(map, type);
	if (input < 0 || (size_t) input >= map->info->nKeys) {
		return;
	}
	mInputUnbindKey(map, type, input);
	impl->map[input] = key;
}

void mInputUnbindKey(struct mInputMap* map, uint32_t type, int input) {
	struct mInputMapImpl* impl = _lookupMap(map, type);
	if (input < 0 || (size_t) input >= map->info->nKeys) {
		return;
	}
	if (impl) {
		impl->map[input] = -1;
	}
}

void mInputUnbindAllKeys(struct mInputMap* map, uint32_t type) {
	struct mInputMapImpl* impl = _lookupMap(map, type);
	if (impl) {
		size_t i;
		for (i = 0; i < map->info->nKeys; ++i) {
			impl->map[i] = -1;
		}
	}
}

int mInputQueryBinding(const struct mInputMap* map, uint32_t type, int input) {
	if (input < 0 || (size_t) input >= map->info->nKeys) {
		return -1;
	}

	const struct mInputMapImpl* impl = _lookupMapConst(map, type);
	if (!impl || !impl->map) {
		return -1;
	}

	return impl->map[input];
}

int mInputMapAxis(const struct mInputMap* map, uint32_t type, int axis, int value) {
	const struct mInputMapImpl* impl = _lookupMapConst(map, type);
	if (!impl) {
		return -1;
	}
	struct mInputAxis* description = TableLookup(&impl->axes, axis);
	if (!description) {
		return -1;
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
	return -1;
}

int mInputClearAxis(const struct mInputMap* map, uint32_t type, int axis, int keys) {
	const struct mInputMapImpl* impl = _lookupMapConst(map, type);
	if (!impl) {
		return keys;
	}
	struct mInputAxis* description = TableLookup(&impl->axes, axis);
	if (!description) {
		return keys;
	}
	return keys &= ~((1 << description->highDirection) | (1 << description->lowDirection));
}

void mInputBindAxis(struct mInputMap* map, uint32_t type, int axis, const struct mInputAxis* description) {
	struct mInputMapImpl* impl = _guaranteeMap(map, type);
	struct mInputAxis d2 = *description;
	TableEnumerate(&impl->axes, _unbindAxis, &d2.highDirection);
	TableEnumerate(&impl->axes, _unbindAxis, &d2.lowDirection);
	struct mInputAxis* dup = malloc(sizeof(struct mInputAxis));
	*dup = *description;
	TableInsert(&impl->axes, axis, dup);
}

void mInputUnbindAxis(struct mInputMap* map, uint32_t type, int axis) {
	struct mInputMapImpl* impl = _lookupMap(map, type);
	if (impl) {
		TableRemove(&impl->axes, axis);
	}
}

void mInputUnbindAllAxes(struct mInputMap* map, uint32_t type) {
	struct mInputMapImpl* impl = _lookupMap(map, type);
	if (impl) {
		TableClear(&impl->axes);
	}
}

const struct mInputAxis* mInputQueryAxis(const struct mInputMap* map, uint32_t type, int axis) {
	const struct mInputMapImpl* impl = _lookupMapConst(map, type);
	if (!impl) {
		return 0;
	}
	return TableLookup(&impl->axes, axis);
}

void mInputEnumerateAxes(const struct mInputMap* map, uint32_t type, void (handler(int axis, const struct mInputAxis* description, void* user)), void* user) {
	const struct mInputMapImpl* impl = _lookupMapConst(map, type);
	if (!impl) {
		return;
	}
	struct mInputAxisEnumerate enumUser = {
		handler,
		user
	};
	TableEnumerate(&impl->axes, _enumerateAxis, &enumUser);
}

int mInputMapHat(const struct mInputMap* map, uint32_t type, int id, int direction) {
	const struct mInputMapImpl* impl = _lookupMapConst(map, type);
	if (!impl) {
		return 0;
	}
	if (id >= (ssize_t) mInputHatListSize(&impl->hats)) {
		return 0;
	}
	const struct mInputHatBindings* description = mInputHatListGetConstPointer(&impl->hats, id);
	int mapping = 0;
	if (direction & M_INPUT_HAT_UP && description->up >= 0) {
		mapping |= 1 << description->up;
	}
	if (direction & M_INPUT_HAT_RIGHT && description->right >= 0) {
		mapping |= 1 << description->right;
	}
	if (direction & M_INPUT_HAT_DOWN && description->down >= 0) {
		mapping |= 1 << description->down;
	}
	if (direction & M_INPUT_HAT_LEFT && description->left >= 0) {
		mapping |= 1 << description->left;
	}
	return mapping;
}

void mInputBindHat(struct mInputMap* map, uint32_t type, int id, const struct mInputHatBindings* bindings) {
	struct mInputMapImpl* impl = _guaranteeMap(map, type);
	while (id >= (ssize_t) mInputHatListSize(&impl->hats)) {
		*mInputHatListAppend(&impl->hats) = (struct mInputHatBindings) { -1, -1, -1, -1 };
	}
	*mInputHatListGetPointer(&impl->hats, id) = *bindings;
}

bool mInputQueryHat(const struct mInputMap* map, uint32_t type, int id, struct mInputHatBindings* bindings) {
	const struct mInputMapImpl* impl = _lookupMapConst(map, type);
	if (!impl) {
		return false;
	}
	if (id >= (ssize_t) mInputHatListSize(&impl->hats)) {
		return false;
	}
	*bindings = *mInputHatListGetConstPointer(&impl->hats, id);
	return true;
}

void mInputUnbindHat(struct mInputMap* map, uint32_t type, int id) {
	struct mInputMapImpl* impl = _lookupMap(map, type);
	if (!impl) {
		return;
	}
	if (id >= (ssize_t) mInputHatListSize(&impl->hats)) {
		return;
	}
	struct mInputHatBindings* description = mInputHatListGetPointer(&impl->hats, id);
	memset(description, -1, sizeof(*description));
}

void mInputUnbindAllHats(struct mInputMap* map, uint32_t type) {
	struct mInputMapImpl* impl = _lookupMap(map, type);
	if (!impl) {
		return;
	}

	size_t id;
	for (id = 0; id < mInputHatListSize(&impl->hats); ++id) {
		struct mInputHatBindings* description = mInputHatListGetPointer(&impl->hats, id);
		memset(description, -1, sizeof(*description));
	}
}

void mInputEnumerateHats(const struct mInputMap* map, uint32_t type, void (handler(int hat, const struct mInputHatBindings* bindings, void* user)), void* user) {
	const struct mInputMapImpl* impl = _lookupMapConst(map, type);
	if (!impl) {
		return;
	}
	struct mInputHatEnumerate enumUser = {
		handler,
		user
	};
	TableEnumerate(&impl->axes, _enumerateHat, &enumUser);
}

bool mInputMapLoad(struct mInputMap* map, uint32_t type, const struct Configuration* config) {
	char sectionName[SECTION_NAME_MAX];
	_makeSectionName(map->info->platformName, sectionName, SECTION_NAME_MAX, type);
	return _loadAll(map, type, sectionName, config);
}

void mInputMapSave(const struct mInputMap* map, uint32_t type, struct Configuration* config) {
	char sectionName[SECTION_NAME_MAX];
	_makeSectionName(map->info->platformName, sectionName, SECTION_NAME_MAX, type);
	_saveAll(map, type, sectionName, config);
}

bool mInputProfileLoad(struct mInputMap* map, uint32_t type, const struct Configuration* config, const char* profile) {
	char sectionName[SECTION_NAME_MAX];
	snprintf(sectionName, SECTION_NAME_MAX, "%s.input-profile.%s", map->info->platformName, profile);
	sectionName[SECTION_NAME_MAX - 1] = '\0';
	return _loadAll(map, type, sectionName, config);
}

void mInputProfileSave(const struct mInputMap* map, uint32_t type, struct Configuration* config, const char* profile) {
	char sectionName[SECTION_NAME_MAX];
	snprintf(sectionName, SECTION_NAME_MAX, "%s.input-profile.%s", map->info->platformName, profile);
	sectionName[SECTION_NAME_MAX - 1] = '\0';
	_saveAll(map, type, sectionName, config);
}

const char* mInputGetPreferredDevice(const struct Configuration* config, const char* platformName, uint32_t type, int playerId) {
	char sectionName[SECTION_NAME_MAX];
	_makeSectionName(platformName, sectionName, SECTION_NAME_MAX, type);

	char deviceId[KEY_NAME_MAX];
	snprintf(deviceId, sizeof(deviceId), "device%i", playerId);
	return ConfigurationGetValue(config, sectionName, deviceId);
}

void mInputSetPreferredDevice(struct Configuration* config, const char* platformName, uint32_t type, int playerId, const char* deviceName) {
	char sectionName[SECTION_NAME_MAX];
	_makeSectionName(platformName, sectionName, SECTION_NAME_MAX, type);

	char deviceId[KEY_NAME_MAX];
	snprintf(deviceId, sizeof(deviceId), "device%i", playerId);
	return ConfigurationSetValue(config, sectionName, deviceId, deviceName);
}

const char* mInputGetCustomValue(const struct Configuration* config, const char* platformName, uint32_t type, const char* key, const char* profile) {
	char sectionName[SECTION_NAME_MAX];
	if (profile) {
		snprintf(sectionName, SECTION_NAME_MAX, "%s.input-profile.%s", platformName, profile);
		const char* value = ConfigurationGetValue(config, sectionName, key);
		if (value) {
			return value;
		}
	}
	_makeSectionName(platformName, sectionName, SECTION_NAME_MAX, type);
	return ConfigurationGetValue(config, sectionName, key);
}

void mInputSetCustomValue(struct Configuration* config, const char* platformName, uint32_t type, const char* key, const char* value, const char* profile) {
	char sectionName[SECTION_NAME_MAX];
	if (profile) {
		snprintf(sectionName, SECTION_NAME_MAX, "%s.input-profile.%s", platformName, profile);
		ConfigurationSetValue(config, sectionName, key, value);
	}
	_makeSectionName(platformName, sectionName, SECTION_NAME_MAX, type);
	ConfigurationSetValue(config, sectionName, key, value);
}
