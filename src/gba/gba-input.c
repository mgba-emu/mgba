#include "gba-input.h"

#include "util/configuration.h"

#define SECTION_NAME_MAX 128
#define KEY_NAME_MAX 32

struct GBAInputMapImpl {
	int* map;
	uint32_t type;
};

static void _loadKey(struct GBAInputMap* map, uint32_t type, const struct Configuration* config, enum GBAKey key, const char* keyName) {
	char sectionName[SECTION_NAME_MAX];
	snprintf(sectionName, SECTION_NAME_MAX, "input.%c%c%c%c", type >> 24, type >> 16, type >> 8, type);
	sectionName[SECTION_NAME_MAX - 1] = '\0';

	char keyKey[KEY_NAME_MAX];
	snprintf(keyKey, KEY_NAME_MAX, "key%s", keyName);

	const char* value = ConfigurationGetValue(config, sectionName, keyKey);
	if (!value) {
		return;
	}
	char* end;
	long intValue = strtol(value, &end, 10);
	if (*end) {
		return;
	}
	GBAInputBindKey(map, type, intValue, key);
}

void GBAInputMapInit(struct GBAInputMap* map) {
	map->maps = 0;
	map->numMaps = 0;
}

void GBAInputMapDeinit(struct GBAInputMap* map) {
	size_t m;
	for (m = 0; m < map->numMaps; ++m) {
		free(map->maps[m].map);
	}
	free(map->maps);
	map->maps = 0;
	map->numMaps = 0;
}

enum GBAKey GBAInputMapKey(const struct GBAInputMap* map, uint32_t type, int key) {
	size_t m;
	const struct GBAInputMapImpl* impl = 0;
	for (m = 0; m < map->numMaps; ++m) {
		if (map->maps[m].type == type) {
			impl = &map->maps[m];
			break;
		}
	}
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
	struct GBAInputMapImpl* impl = 0;
	if (map->numMaps == 0) {
		map->maps = malloc(sizeof(*map->maps));
		map->numMaps = 1;
		impl = &map->maps[0];
		impl->type = type;
		impl->map = calloc(GBA_KEY_MAX, sizeof(enum GBAKey));
	} else {
		size_t m;
		for (m = 0; m < map->numMaps; ++m) {
			if (map->maps[m].type == type) {
				impl = &map->maps[m];
				break;
			}
		}
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
	impl->map[input] = key;
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
