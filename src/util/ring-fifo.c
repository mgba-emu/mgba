/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/ring-fifo.h>

#include <mgba-util/memory.h>

void RingFIFOInit(struct RingFIFO* buffer, size_t capacity) {
	buffer->data = anonymousMemoryMap(capacity);
	buffer->capacity = capacity;
	RingFIFOClear(buffer);
}

void RingFIFODeinit(struct RingFIFO* buffer) {
	mappedMemoryFree(buffer->data, buffer->capacity);
	buffer->data = 0;
}

size_t RingFIFOCapacity(const struct RingFIFO* buffer) {
	return buffer->capacity;
}

size_t RingFIFOSize(const struct RingFIFO* buffer) {
	const void* read;
	const void* write;
	ATOMIC_LOAD_PTR(read, buffer->readPtr);
	ATOMIC_LOAD_PTR(write, buffer->writePtr);
	if (read <= write) {
		return (uintptr_t) write - (uintptr_t) read;
	} else {
		return buffer->capacity - (uintptr_t) read + (uintptr_t) write;
	}
}

void RingFIFOClear(struct RingFIFO* buffer) {
	ATOMIC_STORE_PTR(buffer->readPtr, buffer->data);
	ATOMIC_STORE_PTR(buffer->writePtr, buffer->data);
}

size_t RingFIFOWrite(struct RingFIFO* buffer, const void* value, size_t length) {
	void* data = buffer->writePtr;
	void* end;
	ATOMIC_LOAD_PTR(end, buffer->readPtr);

	// Wrap around if we can't fit enough in here
	if ((uintptr_t) data - (uintptr_t) buffer->data + length >= buffer->capacity) {
		if (end == buffer->data || end > data) {
			// Oops! If we wrap now, it'll appear empty
			return 0;
		}
		data = buffer->data;
	}

	size_t remaining;
	if (data >= end) {
		uintptr_t bufferEnd = (uintptr_t) buffer->data + buffer->capacity;
		remaining = bufferEnd - (uintptr_t) data;
	} else {
		remaining = (uintptr_t) end - (uintptr_t) data;
	}
	// Note that we can't hit the end pointer
	if (remaining <= length) {
		return 0;
	}
	if (value) {
		memcpy(data, value, length);
	}
	ATOMIC_STORE_PTR(buffer->writePtr, (void*) ((intptr_t) data + length));
	return length;
}

size_t RingFIFORead(struct RingFIFO* buffer, void* output, size_t length) {
	void* data = buffer->readPtr;
	void* end;
	ATOMIC_LOAD_PTR(end, buffer->writePtr);

	// Wrap around if we can't fit enough in here
	if ((uintptr_t) data - (uintptr_t) buffer->data + length >= buffer->capacity) {
		if (end >= data) {
			// Oops! If we wrap now, it'll appear full
			return 0;
		}
		data = buffer->data;
	}

	size_t remaining;
	if (data > end) {
		uintptr_t bufferEnd = (uintptr_t) buffer->data + buffer->capacity;
		remaining = bufferEnd - (uintptr_t) data;
	} else {
		remaining = (uintptr_t) end - (uintptr_t) data;
	}
	// If the pointers touch, it's empty
	if (remaining < length) {
		return 0;
	}
	if (output) {
		memcpy(output, data, length);
	}
	ATOMIC_STORE_PTR(buffer->readPtr, (void*) ((uintptr_t) data + length));
	return length;
}
