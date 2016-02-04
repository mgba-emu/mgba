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

struct VFile;
struct mRTCSource;

#ifdef COLOR_16_BIT
typedef uint16_t color_t;
#define BYTES_PER_PIXEL 2
#else
typedef uint32_t color_t;
#define BYTES_PER_PIXEL 4
#endif

struct mCoreSync;
struct mCore {
	void* cpu;
	void* board;

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	struct mDirectorySet dirs;
#endif

	bool (*init)(struct mCore*);
	void (*deinit)(struct mCore*);

	void (*setSync)(struct mCore*, struct mCoreSync*);

	void (*desiredVideoDimensions)(struct mCore*, unsigned* width, unsigned* height);
	void (*setVideoBuffer)(struct mCore*, color_t* buffer, size_t stride);

	bool (*isROM)(struct VFile* vf);
	bool (*loadROM)(struct mCore*, struct VFile* vf);
	bool (*loadSave)(struct mCore*, struct VFile* vf);
	void (*unloadROM)(struct mCore*);

	bool (*loadBIOS)(struct mCore*, struct VFile* vf, int biosID);
	bool (*selectBIOS)(struct mCore*, int biosID);

	void (*reset)(struct mCore*);
	void (*runFrame)(struct mCore*);
	void (*runLoop)(struct mCore*);
	void (*step)(struct mCore*);

	void (*setKeys)(struct mCore*, uint32_t keys);
	void (*addKeys)(struct mCore*, uint32_t keys);
	void (*clearKeys)(struct mCore*, uint32_t keys);

	int32_t (*frameCounter)(struct mCore*);
	int32_t (*frameCycles)(struct mCore*);
	int32_t (*frequency)(struct mCore*);

	void (*setRTC)(struct mCore*, struct mRTCSource*);
};

bool mCoreLoadFile(struct mCore* core, const char* path);
bool mCoreAutoloadSave(struct mCore* core);

#endif
