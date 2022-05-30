/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_CORE_CONFIG_H
#define M_CORE_CONFIG_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba-util/configuration.h>

struct mCoreConfig {
	struct Configuration configTable;
	struct Configuration defaultsTable;
	struct Configuration overridesTable;
	char* port;
};

enum mCoreConfigLevel {
	mCONFIG_LEVEL_DEFAULT = 0,
	mCONFIG_LEVEL_CUSTOM,
	mCONFIG_LEVEL_OVERRIDE,
};

struct mCoreOptions {
	char* bios;
	bool skipBios;
	bool useBios;
	int logLevel;
	int frameskip;
	bool rewindEnable;
	int rewindBufferCapacity;
	float fpsTarget;
	size_t audioBuffers;
	unsigned sampleRate;

	int fullscreen;
	int width;
	int height;
	bool lockAspectRatio;
	bool lockIntegerScaling;
	bool interframeBlending;
	bool resampleVideo;
	bool suspendScreensaver;
	char* shader;

	char* savegamePath;
	char* savestatePath;
	char* screenshotPath;
	char* patchPath;
	char* cheatsPath;

	int volume;
	bool mute;

	bool videoSync;
	bool audioSync;
};

void mCoreConfigInit(struct mCoreConfig*, const char* port);
void mCoreConfigDeinit(struct mCoreConfig*);

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
bool mCoreConfigLoad(struct mCoreConfig*);
bool mCoreConfigSave(const struct mCoreConfig*);
bool mCoreConfigLoadPath(struct mCoreConfig*, const char* path);
bool mCoreConfigSavePath(const struct mCoreConfig*, const char* path);
bool mCoreConfigLoadVFile(struct mCoreConfig*, struct VFile* vf);
bool mCoreConfigSaveVFile(const struct mCoreConfig*, struct VFile* vf);

void mCoreConfigMakePortable(const struct mCoreConfig*);
void mCoreConfigDirectory(char* out, size_t outLength);
void mCoreConfigPortablePath(char* out, size_t outLength);
bool mCoreConfigIsPortable(void);
#endif

const char* mCoreConfigGetValue(const struct mCoreConfig*, const char* key);
bool mCoreConfigGetBoolValue(const struct mCoreConfig*, const char* key, bool* value);
bool mCoreConfigGetIntValue(const struct mCoreConfig*, const char* key, int* value);
bool mCoreConfigGetUIntValue(const struct mCoreConfig*, const char* key, unsigned* value);
bool mCoreConfigGetFloatValue(const struct mCoreConfig*, const char* key, float* value);

void mCoreConfigSetValue(struct mCoreConfig*, const char* key, const char* value);
void mCoreConfigSetIntValue(struct mCoreConfig*, const char* key, int value);
void mCoreConfigSetUIntValue(struct mCoreConfig*, const char* key, unsigned value);
void mCoreConfigSetFloatValue(struct mCoreConfig*, const char* key, float value);

void mCoreConfigSetDefaultValue(struct mCoreConfig*, const char* key, const char* value);
void mCoreConfigSetDefaultIntValue(struct mCoreConfig*, const char* key, int value);
void mCoreConfigSetDefaultUIntValue(struct mCoreConfig*, const char* key, unsigned value);
void mCoreConfigSetDefaultFloatValue(struct mCoreConfig*, const char* key, float value);

void mCoreConfigSetOverrideValue(struct mCoreConfig*, const char* key, const char* value);
void mCoreConfigSetOverrideIntValue(struct mCoreConfig*, const char* key, int value);
void mCoreConfigSetOverrideUIntValue(struct mCoreConfig*, const char* key, unsigned value);
void mCoreConfigSetOverrideFloatValue(struct mCoreConfig*, const char* key, float value);

void mCoreConfigCopyValue(struct mCoreConfig* config, const struct mCoreConfig* src, const char* key);

void mCoreConfigMap(const struct mCoreConfig* config, struct mCoreOptions* opts);
void mCoreConfigLoadDefaults(struct mCoreConfig* config, const struct mCoreOptions* opts);

void mCoreConfigEnumerate(const struct mCoreConfig* config, const char* prefix, void (*handler)(const char* key, const char* value, enum mCoreConfigLevel type, void* user), void* user);

struct Configuration* mCoreConfigGetInput(struct mCoreConfig*);
struct Configuration* mCoreConfigGetOverrides(struct mCoreConfig*);
const struct Configuration* mCoreConfigGetOverridesConst(const struct mCoreConfig*);

void mCoreConfigFreeOpts(struct mCoreOptions* opts);

CXX_GUARD_END

#endif
