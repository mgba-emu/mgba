/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef CORE_INTERFACE_H
#define CORE_INTERFACE_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba-util/vector.h>

struct mCore;
struct mStateExtdataItem;

#ifdef COLOR_16_BIT
typedef uint16_t color_t;
#define BYTES_PER_PIXEL 2
#else
typedef uint32_t color_t;
#define BYTES_PER_PIXEL 4
#endif

#define M_R5(X) ((X) & 0x1F)
#define M_G5(X) (((X) >> 5) & 0x1F)
#define M_B5(X) (((X) >> 10) & 0x1F)

#define M_R8(X) (((((X) << 3) & 0xF8) * 0x21) >> 5)
#define M_G8(X) (((((X) >> 2) & 0xF8) * 0x21) >> 5)
#define M_B8(X) (((((X) >> 7) & 0xF8) * 0x21) >> 5)

#define M_RGB5_TO_BGR8(X) ((M_R5(X) << 3) | (M_G5(X) << 11) | (M_B5(X) << 19))
#define M_RGB5_TO_RGB8(X) ((M_R5(X) << 19) | (M_G5(X) << 11) | (M_B5(X) << 3))
#define M_RGB8_TO_BGR5(X) ((((X) & 0xF8) >> 3) | (((X) & 0xF800) >> 6) | (((X) & 0xF80000) >> 9))
#define M_RGB8_TO_RGB5(X) ((((X) & 0xF8) << 7) | (((X) & 0xF800) >> 6) | (((X) & 0xF80000) >> 19))

#ifndef PYCPARSE
static inline color_t mColorFrom555(uint16_t value) {
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	color_t color = 0;
	color |= (value & 0x001F) << 11;
	color |= (value & 0x03E0) << 1;
	color |= (value & 0x7C00) >> 10;
#else
	color_t color = value;
#endif
#else
	color_t color = M_RGB5_TO_BGR8(value);
	color |= (color >> 5) & 0x070707;
#endif
	return color;
}
#endif

struct blip_t;

enum mColorFormat {
	mCOLOR_XBGR8  = 0x00001,
	mCOLOR_XRGB8  = 0x00002,
	mCOLOR_BGRX8  = 0x00004,
	mCOLOR_RGBX8  = 0x00008,
	mCOLOR_ABGR8  = 0x00010,
	mCOLOR_ARGB8  = 0x00020,
	mCOLOR_BGRA8  = 0x00040,
	mCOLOR_RGBA8  = 0x00080,
	mCOLOR_RGB5   = 0x00100,
	mCOLOR_BGR5   = 0x00200,
	mCOLOR_RGB565 = 0x00400,
	mCOLOR_BGR565 = 0x00800,
	mCOLOR_ARGB5  = 0x01000,
	mCOLOR_ABGR5  = 0x02000,
	mCOLOR_RGBA5  = 0x04000,
	mCOLOR_BGRA5  = 0x08000,
	mCOLOR_RGB8   = 0x10000,
	mCOLOR_BGR8   = 0x20000,

	mCOLOR_ANY    = -1
};

enum mCoreFeature {
	mCORE_FEATURE_OPENGL = 1,
};

struct mCoreCallbacks {
	void* context;
	void (*videoFrameStarted)(void* context);
	void (*videoFrameEnded)(void* context);
	void (*coreCrashed)(void* context);
	void (*sleep)(void* context);
	void (*keysRead)(void* context);
};

DECLARE_VECTOR(mCoreCallbacksList, struct mCoreCallbacks);

struct mAVStream {
	void (*videoDimensionsChanged)(struct mAVStream*, unsigned width, unsigned height);
	void (*postVideoFrame)(struct mAVStream*, const color_t* buffer, size_t stride);
	void (*postAudioFrame)(struct mAVStream*, int16_t left, int16_t right);
	void (*postAudioBuffer)(struct mAVStream*, struct blip_t* left, struct blip_t* right);
};

struct mKeyCallback {
	uint16_t (*readKeys)(struct mKeyCallback*);
};

enum mPeripheral {
	mPERIPH_ROTATION = 1,
	mPERIPH_RUMBLE,
	mPERIPH_IMAGE_SOURCE,
	mPERIPH_CUSTOM = 0x1000
};

struct mRotationSource {
	void (*sample)(struct mRotationSource*);

	int32_t (*readTiltX)(struct mRotationSource*);
	int32_t (*readTiltY)(struct mRotationSource*);

	int32_t (*readGyroZ)(struct mRotationSource*);
};

struct mRTCSource {
	void (*sample)(struct mRTCSource*);

	time_t (*unixTime)(struct mRTCSource*);

	void (*serialize)(struct mRTCSource*, struct mStateExtdataItem*);
	bool (*deserialize)(struct mRTCSource*, const struct mStateExtdataItem*);
};

struct mImageSource {
	void (*startRequestImage)(struct mImageSource*, unsigned w, unsigned h, int colorFormats);
	void (*stopRequestImage)(struct mImageSource*);
	void (*requestImage)(struct mImageSource*, const void** buffer, size_t* stride, enum mColorFormat* colorFormat);
};

enum mRTCGenericType {
	RTC_NO_OVERRIDE,
	RTC_FIXED,
	RTC_FAKE_EPOCH,
	RTC_CUSTOM_START = 0x1000
};

struct mRTCGenericSource {
	struct mRTCSource d;
	struct mCore* p;
	enum mRTCGenericType override;
	int64_t value;
	struct mRTCSource* custom;
};

struct mRTCGenericState {
	int32_t type;
	int32_t padding;
	int64_t value;
};

void mRTCGenericSourceInit(struct mRTCGenericSource* rtc, struct mCore* core);

struct mRumble {
	void (*setRumble)(struct mRumble*, int enable);
};

struct mCoreChannelInfo {
	size_t id;
	const char* internalName;
	const char* visibleName;
	const char* visibleType;
};

enum mCoreMemoryBlockFlags {
	mCORE_MEMORY_READ = 0x01,
	mCORE_MEMORY_WRITE = 0x02,
	mCORE_MEMORY_RW = 0x03,
	mCORE_MEMORY_MAPPED = 0x10,
	mCORE_MEMORY_VIRTUAL = 0x20,
};

struct mCoreMemoryBlock {
	size_t id;
	const char* internalName;
	const char* shortName;
	const char* longName;
	uint32_t start;
	uint32_t end;
	uint32_t size;
	uint32_t flags;
	uint16_t maxSegment;
	uint32_t segmentStart;
};

CXX_GUARD_END

#endif
