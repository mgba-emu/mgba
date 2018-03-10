/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/core.h>

#include <mgba/core/cheats.h>
#include <mgba/core/log.h>
#include <mgba/core/serialize.h>
#include <mgba-util/vfs.h>
#include <mgba/internal/debugger/symbols.h>

#ifdef USE_ELF
#include <mgba-util/elf-read.h>
#endif

#ifdef M_CORE_GB
#include <mgba/gb/core.h>
// TODO: Fix layering violation
#include <mgba/internal/gb/gb.h>
#endif
#ifdef M_CORE_GBA
#include <mgba/gba/core.h>
#include <mgba/internal/gba/gba.h>
#endif
#ifndef MINIMAL_CORE
#include <mgba/feature/video-logger.h>
#endif

static const struct mCoreFilter {
	bool (*filter)(struct VFile*);
	struct mCore* (*open)(void);
	enum mPlatform platform;
} _filters[] = {
#ifdef M_CORE_GBA
	{ GBAIsROM, GBACoreCreate, PLATFORM_GBA },
#endif
#ifdef M_CORE_GB
	{ GBIsROM, GBCoreCreate, PLATFORM_GB },
#endif
	{ 0, 0, PLATFORM_NONE }
};

struct mCore* mCoreFindVF(struct VFile* vf) {
	if (!vf) {
		return NULL;
	}
	const struct mCoreFilter* filter;
	for (filter = &_filters[0]; filter->filter; ++filter) {
		if (filter->filter(vf)) {
			break;
		}
	}
	if (filter->open) {
		return filter->open();
	}
#ifndef MINIMAL_CORE
	return mVideoLogCoreFind(vf);
#endif
	return NULL;
}

enum mPlatform mCoreIsCompatible(struct VFile* vf) {
	if (!vf) {
		return false;
	}
	const struct mCoreFilter* filter;
	for (filter = &_filters[0]; filter->filter; ++filter) {
		if (filter->filter(vf)) {
			return filter->platform;
		}
	}
	return PLATFORM_NONE;
}

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
#include <mgba-util/png-io.h>

#ifdef PSP2
#include <psp2/photoexport.h>
#endif

struct mCore* mCoreFind(const char* path) {
	struct VDir* archive = VDirOpenArchive(path);
	struct mCore* core = NULL;
	if (archive) {
		struct VDirEntry* dirent = archive->listNext(archive);
		while (dirent) {
			struct VFile* vf = archive->openFile(archive, dirent->name(dirent), O_RDONLY);
			if (!vf) {
				dirent = archive->listNext(archive);
				continue;
			}
			core = mCoreFindVF(vf);
			vf->close(vf);
			if (core) {
				break;
			}
			dirent = archive->listNext(archive);
		}
		archive->close(archive);
	} else {
		struct VFile* vf = VFileOpen(path, O_RDONLY);
		if (!vf) {
			return NULL;
		}
		core = mCoreFindVF(vf);
		vf->close(vf);
	}
	if (core) {
		return core;
	}
	return NULL;
}

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

bool mCorePreloadVF(struct mCore* core, struct VFile* vf) {
	struct VFile* vfm = VFileMemChunk(NULL, vf->size(vf));
	uint8_t buffer[2048];
	ssize_t read;
	vf->seek(vf, 0, SEEK_SET);
	while ((read = vf->read(vf, buffer, sizeof(buffer))) > 0) {
		vfm->write(vfm, buffer, read);
	}
	vf->close(vf);
	bool ret = core->loadROM(core, vfm);
	if (!ret) {
		vfm->close(vfm);
	}
	return ret;
}

bool mCorePreloadFile(struct mCore* core, const char* path) {
	struct VFile* rom = mDirectorySetOpenPath(&core->dirs, path, core->isROM);
	if (!rom) {
		return false;
	}

	bool ret = mCorePreloadVF(core, rom);
	if (!ret) {
		rom->close(rom);
	}
	return ret;
}

bool mCoreAutoloadSave(struct mCore* core) {
	return core->loadSave(core, mDirectorySetOpenSuffix(&core->dirs, core->dirs.save, ".sav", O_CREAT | O_RDWR));
}

bool mCoreAutoloadPatch(struct mCore* core) {
	return core->loadPatch(core, mDirectorySetOpenSuffix(&core->dirs, core->dirs.patch, ".ups", O_RDONLY)) ||
	       core->loadPatch(core, mDirectorySetOpenSuffix(&core->dirs, core->dirs.patch, ".ips", O_RDONLY)) ||
	       core->loadPatch(core, mDirectorySetOpenSuffix(&core->dirs, core->dirs.patch, ".bps", O_RDONLY));
}

bool mCoreAutoloadCheats(struct mCore* core) {
	bool success = true;
	int cheatAuto;
	if (!mCoreConfigGetIntValue(&core->config, "cheatAutoload", &cheatAuto) || cheatAuto) {
		struct VFile* vf = mDirectorySetOpenSuffix(&core->dirs, core->dirs.cheats, ".cheats", O_RDONLY);
		if (vf) {
			struct mCheatDevice* device = core->cheatDevice(core);
			success = mCheatParseFile(device, vf);
			vf->close(vf);
		}
	}
	if (!mCoreConfigGetIntValue(&core->config, "cheatAutosave", &cheatAuto) || cheatAuto) {
		struct mCheatDevice* device = core->cheatDevice(core);
		device->autosave = true;
	}
	return success;
}

