/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "util/table.h"

struct VFile;

struct Configuration {
	struct Table sections;
	struct Table root;
};

void ConfigurationInit(struct Configuration*);
void ConfigurationDeinit(struct Configuration*);

void ConfigurationSetValue(struct Configuration*, const char* section, const char* key, const char* value);
void ConfigurationSetIntValue(struct Configuration*, const char* section, const char* key, int value);
void ConfigurationSetUIntValue(struct Configuration*, const char* section, const char* key, unsigned value);
void ConfigurationSetFloatValue(struct Configuration*, const char* section, const char* key, float value);

bool ConfigurationHasSection(const struct Configuration*, const char* section);
const char* ConfigurationGetValue(const struct Configuration*, const char* section, const char* key);

void ConfigurationClearValue(struct Configuration*, const char* section, const char* key);

bool ConfigurationRead(struct Configuration*, const char* path);
bool ConfigurationWrite(const struct Configuration*, const char* path);
bool ConfigurationWriteSection(const struct Configuration*, const char* path, const char* section);

#endif
