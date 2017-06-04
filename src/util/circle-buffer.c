/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/circle-buffer.h>

#ifndef NDEBUG
static int _checkIntegrity(struct CircleBuffer* buffer) {
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

void CircleBufferInit(struct CircleBuffer* buffer, unsigned capacity) {
	buffer->data = malloc(capacity);
	buffer->capacity = capacity;
	CircleBufferClear(buffer);
}

void CircleBufferDeinit(struct CircleBuffer* buffer) {
	free(buffer->data);
	buffer->data = 0;
}

size_t CircleBufferSize(const struct CircleBuffer* buffer) {
	return buffer->size;
}

size_t CircleBufferCapacity(const struct CircleBuffer* buffer) {
	return buffer->capacity;
}

void CircleBufferClear(struct CircleBuffer* buffer) {
	buffer->size = 0;
	buffer->readPtr = buffer->data;
	buffer->writePtr = buffer->data;
}

int CircleBufferWrite8(struct CircleBuffer* buffer, int8_t value) {
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

int CircleBufferWrite32(struct CircleBuffer* buffer, int32_t value) {
	int32_t* data = buffer->writePtr;
	if (buffer->size + sizeof(int32_t) > buffer->capacity) {
		return 0;
	}
	if ((intptr_t) data & 0x3) {
		int written = 0;
		written += CircleBufferWrite8(buffer, ((int8_t*) &value)[0]);
		written += CircleBufferWrite8(buffer, ((int8_t*) &value)[1]);
		written += CircleBufferWrite8(buffer, ((int8_t*) &value)[2]);
		written += CircleBufferWrite8(buffer, ((int8_t*) &value)[3]);
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

int CircleBufferWrite16(struct CircleBuffer* buffer, int16_t value) {
	int16_t* data = buffer->writePtr;
	if (buffer->size + sizeof(int16_t) > buffer->capacity) {
		return 0;
	}
	if ((intptr_t) data & 0x3) {
		int written = 0;
		written += CircleBufferWrite8(buffer, ((int8_t*) &value)[0]);
		written += CircleBufferWrite8(buffer, ((int8_t*) &value)[1]);
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

size_t CircleBufferWrite(struct CircleBuffer* buffer, const void* input, size_t length) {
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

int CircleBufferRead8(struct CircleBuffer* buffer, int8_t* value) {
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

int CircleBufferRead16(struct CircleBuffer* buffer, int16_t* value) {
	int16_t* data = buffer->readPtr;
	if (buffer->size < sizeof(int16_t)) {
		return 0;
	}
	if ((intptr_t) data & 0x3) {
		int read = 0;
		read += CircleBufferRead8(buffer, &((int8_t*) value)[0]);
		read += CircleBufferRead8(buffer, &((int8_t*) value)[1]);
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

int CircleBufferRead32(struct CircleBuffer* buffer, int32_t* value) {
	int32_t* data = buffer->readPtr;
	if (buffer->size < sizeof(int32_t)) {
		return 0;
	}
	if ((intptr_t) data & 0x3) {
		int read = 0;
		read += CircleBufferRead8(buffer, &((int8_t*) value)[0]);
		read += CircleBufferRead8(buffer, &((int8_t*) value)[1]);
		read += CircleBufferRead8(buffer, &((int8_t*) value)[2]);
		read += CircleBufferRead8(buffer, &((int8_t*) value)[3]);
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

size_t CircleBufferRead(struct CircleBuffer* buffer, void* output, size_t length) {
	int8_t* data = buffer->readPtr;
	if (buffer->size == 0) {
		return 0;
	}
	if (length > buffer->size) {
		length = buffer->size;
	}
	size_t remaining = buffer->capacity - ((int8_t*) data - (int8_t*) buffer->data);
	if (length <= remaining) {
		memcpy(output, data, length);
		if (length == remaining) {
			buffer->readPtr = buffer->data;
		} else {
			buffer->readPtr = (int8_t*) data + length;
		}
	} else {
		memcpy(output, data, remaining);
		memcpy((int8_t*) output + remaining, buffer->data, length - remaining);
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

size_t CircleBufferDump(const struct CircleBuffer* buffer, void* output, size_t length) {
	int8_t* data = buffer->readPtr;
	if (buffer->size == 0) {
		return 0;
	}
	if (length > buffer->size) {
		length = buffer->size;
	}
	size_t remaining = buffer->capacity - ((int8_t*) data - (int8_t*) buffer->data);
	if (length <= remaining) {
		memcpy(output, data, length);
	} else {
		memcpy(output, data, remaining);
		memcpy((int8_t*) output + remaining, buffer->data, length - remaining);
	}

	return length;
}