bool mCoreSaveState(struct mCore* core, int slot, int flags) {
	struct VFile* vf = mCoreGetState(core, slot, true);
	if (!vf) {
		return false;
	}
	bool success = mCoreSaveStateNamed(core, vf, flags);
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
	bool success = mCoreLoadStateNamed(core, vf, flags);
	vf->close(vf);
	if (success) {
		mLOG(STATUS, INFO, "State %i loaded", slot);
	} else {
		mLOG(STATUS, INFO, "State %i failed to load", slot);
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
	const void* pixels = 0;
	unsigned width, height;
	core->desiredVideoDimensions(core, &width, &height);
	struct VFile* vf;
#ifndef PSP2
	vf = VDirFindNextAvailable(core->dirs.screenshot, core->dirs.baseName, "-", ".png", O_CREAT | O_TRUNC | O_WRONLY);
#else
	vf = VFileMemChunk(0, 0);
#endif
	bool success = false;
	if (vf) {
		core->getPixels(core, &pixels, &stride);
		png_structp png = PNGWriteOpen(vf);
		png_infop info = PNGWriteHeader(png, width, height);
		success = PNGWritePixels(png, width, height, stride, pixels);
		PNGWriteClose(png, info);
#ifdef PSP2
		void* data = vf->map(vf, 0, 0);
		PhotoExportParam param = {
			0,
			NULL,
			NULL,
			NULL,
			{ 0 }
		};
		scePhotoExportFromData(data, vf->size(vf), &param, NULL, NULL, NULL, NULL, 0);
#endif
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
#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	mCoreConfigLoad(&core->config);
#endif
	mCoreLoadForeignConfig(core, &core->config);
}

void mCoreLoadForeignConfig(struct mCore* core, const struct mCoreConfig* config) {
	mCoreConfigMap(config, &core->opts);
#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	mDirectorySetMapOptions(&core->dirs, &core->opts);
#endif
	if (core->opts.audioBuffers) {
		core->setAudioBufferSize(core, core->opts.audioBuffers);
	}

	mCoreConfigCopyValue(&core->config, config, "cheatAutosave");
	mCoreConfigCopyValue(&core->config, config, "cheatAutoload");

	core->loadConfig(core, config);
}

void mCoreSetRTC(struct mCore* core, struct mRTCSource* rtc) {
	core->rtc.custom = rtc;
	core->rtc.override = RTC_CUSTOM_START;
}

void* mCoreGetMemoryBlock(struct mCore* core, uint32_t start, size_t* size) {
	const struct mCoreMemoryBlock* blocks;
	size_t nBlocks = core->listMemoryBlocks(core, &blocks);
	size_t i;
	for (i = 0; i < nBlocks; ++i) {
		if (!(blocks[i].flags & mCORE_MEMORY_MAPPED)) {
			continue;
		}
		if (start < blocks[i].start) {
			continue;
		}
		if (start >= blocks[i].start + blocks[i].size) {
			continue;
		}
		uint8_t* out = core->getMemoryBlock(core, blocks[i].id, size);
		out += start - blocks[i].start;
		*size -= start - blocks[i].start;
		return out;
	}
	return NULL;
}

#ifdef USE_ELF
bool mCoreLoadELF(struct mCore* core, struct ELF* elf) {
	struct ELFProgramHeaders ph;
	ELFProgramHeadersInit(&ph, 0);
	ELFGetProgramHeaders(elf, &ph);
	size_t i;
	for (i = 0; i < ELFProgramHeadersSize(&ph); ++i) {
		size_t bsize, esize;
		Elf32_Phdr* phdr = ELFProgramHeadersGetPointer(&ph, i);
		void* block = mCoreGetMemoryBlock(core, phdr->p_paddr, &bsize);
		char* bytes = ELFBytes(elf, &esize);
		if (block && bsize >= phdr->p_filesz && esize >= phdr->p_filesz + phdr->p_offset) {
			memcpy(block, &bytes[phdr->p_offset], phdr->p_filesz);
		} else {
			return false;
		}
	}
	return true;
}

#ifdef USE_DEBUGGERS
void mCoreLoadELFSymbols(struct mDebuggerSymbols* symbols, struct ELF* elf) {
	size_t symIndex = ELFFindSection(elf, ".symtab");
	size_t names = ELFFindSection(elf, ".strtab");
	Elf32_Shdr* symHeader = ELFGetSectionHeader(elf, symIndex);
	char* bytes = ELFBytes(elf, NULL);

	Elf32_Sym* syms = (Elf32_Sym*) &bytes[symHeader->sh_offset];
	size_t i;
	for (i = 0; i * sizeof(*syms) < symHeader->sh_size; ++i) {
		if (!syms[i].st_name || ELF32_ST_TYPE(syms[i].st_info) == STT_FILE) {
			continue;
		}
		const char* name = ELFGetString(elf, names, syms[i].st_name);
		if (name[0] == '$') {
			continue;
		}
		mDebuggerSymbolAdd(symbols, name, syms[i].st_value, -1);
	}
}
#endif
#endif
