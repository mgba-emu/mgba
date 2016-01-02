/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "config.h"

#include "util/formatting.h"
#include "util/string.h"
#include "util/vfs.h"

#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <strsafe.h>
#endif

#ifdef PSP2
#include <psp2/io/stat.h>
#endif

#ifdef _3DS
#include "platform/3ds/3ds-vfs.h"
#endif

#define SECTION_NAME_MAX 128

static const char* _lookupValue(const struct GBAConfig* config, const char* key) {
	const char* value;
	if (config->port) {
		value = ConfigurationGetValue(&config->overridesTable, config->port, key);
		if (value) {
			return value;
		}
	}
	value = ConfigurationGetValue(&config->overridesTable, 0, key);
	if (value) {
		return value;
	}
	if (config->port) {
		value = ConfigurationGetValue(&config->configTable, config->port, key);
		if (value) {
			return value;
		}
	}
	value = ConfigurationGetValue(&config->configTable, 0, key);
	if (value) {
		return value;
	}
	if (config->port) {
		value = ConfigurationGetValue(&config->defaultsTable, config->port, key);
		if (value) {
			return value;
		}
	}
	return ConfigurationGetValue(&config->defaultsTable, 0, key);
}

static bool _lookupCharValue(const struct GBAConfig* config, const char* key, char** out) {
	const char* value = _lookupValue(config, key);
	if (!value) {
		return false;
	}
	if (*out) {
		free(*out);
	}
	*out = strdup(value);
	return true;
}

static bool _lookupIntValue(const struct GBAConfig* config, const char* key, int* out) {
	const char* charValue = _lookupValue(config, key);
	if (!charValue) {
		return false;
	}
	char* end;
	long value = strtol(charValue, &end, 10);
	if (*end) {
		return false;
	}
	*out = value;
	return true;
}

static bool _lookupUIntValue(const struct GBAConfig* config, const char* key, unsigned* out) {
	const char* charValue = _lookupValue(config, key);
	if (!charValue) {
		return false;
	}
	char* end;
	unsigned long value = strtoul(charValue, &end, 10);
	if (*end) {
		return false;
	}
	*out = value;
	return true;
}

static bool _lookupFloatValue(const struct GBAConfig* config, const char* key, float* out) {
	const char* charValue = _lookupValue(config, key);
	if (!charValue) {
		return false;
	}
	char* end;
	float value = strtof_u(charValue, &end);
	if (*end) {
		return false;
	}
	*out = value;
	return true;
}

void GBAConfigInit(struct GBAConfig* config, const char* port) {
	ConfigurationInit(&config->configTable);
	ConfigurationInit(&config->defaultsTable);
	ConfigurationInit(&config->overridesTable);
	if (port) {
		config->port = malloc(strlen("ports.") + strlen(port) + 1);
		snprintf(config->port, strlen("ports.") + strlen(port) + 1, "ports.%s", port);
	} else {
		config->port = 0;
	}
}

void GBAConfigDeinit(struct GBAConfig* config) {
	ConfigurationDeinit(&config->configTable);
	ConfigurationDeinit(&config->defaultsTable);
	ConfigurationDeinit(&config->overridesTable);
	free(config->port);
}

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
bool GBAConfigLoad(struct GBAConfig* config) {
	char path[PATH_MAX];
	GBAConfigDirectory(path, PATH_MAX);
	strncat(path, PATH_SEP "config.ini", PATH_MAX - strlen(path));
	return GBAConfigLoadPath(config, path);
}

bool GBAConfigSave(const struct GBAConfig* config) {
	char path[PATH_MAX];
	GBAConfigDirectory(path, PATH_MAX);
	strncat(path, PATH_SEP "config.ini", PATH_MAX - strlen(path));
	return GBAConfigSavePath(config, path);
}

bool GBAConfigLoadPath(struct GBAConfig* config, const char* path) {
	return ConfigurationRead(&config->configTable, path);
}

bool GBAConfigSavePath(const struct GBAConfig* config, const char* path) {
	return ConfigurationWrite(&config->configTable, path);
}

