/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "core.h"

#include "core/log.h"
#include "util/vfs.h"

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
#include "util/png-io.h"

bool mCoreLoadFile(struct mCore* core, const char* path) {
	struct VFile* rom = mDirectorySetOpenPath(&core->dirs, path, core->isROM);
	if (!rom) {
		return false;
	}

	bool ret = core->loadROM(core, rom);
	if (!ret) {
		rom->close(rom);
	}
	return ret;
}

bool mCoreAutoloadSave(struct mCore* core) {
	return core->loadSave(core, mDirectorySetOpenSuffix(&core->dirs, ".sav", O_CREAT | O_RDWR));
}

bool mCoreAutoloadPatch(struct mCore* core) {
	return core->loadPatch(core, mDirectorySetOpenSuffix(&core->dirs, ".ups", O_RDONLY)) ||
	       core->loadPatch(core, mDirectorySetOpenSuffix(&core->dirs, ".ips", O_RDONLY)) ||
	       core->loadPatch(core, mDirectorySetOpenSuffix(&core->dirs, ".bps", O_RDONLY));
}

bool mCoreSaveState(struct mCore* core, int slot, int flags) {
	struct VFile* vf = mCoreGetState(core, slot, true);
	if (!vf) {
		return false;
	}
	bool success = core->saveState(core, vf, flags);
	vf->close(vf);
	if (success) {
		mLOG(STATUS, INFO, "State %i saved", slot);
	} else {
		mLOG(STATUS, INFO, "State %i failed to save", slot);
	}

	return success;
}

bool mCoreLoadState(struct mCore* core, int slot, int flags) {
	struct VFile* vf = mCoreGetState(core, slot, false);
	if (!vf) {
		return false;
	}
	bool success = core->loadState(core, vf, flags);
	vf->close(vf);
	if (success) {
		mLOG(STATUS, INFO, "State %i loaded", slot);
	} else {
		mLOG(STATUS, INFO, "State %i failed to loaded", slot);
	}

	return success;
}

struct VFile* mCoreGetState(struct mCore* core, int slot, bool write) {
	char name[PATH_MAX];
	snprintf(name, sizeof(name), "%s.ss%i", core->dirs.baseName, slot);
	return core->dirs.state->openFile(core->dirs.state, name, write ? (O_CREAT | O_TRUNC | O_RDWR) : O_RDONLY);
}

void mCoreDeleteState(struct mCore* core, int slot) {
	char name[PATH_MAX];
	snprintf(name, sizeof(name), "%s.ss%i", core->dirs.baseName, slot);
	core->dirs.state->deleteFile(core->dirs.state, name);
}

void mCoreTakeScreenshot(struct mCore* core) {
#ifdef USE_PNG
	size_t stride;
	color_t* pixels = 0;
	unsigned width, height;
	core->desiredVideoDimensions(core, &width, &height);
	struct VFile* vf = VDirFindNextAvailable(core->dirs.screenshot, core->dirs.baseName, "-", ".png", O_CREAT | O_TRUNC | O_WRONLY);
	bool success = false;
	if (vf) {
		core->getVideoBuffer(core, &pixels, &stride);
		png_structp png = PNGWriteOpen(vf);
		png_infop info = PNGWriteHeader(png, width, height);
		success = PNGWritePixels(png, width, height, stride, pixels);
		PNGWriteClose(png, info);
		vf->close(vf);
	}
	if (success) {
		mLOG(STATUS, INFO, "Screenshot saved");
		return;
	}
#else
	UNUSED(core);
#endif
	mLOG(STATUS, WARN, "Failed to take screenshot");
}
#endif

void mCoreInitConfig(struct mCore* core, const char* port) {
	mCoreConfigInit(&core->config, port);
}

void mCoreLoadConfig(struct mCore* core) {
#ifndef MINIMAL_CORE
	mCoreConfigLoad(&core->config);
#endif
	mCoreConfigMap(&core->config, &core->opts);
#ifndef MINIMAL_CORE
	mDirectorySetMapOptions(&core->dirs, &core->opts);
#endif
	core->loadConfig(core);
}
