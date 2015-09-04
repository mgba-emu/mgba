/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ring-fifo.h"

void RingFIFOInit(struct RingFIFO* buffer, size_t capacity, size_t maxalloc) {
	buffer->data = anonymousMemoryMap(capacity);
	buffer->capacity = capacity;
	buffer->maxalloc = maxalloc;
	RingFIFOClear(buffer);
}

void RingFIFODeinit(struct RingFIFO* buffer) {
	memoryMapFree(buffer->data, buffer->capacity);
	buffer->data = 0;
}

size_t RingFIFOCapacity(const struct RingFIFO* buffer) {
	return buffer->capacity;
}

void RingFIFOClear(struct RingFIFO* buffer) {
	buffer->readPtr = buffer->data;
	buffer->writePtr = buffer->data;
}

size_t RingFIFOWrite(struct RingFIFO* buffer, const void* value, size_t length) {
	void* data = buffer->writePtr;
	void* end = buffer->readPtr;
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
	memcpy(data, value, length);
	buffer->writePtr = (void*) ((intptr_t) data + length);
	return length;
}

size_t RingFIFORead(struct RingFIFO* buffer, void* output, size_t length) {
	void* data = buffer->readPtr;
	void* end = buffer->writePtr;
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
	memcpy(output, data, length);
	buffer->readPtr = (void*) ((intptr_t) data + length);
	return length;
}
