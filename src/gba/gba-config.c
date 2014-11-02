#include "gba-config.h"

#include "platform/commandline.h"
#include "util/configuration.h"

#define SECTION_NAME_MAX 128

static const char* _lookupValue(const struct Configuration* config, const char* key, const char* port) {
	if (port) {
		char sectionName[SECTION_NAME_MAX];
		snprintf(sectionName, SECTION_NAME_MAX, "ports.%s", port);
		sectionName[SECTION_NAME_MAX - 1] = '\0';
		const char* value = ConfigurationGetValue(config, sectionName, key);
		if (value) {
			return value;
		}
	}
	return ConfigurationGetValue(config, 0, key);
}

static bool _lookupCharValue(const struct Configuration* config, const char* key, const char* port, char** out) {
	const char* value = _lookupValue(config, key, port);
	if (!value) {
		return false;
	}
	*out = strdup(value);
	return true;
}

static bool _lookupIntValue(const struct Configuration* config, const char* key, const char* port, int* out) {
	const char* charValue = _lookupValue(config, key, port);
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

bool GBAConfigLoad(struct Configuration* config) {
	return ConfigurationRead(config, BINARY_NAME ".ini");
}

void GBAConfigMapStartupOpts(const struct Configuration* config, const char* port, struct StartupOptions* opts) {
	_lookupCharValue(config, "bios", port, &opts->bios);
	_lookupIntValue(config, "logLevel", port, &opts->logLevel);
	_lookupIntValue(config, "frameskip", port, &opts->frameskip);
	_lookupIntValue(config, "rewindBufferCapacity", port, &opts->rewindBufferCapacity);
	_lookupIntValue(config, "rewindBufferInterval", port, &opts->rewindBufferInterval);
}

void GBAConfigMapGraphicsOpts(const struct Configuration* config, const char* port, struct GraphicsOpts* opts) {
	_lookupIntValue(config, "fullscreen", port, &opts->fullscreen);
	_lookupIntValue(config, "width", port, &opts->width);
	_lookupIntValue(config, "height", port, &opts->height);
}
