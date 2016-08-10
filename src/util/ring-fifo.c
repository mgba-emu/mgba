/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ring-fifo.h"

#include "util/memory.h"

#ifndef _MSC_VER
#define ATOMIC_STORE(DST, SRC) __atomic_store_n(&DST, SRC, __ATOMIC_RELEASE)
#define ATOMIC_LOAD(DST, SRC) DST = __atomic_load_n(&SRC, __ATOMIC_ACQUIRE)
#else
// TODO
#define ATOMIC_STORE(DST, SRC) DST = SRC
#define ATOMIC_LOAD(DST, SRC) DST = SRC
#endif

void RingFIFOInit(struct RingFIFO* buffer, size_t capacity, size_t maxalloc) {
	buffer->data = anonymousMemoryMap(capacity);
	buffer->capacity = capacity;
	buffer->maxalloc = maxalloc;
	RingFIFOClear(buffer);
}

void RingFIFODeinit(struct RingFIFO* buffer) {
	mappedMemoryFree(buffer->data, buffer->capacity);
	buffer->data = 0;
}

size_t RingFIFOCapacity(const struct RingFIFO* buffer) {
	return buffer->capacity;
}

void RingFIFOClear(struct RingFIFO* buffer) {
	ATOMIC_STORE(buffer->readPtr, buffer->data);
	ATOMIC_STORE(buffer->writePtr, buffer->data);
}

size_t RingFIFOWrite(struct RingFIFO* buffer, const void* value, size_t length) {
	void* data = buffer->writePtr;
	void* end;
	ATOMIC_LOAD(end, buffer->readPtr);
	size_t remaining;
	if ((intptr_t) data - (intptr_t) buffer->data + buffer->maxalloc >= buffer->capacity) {
		data = buffer->data;
	}
	if (data >= end) {
		remaining = (intptr_t) buffer->data + buffer->capacity - (intptr_t) data;
	} else {
		remaining = (intptr_t) end - (intptr_t) data;
	}
	if (remaining <= length) {
		return 0;
	}
	if (value) {
		memcpy(data, value, length);
	}
	ATOMIC_STORE(buffer->writePtr, (void*) ((intptr_t) data + length));
	return length;
}

size_t RingFIFORead(struct RingFIFO* buffer, void* output, size_t length) {
	void* data = buffer->readPtr;
	void* end;
	ATOMIC_LOAD(end, buffer->writePtr);
	size_t remaining;
	if ((intptr_t) data - (intptr_t) buffer->data + buffer->maxalloc >= buffer->capacity) {
		data = buffer->data;
	}
	if (data > end) {
		remaining = (intptr_t) buffer->data + buffer->capacity - (intptr_t) data;
	} else {
		remaining = (intptr_t) end - (intptr_t) data;
	}
	if (remaining <= length) {
		return 0;
	}
	if (output) {
		memcpy(output, data, length);
	}
	ATOMIC_STORE(buffer->readPtr, (void*) ((intptr_t) data + length));
	return length;
}
