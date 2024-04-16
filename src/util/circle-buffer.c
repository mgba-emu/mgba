/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/circle-buffer.h>

#ifndef NDEBUG
static int _checkIntegrity(struct mCircleBuffer* buffer) {
	if ((int8_t*) buffer->writePtr - (int8_t*) buffer->readPtr == (ssize_t) buffer->size) {
		return 1;
	}
	if ((ssize_t) (buffer->capacity - buffer->size) == ((int8_t*) buffer->writePtr - (int8_t*) buffer->readPtr)) {
		return 1;
	}
	if ((ssize_t) (buffer->capacity - buffer->size) == ((int8_t*) buffer->readPtr - (int8_t*) buffer->writePtr)) {
		return 1;
	}
	return 0;
}
#endif

void mCircleBufferInit(struct mCircleBuffer* buffer, unsigned capacity) {
	buffer->data = malloc(capacity);
	buffer->capacity = capacity;
	mCircleBufferClear(buffer);
}

void mCircleBufferDeinit(struct mCircleBuffer* buffer) {
	free(buffer->data);
	buffer->data = 0;
}

size_t mCircleBufferSize(const struct mCircleBuffer* buffer) {
	return buffer->size;
}

size_t mCircleBufferCapacity(const struct mCircleBuffer* buffer) {
	return buffer->capacity;
}

void mCircleBufferClear(struct mCircleBuffer* buffer) {
	buffer->size = 0;
	buffer->readPtr = buffer->data;
	buffer->writePtr = buffer->data;
}

int mCircleBufferWrite8(struct mCircleBuffer* buffer, int8_t value) {
	int8_t* data = buffer->writePtr;
	if (buffer->size + sizeof(int8_t) > buffer->capacity) {
		return 0;
	}
	*data = value;
	++data;
	size_t size = (int8_t*) data - (int8_t*) buffer->data;
	if (size < buffer->capacity) {
		buffer->writePtr = data;
	} else {
		buffer->writePtr = buffer->data;
	}
	buffer->size += sizeof(int8_t);
#ifndef NDEBUG
	if (!_checkIntegrity(buffer)) {
		abort();
	}
#endif
	return 1;
}

int mCircleBufferWrite32(struct mCircleBuffer* buffer, int32_t value) {
	int32_t* data = buffer->writePtr;
	if (buffer->size + sizeof(int32_t) > buffer->capacity) {
		return 0;
	}
	if (((intptr_t) data & 0x3) || (uintptr_t) data - (uintptr_t) buffer->data > buffer->capacity - sizeof(int32_t)) {
		int written = 0;
		written += mCircleBufferWrite8(buffer, ((int8_t*) &value)[0]);
		written += mCircleBufferWrite8(buffer, ((int8_t*) &value)[1]);
		written += mCircleBufferWrite8(buffer, ((int8_t*) &value)[2]);
		written += mCircleBufferWrite8(buffer, ((int8_t*) &value)[3]);
		return written;
	}
	*data = value;
	++data;
	size_t size = (int8_t*) data - (int8_t*) buffer->data;
	if (size < buffer->capacity) {
		buffer->writePtr = data;
	} else {
		buffer->writePtr = buffer->data;
	}
	buffer->size += sizeof(int32_t);
#ifndef NDEBUG
	if (!_checkIntegrity(buffer)) {
		abort();
	}
#endif
	return 4;
}

int mCircleBufferWrite16(struct mCircleBuffer* buffer, int16_t value) {
	int16_t* data = buffer->writePtr;
	if (buffer->size + sizeof(int16_t) > buffer->capacity) {
		return 0;
	}
	if (((intptr_t) data & 0x1) || (uintptr_t) data - (uintptr_t) buffer->data > buffer->capacity - sizeof(int16_t)) {
		int written = 0;
		written += mCircleBufferWrite8(buffer, ((int8_t*) &value)[0]);
		written += mCircleBufferWrite8(buffer, ((int8_t*) &value)[1]);
		return written;
	}
	*data = value;
	++data;
	size_t size = (int8_t*) data - (int8_t*) buffer->data;
	if (size < buffer->capacity) {
		buffer->writePtr = data;
	} else {
		buffer->writePtr = buffer->data;
	}
	buffer->size += sizeof(int16_t);
#ifndef NDEBUG
	if (!_checkIntegrity(buffer)) {
		abort();
	}
#endif
	return 2;
}

size_t mCircleBufferWrite(struct mCircleBuffer* buffer, const void* input, size_t length) {
	int8_t* data = buffer->writePtr;
	if (buffer->size + length > buffer->capacity) {
		return 0;
	}
	size_t remaining = buffer->capacity - ((int8_t*) data - (int8_t*) buffer->data);
	if (length <= remaining) {
		memcpy(data, input, length);
		if (length == remaining) {
			buffer->writePtr = buffer->data;
		} else {
			buffer->writePtr = (int8_t*) data + length;
		}
	} else {
		memcpy(data, input, remaining);
		memcpy(buffer->data, (const int8_t*) input + remaining, length - remaining);
		buffer->writePtr = (int8_t*) buffer->data + length - remaining;
	}

	buffer->size += length;
#ifndef NDEBUG
	if (!_checkIntegrity(buffer)) {
		abort();
	}
#endif
	return length;
}

