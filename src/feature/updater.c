/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/feature/updater.h>

#include <mgba-util/string.h>
#include <mgba-util/table.h>
#include <mgba-util/vector.h>
#include <mgba-util/vfs.h>

#define UPDATE_SECTION "update"

struct mUpdateMatch {
	const char* channel;
	struct mUpdate* out;
};

static void _updateListSections(const char* sectionName, void* user) {
	struct StringList* out = user;
	if (strncmp("platform.", sectionName, 9) == 0) {
		*StringListAppend(out) = (char*) &sectionName[9];
	}
}

static void _updateUpdate(struct mUpdate* update, const char* item, const char* value) {
	if (strcmp("name", item) == 0) {
		update->path = value;
	} else if (strcmp("version", item) == 0) {
		update->version = value;
	} else if (strcmp("size", item) == 0) {
		update->size = strtoull(value, NULL, 10);
	} else if (strcmp("rev", item) == 0) {
		update->rev = strtol(value, NULL, 10);
	} else if (strcmp("commit", item) == 0) {
		update->commit = value;
	} else if (strcmp("sha256", item) == 0) {
		update->sha256 = value;
	}
}

static void _updateList(const char* key, const char* value, void* user) {
	char channel[64] = {0};
	const char* dotLoc;
	if (strncmp("medusa.", key, 7) == 0) {
		dotLoc = strchr(&key[7], '.');
	} else {
		dotLoc = strchr(key, '.');		
	}
	if (!dotLoc) {
		return;
	}
	size_t size = dotLoc - key;
	if (size >= sizeof(channel)) {
		return;
	}
	strncpy(channel, key, size);
	const char* item = &key[size + 1];

	struct Table* out = user;
	struct mUpdate* update = HashTableLookup(out, channel);
	if (!update) {
		update = calloc(1, sizeof(*update));
		HashTableInsert(out, channel, update);
	}
	_updateUpdate(update, item, value);
}

static void _updateMatch(const char* key, const char* value, void* user) {
	struct mUpdateMatch* match = user;

	size_t dotLoc = strlen(match->channel);
	if (dotLoc >= strlen(key) || key[dotLoc] != '.') {
		return;
	}
	if (strncmp(match->channel, key, dotLoc) != 0) {
		return;
	}
	const char* item = &key[dotLoc + 1];
	_updateUpdate(match->out, item, value);
}

bool mUpdaterInit(struct mUpdaterContext* context, const char* manifest) {
	ConfigurationInit(&context->manifest);

	struct VFile* vf = VFileFromConstMemory(manifest, strlen(manifest) + 1);
	bool success = ConfigurationReadVFile(&context->manifest, vf);
	vf->close(vf);
	if (!success) {
		ConfigurationDeinit(&context->manifest);
	}
	return success;
}

void mUpdaterDeinit(struct mUpdaterContext* context) {
	ConfigurationDeinit(&context->manifest);
}

void mUpdaterGetPlatforms(const struct mUpdaterContext* context, struct StringList* out) {
	StringListClear(out);
	ConfigurationEnumerateSections(&context->manifest, _updateListSections, out);
}

void mUpdaterGetUpdates(const struct mUpdaterContext* context, const char* platform, struct Table* out) {
	char section[64] = {'p', 'l', 'a', 't', 'f', 'o', 'r', 'm', '.'};
	strncpy(&section[9], platform, sizeof(section) - 10);
	ConfigurationEnumerate(&context->manifest, section, _updateList, out);
}

void mUpdaterGetUpdateForChannel(const struct mUpdaterContext* context, const char* platform, const char* channel, struct mUpdate* out) {
	char section[64] = {'p', 'l', 'a', 't', 'f', 'o', 'r', 'm', '.'};
	strncpy(&section[9], platform, sizeof(section) - 10);
	struct mUpdateMatch match = {
		.channel = channel,
		.out = out
	};
	ConfigurationEnumerate(&context->manifest, section, _updateMatch, &match);
}

const char* mUpdaterGetBucket(const struct mUpdaterContext* context) {
	return ConfigurationGetValue(&context->manifest, "meta", "bucket");
}

