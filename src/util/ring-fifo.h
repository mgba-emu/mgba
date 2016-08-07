/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef RING_FIFO_H
#define RING_FIFO_H

#include "util/common.h"

struct RingFIFO {
	void* data;
	size_t capacity;
	size_t maxalloc;
	void* readPtr;
	void* writePtr;
};

void RingFIFOInit(struct RingFIFO* buffer, size_t capacity, size_t maxalloc);
void RingFIFODeinit(struct RingFIFO* buffer);
size_t RingFIFOCapacity(const struct RingFIFO* buffer);
void RingFIFOClear(struct RingFIFO* buffer);
size_t RingFIFOWrite(struct RingFIFO* buffer, const void* value, size_t length);
size_t RingFIFORead(struct RingFIFO* buffer, void* output, size_t length);

#endif
