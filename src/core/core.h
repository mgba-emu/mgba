/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_CORE_H
#define M_CORE_H

#include "util/common.h"

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
#include "core/directories.h"
#endif
#ifndef MINIMAL_CORE
#include "core/input.h"
#endif
#include "core/config.h"

struct VFile;
struct mRTCSource;
struct mCoreConfig;

#ifdef COLOR_16_BIT
typedef uint16_t color_t;
#define BYTES_PER_PIXEL 2
#else
typedef uint32_t color_t;
#define BYTES_PER_PIXEL 4
#endif

struct blip_t;
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

	void (*setSync)(struct mCore*, struct mCoreSync*);
	void (*loadConfig)(struct mCore*);

	void (*desiredVideoDimensions)(struct mCore*, unsigned* width, unsigned* height);
	void (*setVideoBuffer)(struct mCore*, color_t* buffer, size_t stride);

	struct blip_t* (*getAudioChannel)(struct mCore*, int ch);

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

	void (*setRTC)(struct mCore*, struct mRTCSource*);
};

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
bool mCoreLoadFile(struct mCore* core, const char* path);

bool mCoreAutoloadSave(struct mCore* core);
bool mCoreAutoloadPatch(struct mCore* core);

bool mCoreSaveState(struct mCore* core, int slot, int flags);
bool mCoreLoadState(struct mCore* core, int slot, int flags);
struct VFile* mCoreGetState(struct mCore* core, int slot, bool write);
void mCoreDeleteState(struct mCore* core, int slot);
#endif

void mCoreInitConfig(struct mCore* core, const char* port);
void mCoreLoadConfig(struct mCore* core);

#endif