void mUpdateRecord(struct mCoreConfig* config, const char* prefix, const struct mUpdate* update) {
	char key[128];
	snprintf(key, sizeof(key), "%s.path", prefix);
	mCoreConfigSetValue(config, key, update->path);
	snprintf(key, sizeof(key), "%s.size", prefix);
	mCoreConfigSetUIntValue(config, key, update->size);
	snprintf(key, sizeof(key), "%s.rev", prefix);
	if (update->rev > 0) {
		mCoreConfigSetIntValue(config, key, update->rev);
	} else {
		mCoreConfigSetValue(config, key, NULL);
	}
	snprintf(key, sizeof(key), "%s.version", prefix);
	mCoreConfigSetValue(config, key, update->version);
	snprintf(key, sizeof(key), "%s.commit", prefix);
	mCoreConfigSetValue(config, key, update->commit);
	snprintf(key, sizeof(key), "%s.sha256", prefix);
	mCoreConfigSetValue(config, key, update->sha256);
}

bool mUpdateLoad(const struct mCoreConfig* config, const char* prefix, struct mUpdate* update) {
	char key[128];
	memset(update, 0, sizeof(*update));
	snprintf(key, sizeof(key), "%s.path", prefix);
	update->path = mCoreConfigGetValue(config, key);
	snprintf(key, sizeof(key), "%s.size", prefix);
	uint32_t size = 0;
	mCoreConfigGetUIntValue(config, key, &size);
	if (!update->path && !size) {
		return false;
	}
	update->size = size;
	snprintf(key, sizeof(key), "%s.rev", prefix);
	mCoreConfigGetIntValue(config, key, &update->rev);
	snprintf(key, sizeof(key), "%s.version", prefix);
	update->version = mCoreConfigGetValue(config, key);
	snprintf(key, sizeof(key), "%s.commit", prefix);
	update->commit = mCoreConfigGetValue(config, key);
	snprintf(key, sizeof(key), "%s.sha256", prefix);
	update->sha256 = mCoreConfigGetValue(config, key);
	return true;
}

void mUpdateRegister(struct mCoreConfig* config, const char* arg0, const char* updatePath) {
	struct Configuration* cfg = &config->configTable;
	char filename[PATH_MAX];

	strlcpy(filename, arg0, sizeof(filename));
	char* last;
#ifdef _WIN32
	last = strrchr(filename, '\\');
#else
	last = strrchr(filename, '/');
#endif
	if (last) {
		last[0] = '\0';
#ifdef __APPLE__
		ssize_t len = strlen(filename);
		if (len > 19 && strcmp(&filename[len - 19], ".app/Contents/MacOS") == 0) {
			filename[len - 19] = '\0';
			last = strrchr(filename, '/');
			if (last) {
				last[0] = '\0';
			}
		}
#endif
	}
	ConfigurationSetValue(cfg, UPDATE_SECTION, "bin", arg0);
	ConfigurationSetValue(cfg, UPDATE_SECTION, "root", filename);

	separatePath(updatePath, NULL, NULL, filename);
	ConfigurationSetValue(cfg, UPDATE_SECTION, "extension", filename);
	mCoreConfigSave(config);
}

void mUpdateDeregister(struct mCoreConfig* config) {
	ConfigurationDeleteSection(&config->configTable, UPDATE_SECTION);
	mCoreConfigSave(config);
}

const char* mUpdateGetRoot(const struct mCoreConfig* config) {
	return ConfigurationGetValue(&config->configTable, UPDATE_SECTION, "root");
}

const char* mUpdateGetCommand(const struct mCoreConfig* config) {
	return ConfigurationGetValue(&config->configTable, UPDATE_SECTION, "bin");
}

const char* mUpdateGetArchiveExtension(const struct mCoreConfig* config) {
	return ConfigurationGetValue(&config->configTable, UPDATE_SECTION, "extension");
}

bool mUpdateGetArchivePath(const struct mCoreConfig* config, char* out, size_t outLength) {
	const char* extension = ConfigurationGetValue(&config->configTable, UPDATE_SECTION, "extension");
	if (!extension) {
		return false;
	}
	mCoreConfigDirectory(out, outLength);
	size_t start = strlen(out);
	outLength -= start;
	snprintf(&out[start], outLength, PATH_SEP "update.%s", extension);
	return true;
}
