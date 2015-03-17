/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_CONFIG_H
#define GBA_CONFIG_H

#include "util/common.h"

#include "gba/gba.h"

#include "util/configuration.h"

struct GBAConfig {
	struct Configuration configTable;
	struct Configuration defaultsTable;
	char* port;
};

struct GBAOptions {
	char* bios;
	bool skipBios;
	bool useBios;
	int logLevel;
	int frameskip;
	bool rewindEnable;
	int rewindBufferCapacity;
	int rewindBufferInterval;
	float fpsTarget;
	size_t audioBuffers;

	int fullscreen;
	int width;
	int height;
	bool lockAspectRatio;
	bool resampleVideo;

	bool videoSync;
	bool audioSync;

	enum GBAIdleLoopOptimization idleOptimization;
};

void GBAConfigInit(struct GBAConfig*, const char* port);
void GBAConfigDeinit(struct GBAConfig*);

bool GBAConfigLoad(struct GBAConfig*);
bool GBAConfigSave(const struct GBAConfig*);

void GBAConfigDirectory(char* out, size_t outLength);

const char* GBAConfigGetValue(const struct GBAConfig*, const char* key);

void GBAConfigSetValue(struct GBAConfig*, const char* key, const char* value);
void GBAConfigSetIntValue(struct GBAConfig*, const char* key, int value);
void GBAConfigSetUIntValue(struct GBAConfig*, const char* key, unsigned value);
void GBAConfigSetFloatValue(struct GBAConfig*, const char* key, float value);

void GBAConfigSetDefaultValue(struct GBAConfig*, const char* key, const char* value);
void GBAConfigSetDefaultIntValue(struct GBAConfig*, const char* key, int value);
void GBAConfigSetDefaultUIntValue(struct GBAConfig*, const char* key, unsigned value);
void GBAConfigSetDefaultFloatValue(struct GBAConfig*, const char* key, float value);

void GBAConfigMap(const struct GBAConfig* config, struct GBAOptions* opts);
void GBAConfigLoadDefaults(struct GBAConfig* config, const struct GBAOptions* opts);

struct Configuration* GBAConfigGetInput(struct GBAConfig*);
struct Configuration* GBAConfigGetOverrides(struct GBAConfig*);

void GBAConfigFreeOpts(struct GBAOptions* opts);

#endif