void GBAConfigMakePortable(const struct GBAConfig* config) {
	struct VFile* portable = 0;
#ifdef _WIN32
	char out[MAX_PATH];
	wchar_t wpath[MAX_PATH];
	wchar_t wprojectName[MAX_PATH];
	MultiByteToWideChar(CP_UTF8, 0, projectName, -1, wprojectName, MAX_PATH);
	HMODULE hModule = GetModuleHandleW(NULL);
	GetModuleFileNameW(hModule, wpath, MAX_PATH);
	PathRemoveFileSpecW(wpath);
	WideCharToMultiByte(CP_UTF8, 0, wpath, -1, out, MAX_PATH, 0, 0);
	StringCchCatA(out, MAX_PATH, "\\portable.ini");
	portable = VFileOpen(out, O_WRONLY | O_CREAT);
#elif defined(PSP2) || defined(_3DS) || defined(GEKKO)
	// Already portable
#else
	char out[PATH_MAX];
	getcwd(out, PATH_MAX);
	strncat(out, PATH_SEP "portable.ini", PATH_MAX - strlen(out));
	portable = VFileOpen(out, O_WRONLY | O_CREAT);
#endif
	if (portable) {
		portable->close(portable);
		GBAConfigSave(config);
	}
}

void GBAConfigDirectory(char* out, size_t outLength) {
	struct VFile* portable;
#ifdef _WIN32
	wchar_t wpath[MAX_PATH];
	wchar_t wprojectName[MAX_PATH];
	MultiByteToWideChar(CP_UTF8, 0, projectName, -1, wprojectName, MAX_PATH);
	HMODULE hModule = GetModuleHandleW(NULL);
	GetModuleFileNameW(hModule, wpath, MAX_PATH);
	PathRemoveFileSpecW(wpath);
	WideCharToMultiByte(CP_UTF8, 0, wpath, -1, out, outLength, 0, 0);
	StringCchCatA(out, outLength, "\\portable.ini");
	portable = VFileOpen(out, O_RDONLY);
	if (portable) {
		portable->close(portable);
	} else {
		wchar_t* home;
		SHGetKnownFolderPath(&FOLDERID_RoamingAppData, 0, NULL, &home);
		StringCchPrintfW(wpath, MAX_PATH, L"%ws\\%ws", home, wprojectName);
		CoTaskMemFree(home);
		CreateDirectoryW(wpath, NULL);
	}
	WideCharToMultiByte(CP_UTF8, 0, wpath, -1, out, outLength, 0, 0);
#elif defined(PSP2)
	UNUSED(portable);
	snprintf(out, outLength, "cache0:/%s", projectName);
	sceIoMkdir(out, 0777);
#elif defined(GEKKO)
	UNUSED(portable);
	snprintf(out, outLength, "/%s", projectName);
	mkdir(out, 0777);
#elif defined(_3DS)
	snprintf(out, outLength, "/%s", projectName);
	FSUSER_CreateDirectory(0, sdmcArchive, FS_makePath(PATH_CHAR, out));
#else
	getcwd(out, outLength);
	strncat(out, PATH_SEP "portable.ini", outLength - strlen(out));
	portable = VFileOpen(out, O_RDONLY);
	if (portable) {
		getcwd(out, outLength);
		portable->close(portable);
		return;
	}

	char* home = getenv("HOME");
	snprintf(out, outLength, "%s/.config", home);
	mkdir(out, 0755);
	snprintf(out, outLength, "%s/.config/%s", home, binaryName);
	mkdir(out, 0755);
#endif
}
#endif

const char* GBAConfigGetValue(const struct GBAConfig* config, const char* key) {
	return _lookupValue(config, key);
}

bool GBAConfigGetIntValue(const struct GBAConfig* config, const char* key, int* value) {
	return _lookupIntValue(config, key, value);
}

bool GBAConfigGetUIntValue(const struct GBAConfig* config, const char* key, unsigned* value) {
	return _lookupUIntValue(config, key, value);
}

bool GBAConfigGetFloatValue(const struct GBAConfig* config, const char* key, float* value) {
	return _lookupFloatValue(config, key, value);
}

void GBAConfigSetValue(struct GBAConfig* config, const char* key, const char* value) {
	ConfigurationSetValue(&config->configTable, config->port, key, value);
}

