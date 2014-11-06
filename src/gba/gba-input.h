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

void GBAInputMapLoad(struct GBAInputMap*, uint32_t type, const struct Configuration*);

#endif
