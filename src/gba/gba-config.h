#ifndef GBA_CONFIG_H
#define GBA_CONFIG_H

#include "util/common.h"

struct Configuration;

struct GBAOptions {
	char* bios;
	int logLevel;
	int frameskip;
	int rewindBufferCapacity;
	int rewindBufferInterval;
	float fpsTarget;
	size_t audioBuffers;

	int fullscreen;
	int width;
	int height;
};

bool GBAConfigLoad(struct Configuration*);

void GBAConfigMapGeneralOpts(const struct Configuration*, const char* port, struct GBAOptions*);
void GBAConfigMapGraphicsOpts(const struct Configuration*, const char* port, struct GBAOptions*);
void GBAConfigFreeOpts(struct GBAOptions*);

#endif
