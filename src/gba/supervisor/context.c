/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba/supervisor/context.h"

#include "gba/supervisor/overrides.h"

#include "util/memory.h"
#include "util/vfs.h"

bool GBAContextInit(struct GBAContext* context, const char* port) {
	context->gba = anonymousMemoryMap(sizeof(struct GBA));
	context->cpu = anonymousMemoryMap(sizeof(struct ARMCore));
	context->rom = 0;
	context->save = 0;
	context->renderer = 0;

	if (!context->gba || !context->cpu) {
		if (context->gba) {
			mappedMemoryFree(context->gba, sizeof(struct GBA));
		}
		if (context->cpu) {
			mappedMemoryFree(context->cpu, sizeof(struct ARMCore));
		}
		return false;
	}

	GBAConfigInit(&context->config, port);
	if (port) {
		GBAConfigLoad(&context->config);
	}

	GBACreate(context->gba);
	ARMSetComponents(context->cpu, &context->gba->d, 0, 0);
	ARMInit(context->cpu);

	context->gba->sync = 0;
	return true;
}

void GBAContextDeinit(struct GBAContext* context) {
	if (context->bios) {
		context->bios->close(context->bios);
		context->bios = 0;
	}
	if (context->rom) {
		context->rom->close(context->rom);
		context->rom = 0;
	}
	if (context->save) {
		context->save->close(context->save);
		context->save = 0;
	}
	ARMDeinit(context->cpu);
	GBADestroy(context->gba);
	mappedMemoryFree(context->gba, 0);
	mappedMemoryFree(context->cpu, 0);
	GBAConfigDeinit(&context->config);
}

bool GBAContextLoadROM(struct GBAContext* context, const char* path, bool autoloadSave) {
	context->rom = VFileOpen(path, O_RDONLY);
	if (!context->rom) {
		return false;
	}

	if (!GBAIsROM(context->rom)) {
		context->rom->close(context->rom);
		context->rom = 0;
		return false;
	}

	if (autoloadSave) {
		context->save = VDirOptionalOpenFile(0, path, 0, ".sav", O_RDWR | O_CREAT);
	}
	return true;
}

bool GBAContextLoadROMFromVFile(struct GBAContext* context, struct VFile* rom, struct VFile* save) {
	context->rom = rom;
	if (!GBAIsROM(context->rom)) {
		context->rom = 0;
		return false;
	}
	context->save = save;
	return true;
}

bool GBAContextLoadBIOS(struct GBAContext* context, const char* path) {
	context->bios = VFileOpen(path, O_RDONLY);
	if (!context->bios) {
		return false;
	}

	if (!GBAIsBIOS(context->bios)) {
		context->bios->close(context->bios);
		context->bios = 0;
		return false;
	}
	return true;
}

bool GBAContextLoadBIOSFromVFile(struct GBAContext* context, struct VFile* bios) {
	context->bios = bios;
	if (!GBAIsBIOS(context->bios)) {
		context->bios = 0;
		return false;
	}
	return true;
}

bool GBAContextStart(struct GBAContext* context) {
	struct GBAOptions opts = {};
	GBAConfigMap(&context->config, &opts);

	if (context->renderer) {
		GBAVideoAssociateRenderer(&context->gba->video, context->renderer);
	}

	GBALoadROM(context->gba, context->rom, context->save, 0);
	if (opts.useBios && context->bios) {
		GBALoadBIOS(context->gba, context->bios);
	}

	ARMReset(context->cpu);

	if (opts.skipBios) {
		GBASkipBIOS(context->cpu);
	}

	struct GBACartridgeOverride override;
	const struct GBACartridge* cart = (const struct GBACartridge*) context->gba->memory.rom;
	memcpy(override.id, &cart->id, sizeof(override.id));
	if (GBAOverrideFind(GBAConfigGetOverrides(&context->config), &override)) {
		GBAOverrideApply(context->gba, &override);
	}
	GBAConfigFreeOpts(&opts);
	return true;
}

void GBAContextStop(struct GBAContext* context) {
	UNUSED(context);
	// TODO?
}

void GBAContextFrame(struct GBAContext* context, uint16_t keys) {
	int activeKeys = keys;
	context->gba->keySource = &activeKeys;

	int frameCounter = context->gba->video.frameCounter;
	while (frameCounter == context->gba->video.frameCounter) {
		ARMRunLoop(context->cpu);
	}
}
