/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <Cocoa/Cocoa.h>
#import <Foundation/NSUndoManager.h>
#import <unistd.h>

#import "eventpump.h"
#import <mgba/core/thread.h>
#import <mgba/gba/core.h>

static NSMutableArray *eventQueue;

void processEvents(void) {
	@synchronized(eventQueue) {
		if (eventQueue) {
			for (void(^block)() in eventQueue) {
				block();
			}
			[eventQueue removeAllObjects];
		}
	}
}

void postEvent(void(^block)()) {
	@synchronized(eventQueue) {
		[eventQueue addObject:block];
	}

	size_t objectCount = 1;

	while (objectCount) {
		@synchronized(eventQueue) {
			objectCount = [eventQueue count];
		}
		usleep(1000);
	}
}

void setupPostEvent() {
	eventQueue = [[NSMutableArray alloc] init];

	__processEvents = processEvents;
	__postEvent = postEvent;
}
