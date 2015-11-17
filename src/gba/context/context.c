/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba/context/context.h"

#include "gba/context/overrides.h"

#include "util/memory.h"
#include "util/vfs.h"

static struct VFile* _logFile = 0;
static void _GBAContextLog(struct GBAThread* thread, enum GBALogLevel level, const char* format, va_list args);

bool GBAContextInit(struct GBAContext* context, const char* port) {
	context->gba = anonymousMemoryMap(sizeof(struct GBA));
	context->cpu = anonymousMemoryMap(sizeof(struct ARMCore));
	context->rom = 0;
	context->bios = 0;
	context->fname = 0;
	context->save = 0;
	context->renderer = 0;
	memset(context->components, 0, sizeof(context->components));

	if (!context->gba || !context->cpu) {
		if (context->gba) {
			mappedMemoryFree(context->gba, sizeof(struct GBA));
		}
		if (context->cpu) {
			mappedMemoryFree(context->cpu, sizeof(struct ARMCore));
		}
		return false;
	}
	GBACreate(context->gba);
	ARMSetComponents(context->cpu, &context->gba->d, GBA_COMPONENT_MAX, context->components);
	ARMInit(context->cpu);

	GBAConfigInit(&context->config, port);
#ifndef __LIBRETRO__
	if (port) {
		if (!_logFile) {
			char logPath[PATH_MAX];
			GBAConfigDirectory(logPath, PATH_MAX);
			strncat(logPath, PATH_SEP "log", PATH_MAX - strlen(logPath));
			_logFile = VFileOpen(logPath, O_WRONLY | O_CREAT | O_TRUNC);
		}
		context->gba->logHandler = _GBAContextLog;

		char biosPath[PATH_MAX];
		GBAConfigDirectory(biosPath, PATH_MAX);
		strncat(biosPath, PATH_SEP "gba_bios.bin", PATH_MAX - strlen(biosPath));

		struct GBAOptions opts = {
			.bios = biosPath,
			.useBios = true,
			.idleOptimization = IDLE_LOOP_DETECT,
			.logLevel = GBA_LOG_WARN | GBA_LOG_ERROR | GBA_LOG_FATAL | GBA_LOG_STATUS
		};
		GBAConfigLoad(&context->config);
		GBAConfigLoadDefaults(&context->config, &opts);
   }
#endif

	context->gba->sync = 0;
	return true;
}

void GBAContextDeinit(struct GBAContext* context) {
	ARMDeinit(context->cpu);
	GBADestroy(context->gba);
	mappedMemoryFree(context->gba, 0);
	mappedMemoryFree(context->cpu, 0);
	GBAConfigDeinit(&context->config);
}

bool GBAContextLoadROM(struct GBAContext* context, const char* path, bool autoloadSave) {
	struct VDir* dir = VDirOpenArchive(path);
	if (dir) {
		struct VDirEntry* de;
		while ((de = dir->listNext(dir))) {
			struct VFile* vf = dir->openFile(dir, de->name(de), O_RDONLY);
			if (!vf) {
				continue;
			}
			if (GBAIsROM(vf)) {
				context->rom = vf;
				break;
			}
			vf->close(vf);
		}
		dir->close(dir);
	} else {
		context->rom = VFileOpen(path, O_RDONLY);
	}

	if (!context->rom) {
		return false;
	}

	if (!GBAIsROM(context->rom)) {
		context->rom->close(context->rom);
		context->rom = 0;
		return false;
	}

	context->fname = path;
	if (autoloadSave) {
		context->save = VDirOptionalOpenFile(0, path, 0, ".sav", O_RDWR | O_CREAT);
	}
	return true;
}

void GBAContextUnloadROM(struct GBAContext* context) {
	GBAUnloadROM(context->gba);
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
	struct GBAOptions opts = { .bios = 0 };

	if (context->renderer) {
		GBAVideoAssociateRenderer(&context->gba->video, context->renderer);
	}

	if (!GBALoadROM(context->gba, context->rom, context->save, context->fname)) {
		return false;
	}

	GBAConfigMap(&context->config, &opts);

	if (!context->bios && opts.bios) {
		GBAContextLoadBIOS(context, opts.bios);
	}
	if (opts.useBios && context->bios) {
		GBALoadBIOS(context->gba, context->bios);
	}
	context->gba->logLevel = opts.logLevel;
	context->gba->idleOptimization = opts.idleOptimization;

	ARMReset(context->cpu);

	if (opts.skipBios) {
		GBASkipBIOS(context->gba);
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

static void _GBAContextLog(struct GBAThread* thread, enum GBALogLevel level, const char* format, va_list args) {
	UNUSED(thread);
	UNUSED(level);
	// TODO: Make this local
	if (!_logFile) {
		return;
	}
	char out[256];
	size_t len = vsnprintf(out, sizeof(out), format, args);
	if (len >= sizeof(out)) {
		len = sizeof(out) - 1;
	}
	out[len] = '\n';
	_logFile->write(_logFile, out, len + 1);
}
