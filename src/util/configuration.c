/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "configuration.h"

#include "util/vfs.h"

#include "third-party/inih/ini.h"

#include <float.h>

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
	fprintf(user, "%s=%s\n", key, (const char*) value);
}

static void _sectionHandler(const char* key, void* section, void* user) {
	fprintf(user, "[%s]\n", key);
	HashTableEnumerate(section, _keyHandler, user);
	fprintf(user, "\n");
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
		if (!currentSection && value) {
			currentSection = malloc(sizeof(*currentSection));
			HashTableInit(currentSection, 0, _sectionDeinit);
			HashTableInsert(&configuration->sections, section, currentSection);
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
	char charValue[FLT_DIG + 7];
	sprintf(charValue, "%.*g", FLT_DIG, value);
	ConfigurationSetValue(configuration, section, key, charValue);
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

bool ConfigurationRead(struct Configuration* configuration, const char* path) {
	HashTableClear(&configuration->root);
	HashTableClear(&configuration->sections);
	return ini_parse(path, _iniRead, configuration) == 0;
}

bool ConfigurationWrite(const struct Configuration* configuration, const char* path) {
	FILE* file = fopen(path, "w");
	if (!file) {
		return false;
	}
	HashTableEnumerate(&configuration->root, _keyHandler, file);
	HashTableEnumerate(&configuration->sections, _sectionHandler, file);
	fclose(file);
	return true;
}

bool ConfigurationWriteSection(const struct Configuration* configuration, const char* path, const char* section) {
	const struct Table* currentSection = &configuration->root;
	FILE* file = fopen(path, "w");
	if (!file) {
		return false;
	}
	if (section) {
		currentSection = HashTableLookup(&configuration->sections, section);
		fprintf(file, "[%s]\n", section);
	}
	if (currentSection) {
		HashTableEnumerate(currentSection, _sectionHandler, file);
	}
	fclose(file);
	return true;
}
