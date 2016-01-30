/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_CORE_H
#define M_CORE_H

#include "util/common.h"

struct VFile;
struct mRTCSource;

struct mCore {
	void* cpu;
	void* board;

	bool (*init)(struct mCore*);
	void (*deinit)(struct mCore*);

	void (*desiredVideoDimensions)(struct mCore*, unsigned* width, unsigned* height);
	void (*setVideoBuffer)(struct mCore*, void* buffer, size_t stride);

	bool (*isROM)(struct mCore*, struct VFile* vf);
	bool (*loadROM)(struct mCore*, struct VFile* vf, struct VFile* save, const char* fname);
	void (*unloadROM)(struct mCore*);

	bool (*loadBIOS)(struct mCore*, struct VFile* vf, int biosID);
	bool (*selectBIOS)(struct mCore*, int biosID);

	void (*reset)(struct mCore*);
	void (*runFrame)(struct mCore*);
	void (*runLoop)(struct mCore*);
	void (*step)(struct mCore*);

	void (*setKeys)(struct mCore*, uint32_t keys);

	int32_t (*frameCounter)(struct mCore*);
	int32_t (*frameCycles)(struct mCore*);
	int32_t (*frequency)(struct mCore*);

	void (*setRTC)(struct mCore*, struct mRTCSource*);
};

#endif
