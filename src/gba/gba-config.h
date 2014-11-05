#ifndef GBA_CONFIG_H
#define GBA_CONFIG_H

#include "util/common.h"

#include "util/configuration.h"

struct GBAConfig {
	struct Configuration configTable;
	struct Configuration defaultsTable;
	char* port;
};

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

	bool videoSync;
	bool audioSync;
};

void GBAConfigInit(struct GBAConfig*, const char* port);
void GBAConfigDeinit(struct GBAConfig*);

bool GBAConfigLoad(struct GBAConfig*);
bool GBAConfigSave(const struct GBAConfig*);

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

void GBAConfigFreeOpts(struct GBAOptions* opts);

#endif
