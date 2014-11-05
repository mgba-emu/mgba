#include "configuration.h"

#include "util/vfs.h"

#include "third-party/inih/ini.h"

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
	HashTableInit(&configuration->sections, 0, (void (*)(void *)) TableDeinit);
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
	struct Table* currentSection = &configuration->root;
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