size_t mCircleBufferWriteTruncate(struct mCircleBuffer* buffer, const void* input, size_t length) {
	if (buffer->size + length > buffer->capacity) {
		length = buffer->capacity - buffer->size;
	}
	return mCircleBufferWrite(buffer, input, length);
}

int mCircleBufferRead8(struct mCircleBuffer* buffer, int8_t* value) {
	int8_t* data = buffer->readPtr;
	if (buffer->size < sizeof(int8_t)) {
		return 0;
	}
	*value = *data;
	++data;
	size_t size = (int8_t*) data - (int8_t*) buffer->data;
	if (size < buffer->capacity) {
		buffer->readPtr = data;
	} else {
		buffer->readPtr = buffer->data;
	}
	buffer->size -= sizeof(int8_t);
#ifndef NDEBUG
	if (!_checkIntegrity(buffer)) {
		abort();
	}
#endif
	return 1;
}

int mCircleBufferRead16(struct mCircleBuffer* buffer, int16_t* value) {
	int16_t* data = buffer->readPtr;
	if (buffer->size < sizeof(int16_t)) {
		return 0;
	}
	if (((intptr_t) data & 0x1) || (uintptr_t) data - (uintptr_t) buffer->data > buffer->capacity - sizeof(int16_t)) {
		int read = 0;
		read += mCircleBufferRead8(buffer, &((int8_t*) value)[0]);
		read += mCircleBufferRead8(buffer, &((int8_t*) value)[1]);
		return read;
	}
	*value = *data;
	++data;
	size_t size = (int8_t*) data - (int8_t*) buffer->data;
	if (size < buffer->capacity) {
		buffer->readPtr = data;
	} else {
		buffer->readPtr = buffer->data;
	}
	buffer->size -= sizeof(int16_t);
#ifndef NDEBUG
	if (!_checkIntegrity(buffer)) {
		abort();
	}
#endif
	return 2;
}

int mCircleBufferRead32(struct mCircleBuffer* buffer, int32_t* value) {
	int32_t* data = buffer->readPtr;
	if (buffer->size < sizeof(int32_t)) {
		return 0;
	}
	if (((intptr_t) data & 0x3) || (uintptr_t) data - (uintptr_t) buffer->data > buffer->capacity - sizeof(int32_t)) {
		int read = 0;
		read += mCircleBufferRead8(buffer, &((int8_t*) value)[0]);
		read += mCircleBufferRead8(buffer, &((int8_t*) value)[1]);
		read += mCircleBufferRead8(buffer, &((int8_t*) value)[2]);
		read += mCircleBufferRead8(buffer, &((int8_t*) value)[3]);
		return read;
	}
	*value = *data;
	++data;
	size_t size = (int8_t*) data - (int8_t*) buffer->data;
	if (size < buffer->capacity) {
		buffer->readPtr = data;
	} else {
		buffer->readPtr = buffer->data;
	}
	buffer->size -= sizeof(int32_t);
#ifndef NDEBUG
	if (!_checkIntegrity(buffer)) {
		abort();
	}
#endif
	return 4;
}

size_t mCircleBufferRead(struct mCircleBuffer* buffer, void* output, size_t length) {
	int8_t* data = buffer->readPtr;
	if (buffer->size == 0) {
		return 0;
	}
	if (length > buffer->size) {
		length = buffer->size;
	}
	size_t remaining = buffer->capacity - ((int8_t*) data - (int8_t*) buffer->data);
	if (length <= remaining) {
		if (output) {
			memcpy(output, data, length);
		}
		if (length == remaining) {
			buffer->readPtr = buffer->data;
		} else {
			buffer->readPtr = (int8_t*) data + length;
		}
	} else {
		if (output) {
			memcpy(output, data, remaining);
			memcpy((int8_t*) output + remaining, buffer->data, length - remaining);
		}
		buffer->readPtr = (int8_t*) buffer->data + length - remaining;
	}

	buffer->size -= length;
#ifndef NDEBUG
	if (!_checkIntegrity(buffer)) {
		abort();
	}
#endif
	return length;
}

size_t mCircleBufferDump(const struct mCircleBuffer* buffer, void* output, size_t length, size_t offset) {
	int8_t* data = buffer->readPtr;
	if (buffer->size <= offset) {
		return 0;
	}
	if (length > buffer->size - offset) {
		length = buffer->size - offset;
	}
	size_t remaining = buffer->capacity - ((uintptr_t) data - (uintptr_t) buffer->data);
	if (offset) {
		if (remaining >= offset) {
			data += offset;
			remaining -= offset;
		} else {
			offset -= remaining;
			data = buffer->data;
			data += offset;
		}
	}

	if (length <= remaining) {
		memcpy(output, data, length);
	} else {
		memcpy(output, data, remaining);
		memcpy((int8_t*) output + remaining, buffer->data, length - remaining);
	}

	return length;
}
