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
	dprintf((int) user, "%s=%s\n", key, value);
}

static void _sectionHandler(const char* key, void* section, void* user) {
	dprintf((int) user, "[%s]\n", key);
	HashTableEnumerate(section, _keyHandler, user);
	dprintf((int) user, "\n");
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
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd < 0) {
		return false;
	}
	HashTableEnumerate(&configuration->root, _keyHandler, (void*) fd);
	HashTableEnumerate(&configuration->sections, _sectionHandler, (void*) fd);
	close(fd);
	return true;
}

bool ConfigurationWriteSection(const struct Configuration* configuration, const char* path, const char* section) {
	const struct Table* currentSection = &configuration->root;
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd < 0) {
		return false;
	}
	if (section) {
		currentSection = HashTableLookup(&configuration->sections, section);
		dprintf(fd, "[%s]\n", section);
	}
	if (currentSection) {
		HashTableEnumerate(currentSection, _sectionHandler, (void*) fd);
	}
	close(fd);
	return true;
}
