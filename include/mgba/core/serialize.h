/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_SERIALIZE_H
#define M_SERIALIZE_H

#include <mgba-util/common.h>

CXX_GUARD_START

enum mStateExtdataTag {
	EXTDATA_NONE = 0,
	EXTDATA_SCREENSHOT = 1,
	EXTDATA_SAVEDATA = 2,
	EXTDATA_CHEATS = 3,
	EXTDATA_RTC = 4,
	EXTDATA_SCREENSHOT_DIMENSIONS = 5,
	EXTDATA_META_TIME = 0x101,
	EXTDATA_META_CREATOR = 0x102,
	EXTDATA_MAX
};

#define SAVESTATE_SCREENSHOT 1
#define SAVESTATE_SAVEDATA   2
#define SAVESTATE_CHEATS     4
#define SAVESTATE_RTC        8
#define SAVESTATE_METADATA   16
#define SAVESTATE_ALL        31

struct mStateExtdataItem {
	int32_t size;
	void* data;
	void (*clean)(void*);
};

struct mStateExtdata {
	struct mStateExtdataItem data[EXTDATA_MAX];
};

void mStateExtdataInit(struct mStateExtdata*);
void mStateExtdataDeinit(struct mStateExtdata*);
void mStateExtdataPut(struct mStateExtdata*, enum mStateExtdataTag, struct mStateExtdataItem*);
bool mStateExtdataGet(struct mStateExtdata*, enum mStateExtdataTag, struct mStateExtdataItem*);

struct VFile;
bool mStateExtdataSerialize(struct mStateExtdata* extdata, struct VFile* vf);
bool mStateExtdataDeserialize(struct mStateExtdata* extdata, struct VFile* vf);

struct mCore;
bool mCoreSaveStateNamed(struct mCore* core, struct VFile* vf, int flags);
bool mCoreLoadStateNamed(struct mCore* core, struct VFile* vf, int flags);
void* mCoreExtractState(struct mCore* core, struct VFile* vf, struct mStateExtdata* extdata);
bool mCoreExtractExtdata(struct mCore* core, struct VFile* vf, struct mStateExtdata* extdata);

CXX_GUARD_END

#endif
