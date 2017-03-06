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

struct blip_t;

struct mCoreCallbacks {
	void* context;
	void (*videoFrameStarted)(void* context);
	void (*videoFrameEnded)(void* context);
	void (*coreCrashed)(void* context);
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

struct mStopCallback {
	void (*stop)(struct mStopCallback*);
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

CXX_GUARD_END

#endif
