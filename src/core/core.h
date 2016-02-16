/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_CORE_H
#define M_CORE_H

#include "util/common.h"

#include "core/config.h"
#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
#include "core/directories.h"
#endif
#ifndef MINIMAL_CORE
#include "core/input.h"
#endif
#include "core/interface.h"

enum mPlatform {
	PLATFORM_NONE = -1,
#ifdef M_CORE_GBA
	PLATFORM_GBA,
#endif
#ifdef M_CORE_GB
	PLATFORM_GB,
#endif
};

struct mRTCSource;
struct mCoreConfig;
struct mCoreSync;
struct mCore {
	void* cpu;
	void* board;

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	struct mDirectorySet dirs;
#endif
#ifndef MINIMAL_CORE
	struct mInputMap inputMap;
#endif
	struct mCoreConfig config;
	struct mCoreOptions opts;

	bool (*init)(struct mCore*);
	void (*deinit)(struct mCore*);

	enum mPlatform (*platform)(struct mCore*);

	void (*setSync)(struct mCore*, struct mCoreSync*);
	void (*loadConfig)(struct mCore*, const struct mCoreConfig*);

	void (*desiredVideoDimensions)(struct mCore*, unsigned* width, unsigned* height);
	void (*setVideoBuffer)(struct mCore*, color_t* buffer, size_t stride);
	void (*getVideoBuffer)(struct mCore*, color_t** buffer, size_t* stride);

	struct blip_t* (*getAudioChannel)(struct mCore*, int ch);
	void (*setAudioBufferSize)(struct mCore*, size_t samples);
	size_t (*getAudioBufferSize)(struct mCore*);

	void (*setAVStream)(struct mCore*, struct mAVStream*);

	bool (*isROM)(struct VFile* vf);
	bool (*loadROM)(struct mCore*, struct VFile* vf);
	bool (*loadSave)(struct mCore*, struct VFile* vf);
	void (*unloadROM)(struct mCore*);

	bool (*loadBIOS)(struct mCore*, struct VFile* vf, int biosID);
	bool (*selectBIOS)(struct mCore*, int biosID);

	bool (*loadPatch)(struct mCore*, struct VFile* vf);

	void (*reset)(struct mCore*);
	void (*runFrame)(struct mCore*);
	void (*runLoop)(struct mCore*);
	void (*step)(struct mCore*);

	bool (*loadState)(struct mCore*, struct VFile*, int flags);
	bool (*saveState)(struct mCore*, struct VFile*, int flags);

	void (*setKeys)(struct mCore*, uint32_t keys);
	void (*addKeys)(struct mCore*, uint32_t keys);
	void (*clearKeys)(struct mCore*, uint32_t keys);

	int32_t (*frameCounter)(struct mCore*);
	int32_t (*frameCycles)(struct mCore*);
	int32_t (*frequency)(struct mCore*);

	void (*getGameTitle)(struct mCore*, char* title);

	void (*setRTC)(struct mCore*, struct mRTCSource*);
};

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
struct mCore* mCoreFind(const char* path);
bool mCoreLoadFile(struct mCore* core, const char* path);

bool mCoreAutoloadSave(struct mCore* core);
bool mCoreAutoloadPatch(struct mCore* core);

bool mCoreSaveState(struct mCore* core, int slot, int flags);
bool mCoreLoadState(struct mCore* core, int slot, int flags);
struct VFile* mCoreGetState(struct mCore* core, int slot, bool write);
void mCoreDeleteState(struct mCore* core, int slot);

void mCoreTakeScreenshot(struct mCore* core);
#endif

void mCoreInitConfig(struct mCore* core, const char* port);
void mCoreLoadConfig(struct mCore* core);
void mCoreLoadForeignConfig(struct mCore* core, const struct mCoreConfig* config);

#endif
