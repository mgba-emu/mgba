/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/configuration.h>

#include <mgba-util/formatting.h>
#include <mgba-util/string.h>
#include <mgba-util/vfs.h>

#include "third-party/inih/ini.h"

#include <float.h>

struct ConfigurationSectionHandlerData {
	void (*handler)(const char* section, void* data);
	void* data;
};

struct ConfigurationHandlerData {
	void (*handler)(const char* key, const char* value, void* data);
	void* data;
};

static void _tableDeinit(void* table) {
	TableDeinit(table);
	free(table);
}

static void _sectionDeinit(void* string) {
	free(string);
}

static int _iniRead(void* configuration, const char* section, const char* key, const char* value) {
	if (section && !section[0]) {
		section = 0;
	}
	ConfigurationSetValue(configuration, section, key, value);
	return 1;
}

static void _keyHandler(const char* key, void* value, void* user) {
	char line[256];
	struct VFile* vf = user;
	size_t len = snprintf(line, sizeof(line), "%s=%s\n", key, (const char*) value);
	if (len >= sizeof(line)) {
		len = sizeof(line) - 1;
	}
	vf->write(vf, line, len);
}

static void _sectionHandler(const char* key, void* section, void* user) {
	char line[256];
	struct VFile* vf = user;
	size_t len = snprintf(line, sizeof(line), "[%s]\n", key);
	if (len >= sizeof(line)) {
		len = sizeof(line) - 1;
	}
	vf->write(vf, line, len);
	HashTableEnumerate(section, _keyHandler, user);
	vf->write(vf, "\n", 1);
}

static void _sectionEnumHandler(const char* key, void* section, void* user) {
	struct ConfigurationSectionHandlerData* data = user;
	UNUSED(section);
	data->handler(key, data->data);
}

static void _enumHandler(const char* key, void* value, void* user) {
	struct ConfigurationHandlerData* data = user;
	data->handler(key, value, data->data);
}

void ConfigurationInit(struct Configuration* configuration) {
	HashTableInit(&configuration->sections, 0, _tableDeinit);
	HashTableInit(&configuration->root, 0, _sectionDeinit);
}

void ConfigurationDeinit(struct Configuration* configuration) {
	HashTableDeinit(&configuration->sections);
	HashTableDeinit(&configuration->root);
}

void ConfigurationSetValue(struct Configuration* configuration, const char* section, const char* key, const char* value) {
	struct Table* currentSection = &configuration->root;
	if (section) {
		currentSection = HashTableLookup(&configuration->sections, section);
		if (!currentSection) {
			if (value) {
				currentSection = malloc(sizeof(*currentSection));
				HashTableInit(currentSection, 0, _sectionDeinit);
				HashTableInsert(&configuration->sections, section, currentSection);
			} else {
				return;
			}
		}
	}
	if (value) {
		HashTableInsert(currentSection, key, strdup(value));
	} else {
		HashTableRemove(currentSection, key);
	}
}

void ConfigurationSetIntValue(struct Configuration* configuration, const char* section, const char* key, int value) {
	char charValue[12];
	sprintf(charValue, "%i", value);
	ConfigurationSetValue(configuration, section, key, charValue);
}

void ConfigurationSetUIntValue(struct Configuration* configuration, const char* section, const char* key, unsigned value) {
	char charValue[12];
	sprintf(charValue, "%u", value);
	ConfigurationSetValue(configuration, section, key, charValue);
}

void ConfigurationSetFloatValue(struct Configuration* configuration, const char* section, const char* key, float value) {
	char charValue[16];
	ftostr_u(charValue, sizeof(charValue), value);
	ConfigurationSetValue(configuration, section, key, charValue);
}

void ConfigurationClearValue(struct Configuration* configuration, const char* section, const char* key) {
	struct Table* currentSection = &configuration->root;
	if (section) {
		currentSection = HashTableLookup(&configuration->sections, section);
		if (!currentSection) {
			return;
		}
	}
	HashTableRemove(currentSection, key);
}

bool ConfigurationHasSection(const struct Configuration* configuration, const char* section) {
	return HashTableLookup(&configuration->sections, section);
}

void ConfigurationDeleteSection(struct Configuration* configuration, const char* section) {
	HashTableRemove(&configuration->sections, section);
}

const char* ConfigurationGetValue(const struct Configuration* configuration, const char* section, const char* key) {
	const struct Table* currentSection = &configuration->root;
	if (section) {
		currentSection = HashTableLookup(&configuration->sections, section);
		if (!currentSection) {
			return 0;
		}
	}
	return HashTableLookup(currentSection, key);
}

static char* _vfgets(char* stream, int size, void* user) {
	struct VFile* vf = user;
	if (vf->readline(vf, stream, size) > 0) {
		return stream;
	}
	return 0;
}

bool ConfigurationRead(struct Configuration* configuration, const char* path) {
	struct VFile* vf = VFileOpen(path, O_RDONLY);
	if (!vf) {
		return false;
	}
	bool res = ConfigurationReadVFile(configuration, vf);
	vf->close(vf);
	return res;
}

bool ConfigurationReadVFile(struct Configuration* configuration, struct VFile* vf) {
	HashTableClear(&configuration->root);
	HashTableClear(&configuration->sections);
	return ini_parse_stream(_vfgets, vf, _iniRead, configuration) == 0;
}

bool ConfigurationWrite(const struct Configuration* configuration, const char* path) {
	struct VFile* vf = VFileOpen(path, O_WRONLY | O_CREAT | O_TRUNC);
	if (!vf) {
		return false;
	}
	bool res = ConfigurationWriteVFile(configuration, vf);
	vf->close(vf);
	return res;
}

bool ConfigurationWriteVFile(const struct Configuration* configuration, struct VFile* vf) {
	HashTableEnumerate(&configuration->root, _keyHandler, vf);
	HashTableEnumerate(&configuration->sections, _sectionHandler, vf);
	return true;
}

bool ConfigurationWriteSection(const struct Configuration* configuration, const char* path, const char* section) {
	const struct Table* currentSection = &configuration->root;
	struct VFile* vf = VFileOpen(path, O_WRONLY | O_CREAT | O_APPEND);
	if (!vf) {
		return false;
	}
	if (section) {
		currentSection = HashTableLookup(&configuration->sections, section);
		char line[256];
		size_t len = snprintf(line, sizeof(line), "[%s]\n", section);
		if (len >= sizeof(line)) {
			len = sizeof(line) - 1;
		}
		vf->write(vf, line, len);
	}
	if (currentSection) {
		HashTableEnumerate(currentSection, _sectionHandler, vf);
	}
	vf->close(vf);
	return true;
}

void ConfigurationEnumerateSections(const struct Configuration* configuration, void (*handler)(const char* sectionName, void* user), void* user) {
	struct ConfigurationSectionHandlerData handlerData = { handler, user };
	HashTableEnumerate(&configuration->sections, _sectionEnumHandler, &handlerData);
}

void ConfigurationEnumerate(const struct Configuration* configuration, const char* section, void (*handler)(const char* key, const char* value, void* user), void* user) {
	struct ConfigurationHandlerData handlerData = { handler, user };
	const struct Table* currentSection = &configuration->root;
	if (section) {
		currentSection = HashTableLookup(&configuration->sections, section);
	}
	if (currentSection) {
		HashTableEnumerate(currentSection, _enumHandler, &handlerData);
	}
}
