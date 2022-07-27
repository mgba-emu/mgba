/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/interface.h>

#include <mgba/core/core.h>
#include <mgba/core/serialize.h>

DEFINE_VECTOR(mCoreCallbacksList, struct mCoreCallbacks);

static void _rtcGenericSample(struct mRTCSource* source) {
	struct mRTCGenericSource* rtc = (struct mRTCGenericSource*) source;
	switch (rtc->override) {
	default:
		if (rtc->custom->sample) {
			return rtc->custom->sample(rtc->custom);
		}
		break;
	case RTC_NO_OVERRIDE:
	case RTC_FIXED:
	case RTC_FAKE_EPOCH:
	case RTC_WALLCLOCK_OFFSET:
		break;
	}
}

static time_t _rtcGenericCallback(struct mRTCSource* source) {
	struct mRTCGenericSource* rtc = (struct mRTCGenericSource*) source;
	switch (rtc->override) {
	default:
		if (rtc->custom->unixTime) {
			return rtc->custom->unixTime(rtc->custom);
		}
		// Fall through
	case RTC_NO_OVERRIDE:
		return time(0);
	case RTC_FIXED:
		return rtc->value / 1000LL;
	case RTC_FAKE_EPOCH:
		return (rtc->value + rtc->p->frameCounter(rtc->p) * (rtc->p->frameCycles(rtc->p) * 1000LL) / rtc->p->frequency(rtc->p)) / 1000LL;
	case RTC_WALLCLOCK_OFFSET:
		return time(0) + rtc->value / 1000LL;
	}
}

static void _rtcGenericSerialize(struct mRTCSource* source, struct mStateExtdataItem* item) {
	struct mRTCGenericSource* rtc = (struct mRTCGenericSource*) source;
	struct mRTCGenericState state = {
		.type = rtc->override,
		.padding = 0,
		.value = rtc->value
	};
	void* data;
	if (rtc->override >= RTC_CUSTOM_START && rtc->custom->serialize) {
		rtc->custom->serialize(rtc->custom, item);
		data = malloc(item->size + sizeof(state));
		uint8_t* oldData = data;
		oldData += sizeof(state);
		memcpy(oldData, item->data, item->size);
		item->size += sizeof(state);
		if (item->clean) {
			item->clean(item->data);
		}
	} else {
		item->size = sizeof(state);
		data = malloc(item->size);
	}
	memcpy(data, &state, sizeof(state));
	item->data = data;
	item->clean = free;
}

static bool _rtcGenericDeserialize(struct mRTCSource* source, const struct mStateExtdataItem* item) {
	struct mRTCGenericSource* rtc = (struct mRTCGenericSource*) source;
	struct mRTCGenericState* state = item->data;
	if (!state || item->size < (ssize_t) sizeof(*state)) {
		return false;
	}
	if (state->type >= RTC_CUSTOM_START) {
		if (!rtc->custom) {
			return false;
		}
		if (rtc->custom->deserialize) {
			uint8_t* oldData = item->data;
			oldData += sizeof(state);
			struct mStateExtdataItem fakeItem = {
				.size = item->size - sizeof(*state),
				.data = oldData,
				.clean = NULL
			};
			if (!rtc->custom->deserialize(rtc->custom, &fakeItem)) {
				return false;
			}
		}
	}
	rtc->value = state->value;
	rtc->override = state->type;
	return true;
}

void mRTCGenericSourceInit(struct mRTCGenericSource* rtc, struct mCore* core) {
	rtc->p = core;
	rtc->override = RTC_NO_OVERRIDE;
	rtc->value = 0;
	rtc->d.sample = _rtcGenericSample;
	rtc->d.unixTime = _rtcGenericCallback;
	rtc->d.serialize = _rtcGenericSerialize;
	rtc->d.deserialize = _rtcGenericDeserialize;
}
