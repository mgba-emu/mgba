#include "gba-input.h"

struct GBAInputMapImpl {
	int* map;
	uint32_t type;
};

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

enum GBAKey GBAInputMapKey(struct GBAInputMap* map, uint32_t type, int key) {
	size_t m;
	struct GBAInputMapImpl* impl = 0;
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
