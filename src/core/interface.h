/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef CORE_INTERFACE_H
#define CORE_INTERFACE_H

#include "util/common.h"

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
};

enum mRTCGenericType {
	RTC_NO_OVERRIDE,
	RTC_FIXED,
	RTC_FAKE_EPOCH
};

struct mRTCGenericSource {
	struct mRTCSource d;
	struct mCore* p;
	enum mRTCGenericType override;
	int64_t value;
};

void mRTCGenericSourceInit(struct mRTCGenericSource* rtc, struct mCore* core);

struct mRumble {
	void (*setRumble)(struct mRumble*, int enable);
};

#endif
