/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef CORE_INTERFACE_H
#define CORE_INTERFACE_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba-util/image.h>
#include <mgba-util/vector.h>

struct mCore;
struct mStateExtdataItem;

struct blip_t;

enum mCoreFeature {
	mCORE_FEATURE_OPENGL = 1,
};

struct mCoreCallbacks {
	void* context;
	void (*videoFrameStarted)(void* context);
	void (*videoFrameEnded)(void* context);
	void (*coreCrashed)(void* context);
	void (*sleep)(void* context);
	void (*shutdown)(void* context);
	void (*keysRead)(void* context);
	void (*savedataUpdated)(void* context);
	void (*alarm)(void* context);
};

DECLARE_VECTOR(mCoreCallbacksList, struct mCoreCallbacks);

struct mAVStream {
	void (*videoDimensionsChanged)(struct mAVStream*, unsigned width, unsigned height);
	void (*audioRateChanged)(struct mAVStream*, unsigned rate);
	void (*postVideoFrame)(struct mAVStream*, const color_t* buffer, size_t stride);
	void (*postAudioFrame)(struct mAVStream*, int16_t left, int16_t right);
	void (*postAudioBuffer)(struct mAVStream*, struct blip_t* left, struct blip_t* right);
};

struct mStereoSample {
	int16_t left;
	int16_t right;
};

struct mKeyCallback {
	uint16_t (*readKeys)(struct mKeyCallback*);
	bool requireOpposingDirections;
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
	RTC_WALLCLOCK_OFFSET,
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
	mCORE_MEMORY_WORM = 0x04,
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

struct mCoreScreenRegion {
	size_t id;
	const char* description;
	int16_t x;
	int16_t y;
	int16_t w;
	int16_t h;
};

enum mCoreRegisterType {
	mCORE_REGISTER_GPR = 0,
	mCORE_REGISTER_FPR,
	mCORE_REGISTER_FLAGS,
	mCORE_REGISTER_SIMD,
};

struct mCoreRegisterInfo {
	const char* name;
	const char** aliases;
	unsigned width;
	uint32_t mask;
	enum mCoreRegisterType type;
};

CXX_GUARD_END

#endif
