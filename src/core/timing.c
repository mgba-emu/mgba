/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/timing.h>

void mTimingInit(struct mTiming* timing, int32_t* relativeCycles, int32_t* nextEvent) {
	timing->root = NULL;
	timing->reroot = NULL;
	timing->globalCycles = 0;
	timing->masterCycles = 0;
	timing->relativeCycles = relativeCycles;
	timing->nextEvent = nextEvent;
}

void mTimingDeinit(struct mTiming* timing) {
	UNUSED(timing);
}

void mTimingClear(struct mTiming* timing) {
	timing->root = NULL;
	timing->reroot = NULL;
	timing->globalCycles = 0;
	timing->masterCycles = 0;
}

void mTimingInterrupt(struct mTiming* timing) {
	if (!timing->root) {
		return;
	}
	timing->reroot = timing->root;
	timing->root = NULL;
}

void mTimingSchedule(struct mTiming* timing, struct mTimingEvent* event, int32_t when) {
	int32_t nextEvent = when + *timing->relativeCycles;
	event->when = nextEvent + timing->masterCycles;
	if (nextEvent < *timing->nextEvent) {
		*timing->nextEvent = nextEvent;
	}
	if (timing->reroot) {
		timing->root = timing->reroot;
		timing->reroot = NULL;
	}
	struct mTimingEvent** previous = &timing->root;
	struct mTimingEvent* next = timing->root;
	unsigned priority = event->priority;
	while (next) {
		int32_t nextWhen = next->when - timing->masterCycles;
		if (nextWhen > nextEvent || (nextWhen == nextEvent && next->priority > priority)) {
			break;
		}
		previous = &next->next;
		next = next->next;
	}
	event->next = next;
	*previous = event;
}

void mTimingScheduleAbsolute(struct mTiming* timing, struct mTimingEvent* event, int32_t when) {
	mTimingSchedule(timing, event, when - mTimingCurrentTime(timing));
}

void mTimingDeschedule(struct mTiming* timing, struct mTimingEvent* event) {
	if (timing->reroot) {
		timing->root = timing->reroot;
		timing->reroot = NULL;
	}
	struct mTimingEvent** previous = &timing->root;
	struct mTimingEvent* next = timing->root;
	while (next) {
		if (next == event) {
			*previous = next->next;
			return;
		}
		previous = &next->next;
		next = next->next;
	}
}

bool mTimingIsScheduled(const struct mTiming* timing, const struct mTimingEvent* event) {
	const struct mTimingEvent* next = timing->root;
	if (!next) {
		next = timing->reroot;
	}
	while (next) {
		if (next == event) {
			return true;
		}
		next = next->next;
	}
	return false;
}

int32_t mTimingTick(struct mTiming* timing, int32_t cycles) {
	timing->masterCycles += cycles;
	uint32_t masterCycles = timing->masterCycles;
	while (timing->root) {
		struct mTimingEvent* next = timing->root;
		int32_t nextWhen = next->when - masterCycles;
		if (nextWhen > 0) {
			return nextWhen;
		}
		timing->root = next->next;
		next->callback(timing, next->context, -nextWhen);
	}
	if (timing->reroot) {
		timing->root = timing->reroot;
		timing->reroot = NULL;
		*timing->nextEvent = mTimingNextEvent(timing);
		if (*timing->nextEvent <= 0) {
			return mTimingTick(timing, 0);
		}
	}
	return *timing->nextEvent;
}

int32_t mTimingCurrentTime(const struct mTiming* timing) {
	return timing->masterCycles + *timing->relativeCycles;
}

uint64_t mTimingGlobalTime(const struct mTiming* timing) {
	return timing->globalCycles + *timing->relativeCycles;
}

int32_t mTimingNextEvent(struct mTiming* timing) {
	struct mTimingEvent* next = timing->root;
	if (!next) {
		return INT_MAX;
	}
	return next->when - timing->masterCycles - *timing->relativeCycles;
}

int32_t mTimingUntil(const struct mTiming* timing, const struct mTimingEvent* event) {
	return event->when - timing->masterCycles - *timing->relativeCycles;
}