void GBAConfigSetIntValue(struct GBAConfig* config, const char* key, int value) {
	ConfigurationSetIntValue(&config->configTable, config->port, key, value);
}

void GBAConfigSetUIntValue(struct GBAConfig* config, const char* key, unsigned value) {
	ConfigurationSetUIntValue(&config->configTable, config->port, key, value);
}

void GBAConfigSetFloatValue(struct GBAConfig* config, const char* key, float value) {
	ConfigurationSetFloatValue(&config->configTable, config->port, key, value);
}

void GBAConfigSetDefaultValue(struct GBAConfig* config, const char* key, const char* value) {
	ConfigurationSetValue(&config->defaultsTable, config->port, key, value);
}

void GBAConfigSetDefaultIntValue(struct GBAConfig* config, const char* key, int value) {
	ConfigurationSetIntValue(&config->defaultsTable, config->port, key, value);
}

void GBAConfigSetDefaultUIntValue(struct GBAConfig* config, const char* key, unsigned value) {
	ConfigurationSetUIntValue(&config->defaultsTable, config->port, key, value);
}

void GBAConfigSetDefaultFloatValue(struct GBAConfig* config, const char* key, float value) {
	ConfigurationSetFloatValue(&config->defaultsTable, config->port, key, value);
}

void GBAConfigSetOverrideValue(struct GBAConfig* config, const char* key, const char* value) {
	ConfigurationSetValue(&config->overridesTable, config->port, key, value);
}

void GBAConfigSetOverrideIntValue(struct GBAConfig* config, const char* key, int value) {
	ConfigurationSetIntValue(&config->overridesTable, config->port, key, value);
}

void GBAConfigSetOverrideUIntValue(struct GBAConfig* config, const char* key, unsigned value) {
	ConfigurationSetUIntValue(&config->overridesTable, config->port, key, value);
}

void GBAConfigSetOverrideFloatValue(struct GBAConfig* config, const char* key, float value) {
	ConfigurationSetFloatValue(&config->overridesTable, config->port, key, value);
}

void GBAConfigMap(const struct GBAConfig* config, struct GBAOptions* opts) {
	_lookupCharValue(config, "bios", &opts->bios);
	_lookupCharValue(config, "shader", &opts->shader);
	_lookupIntValue(config, "logLevel", &opts->logLevel);
	_lookupIntValue(config, "frameskip", &opts->frameskip);
	_lookupIntValue(config, "volume", &opts->volume);
	_lookupIntValue(config, "rewindBufferCapacity", &opts->rewindBufferCapacity);
	_lookupIntValue(config, "rewindBufferInterval", &opts->rewindBufferInterval);
	_lookupFloatValue(config, "fpsTarget", &opts->fpsTarget);
	unsigned audioBuffers;
	if (_lookupUIntValue(config, "audioBuffers", &audioBuffers)) {
		opts->audioBuffers = audioBuffers;
	}
	_lookupUIntValue(config, "sampleRate", &opts->sampleRate);

	int fakeBool;
	if (_lookupIntValue(config, "useBios", &fakeBool)) {
		opts->useBios = fakeBool;
	}
	if (_lookupIntValue(config, "audioSync", &fakeBool)) {
		opts->audioSync = fakeBool;
	}
	if (_lookupIntValue(config, "videoSync", &fakeBool)) {
		opts->videoSync = fakeBool;
	}
	if (_lookupIntValue(config, "lockAspectRatio", &fakeBool)) {
		opts->lockAspectRatio = fakeBool;
	}
	if (_lookupIntValue(config, "resampleVideo", &fakeBool)) {
		opts->resampleVideo = fakeBool;
	}
	if (_lookupIntValue(config, "suspendScreensaver", &fakeBool)) {
		opts->suspendScreensaver = fakeBool;
	}
	if (_lookupIntValue(config, "mute", &fakeBool)) {
		opts->mute = fakeBool;
	}
	if (_lookupIntValue(config, "skipBios", &fakeBool)) {
		opts->skipBios = fakeBool;
	}
	if (_lookupIntValue(config, "rewindEnable", &fakeBool)) {
		opts->rewindEnable = fakeBool;
	}

	_lookupIntValue(config, "fullscreen", &opts->fullscreen);
	_lookupIntValue(config, "width", &opts->width);
	_lookupIntValue(config, "height", &opts->height);

	char* idleOptimization = 0;
	if (_lookupCharValue(config, "idleOptimization", &idleOptimization)) {
		if (strcasecmp(idleOptimization, "ignore") == 0) {
			opts->idleOptimization = IDLE_LOOP_IGNORE;
		} else if (strcasecmp(idleOptimization, "remove") == 0) {
			opts->idleOptimization = IDLE_LOOP_REMOVE;
		} else if (strcasecmp(idleOptimization, "detect") == 0) {
			opts->idleOptimization = IDLE_LOOP_DETECT;
		}
		free(idleOptimization);
	}
}

