/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_CORE_TIMING
#define M_CORE_TIMING

#include "util/common.h"
#include "util/vector.h"

struct mTiming;
struct mTimingEvent {
	void* context;
	void (*callback)(struct mTiming*, void* context, uint32_t);
	const char* name;
	uint32_t when;
};

DECLARE_VECTOR(mTimingEventList, struct mTimingEvent*);

struct mTiming {
	struct mTimingEventList events;

	uint32_t masterCycles;
	int32_t* relativeCycles;
	int32_t* nextEvent;
};

void mTimingInit(struct mTiming* timing, int32_t* relativeCycles, int32_t* nextEvent);
void mTimingDeinit(struct mTiming* timing);
void mTimingClear(struct mTiming* timing);
void mTimingSchedule(struct mTiming* timing, struct mTimingEvent*, int32_t when);
void mTimingDeschedule(struct mTiming* timing, struct mTimingEvent*);
int32_t mTimingTick(struct mTiming* timing, int32_t cycles);
int32_t mTimingNextEvent(struct mTiming* timing);

#endif
