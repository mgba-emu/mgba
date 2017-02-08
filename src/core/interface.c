/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/interface.h>

#include <mgba/core/core.h>

DEFINE_VECTOR(mCoreCallbacksList, struct mCoreCallbacks);

static time_t _rtcGenericCallback(struct mRTCSource* source) {
	struct mRTCGenericSource* rtc = (struct mRTCGenericSource*) source;
	switch (rtc->override) {
	case RTC_NO_OVERRIDE:
	default:
		return time(0);
	case RTC_FIXED:
		return rtc->value;
	case RTC_FAKE_EPOCH:
		return rtc->value + rtc->p->frameCounter(rtc->p) * (int64_t) rtc->p->frameCycles(rtc->p) / rtc->p->frequency(rtc->p);
	}
}

void mRTCGenericSourceInit(struct mRTCGenericSource* rtc, struct mCore* core) {
	rtc->p = core;
	rtc->override = RTC_NO_OVERRIDE;
	rtc->value = 0;
	rtc->d.sample = 0;
	rtc->d.unixTime = _rtcGenericCallback;
}