void GBAConfigLoadDefaults(struct GBAConfig* config, const struct GBAOptions* opts) {
	ConfigurationSetValue(&config->defaultsTable, 0, "bios", opts->bios);
	ConfigurationSetValue(&config->defaultsTable, 0, "shader", opts->shader);
	ConfigurationSetIntValue(&config->defaultsTable, 0, "skipBios", opts->skipBios);
	ConfigurationSetIntValue(&config->defaultsTable, 0, "useBios", opts->useBios);
	ConfigurationSetIntValue(&config->defaultsTable, 0, "logLevel", opts->logLevel);
	ConfigurationSetIntValue(&config->defaultsTable, 0, "frameskip", opts->frameskip);
	ConfigurationSetIntValue(&config->defaultsTable, 0, "rewindEnable", opts->rewindEnable);
	ConfigurationSetIntValue(&config->defaultsTable, 0, "rewindBufferCapacity", opts->rewindBufferCapacity);
	ConfigurationSetIntValue(&config->defaultsTable, 0, "rewindBufferInterval", opts->rewindBufferInterval);
	ConfigurationSetFloatValue(&config->defaultsTable, 0, "fpsTarget", opts->fpsTarget);
	ConfigurationSetUIntValue(&config->defaultsTable, 0, "audioBuffers", opts->audioBuffers);
	ConfigurationSetUIntValue(&config->defaultsTable, 0, "sampleRate", opts->sampleRate);
	ConfigurationSetIntValue(&config->defaultsTable, 0, "audioSync", opts->audioSync);
	ConfigurationSetIntValue(&config->defaultsTable, 0, "videoSync", opts->videoSync);
	ConfigurationSetIntValue(&config->defaultsTable, 0, "fullscreen", opts->fullscreen);
	ConfigurationSetIntValue(&config->defaultsTable, 0, "width", opts->width);
	ConfigurationSetIntValue(&config->defaultsTable, 0, "height", opts->height);
	ConfigurationSetIntValue(&config->defaultsTable, 0, "volume", opts->volume);
	ConfigurationSetIntValue(&config->defaultsTable, 0, "mute", opts->mute);
	ConfigurationSetIntValue(&config->defaultsTable, 0, "lockAspectRatio", opts->lockAspectRatio);
	ConfigurationSetIntValue(&config->defaultsTable, 0, "resampleVideo", opts->resampleVideo);
	ConfigurationSetIntValue(&config->defaultsTable, 0, "suspendScreensaver", opts->suspendScreensaver);

	switch (opts->idleOptimization) {
	case IDLE_LOOP_IGNORE:
		ConfigurationSetValue(&config->defaultsTable, 0, "idleOptimization", "ignore");
		break;
	case IDLE_LOOP_REMOVE:
		ConfigurationSetValue(&config->defaultsTable, 0, "idleOptimization", "remove");
		break;
	case IDLE_LOOP_DETECT:
		ConfigurationSetValue(&config->defaultsTable, 0, "idleOptimization", "detect");
		break;
	}
}

// These two are basically placeholders in case the internal layout changes, e.g. for loading separate files
struct Configuration* GBAConfigGetInput(struct GBAConfig* config) {
	return &config->configTable;
}

struct Configuration* GBAConfigGetOverrides(struct GBAConfig* config) {
	return &config->configTable;
}

void GBAConfigFreeOpts(struct GBAOptions* opts) {
	free(opts->bios);
	free(opts->shader);
	opts->bios = 0;
	opts->shader = 0;
}
