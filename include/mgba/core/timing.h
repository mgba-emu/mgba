/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_CORE_TIMING
#define M_CORE_TIMING

#include <mgba-util/common.h>

CXX_GUARD_START

struct mTiming;
struct mTimingEvent {
	void* context;
	void (*callback)(struct mTiming*, void* context, uint32_t);
	const char* name;
	uint32_t when;
	unsigned priority;

	struct mTimingEvent* next;
};

struct mTiming {
	struct mTimingEvent* root;
	struct mTimingEvent* reroot;

	uint64_t globalCycles;
	uint32_t masterCycles;
	int32_t* relativeCycles;
	int32_t* nextEvent;
};

void mTimingInit(struct mTiming* timing, int32_t* relativeCycles, int32_t* nextEvent);
void mTimingDeinit(struct mTiming* timing);

void mTimingClear(struct mTiming* timing);
void mTimingInterrupt(struct mTiming* timing);

void mTimingSchedule(struct mTiming* timing, struct mTimingEvent*, int32_t when);
void mTimingScheduleAbsolute(struct mTiming* timing, struct mTimingEvent*, int32_t when);
void mTimingDeschedule(struct mTiming* timing, struct mTimingEvent*);
bool mTimingIsScheduled(const struct mTiming* timing, const struct mTimingEvent*);

int32_t mTimingTick(struct mTiming* timing, int32_t cycles);

int32_t mTimingCurrentTime(const struct mTiming* timing);
uint64_t mTimingGlobalTime(const struct mTiming* timing);

int32_t mTimingNextEvent(struct mTiming* timing);
int32_t mTimingUntil(const struct mTiming* timing, const struct mTimingEvent*);

CXX_GUARD_END

#endif
