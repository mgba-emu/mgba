/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef CONTEXT_H
#define CONTEXT_H

#include "util/common.h"

#include "gba/supervisor/config.h"
#include "gba/input.h"

struct GBAContext {
	struct GBA* gba;
	struct ARMCore* cpu;
	struct GBAVideoRenderer* renderer;
	struct VFile* rom;
	struct VFile* save;
	struct VFile* bios;
	struct GBAConfig config;
	struct GBAOptions opts;
	struct GBAInputMap inputMap;
};

bool GBAContextInit(struct GBAContext* context, const char* port);
void GBAContextDeinit(struct GBAContext* context);

bool GBAContextLoadROM(struct GBAContext* context, const char* path, bool autoloadSave);
bool GBAContextLoadROMFromVFile(struct GBAContext* context, struct VFile* rom, struct VFile* save);
bool GBAContextLoadBIOS(struct GBAContext* context, const char* path);
bool GBAContextLoadBIOSFromVFile(struct GBAContext* context, struct VFile* bios);

bool GBAContextStart(struct GBAContext* context);
void GBAContextStop(struct GBAContext* context);
void GBAContextFrame(struct GBAContext* context, uint16_t keys);

#endif
