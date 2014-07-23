#ifndef GBA_INPUT_H
#define GBA_INPUT_H

#include "gba.h"

struct GBAInputMap {
	struct GBAInputMapImpl* maps;
	size_t numMaps;
};

void GBAInputMapInit(struct GBAInputMap*);
void GBAInputMapDeinit(struct GBAInputMap*);

enum GBAKey GBAInputMapKey(struct GBAInputMap*, uint32_t type, int key);
void GBAInputBindKey(struct GBAInputMap*, uint32_t type, int key, enum GBAKey input);

#endif
