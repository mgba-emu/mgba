#include "circle-buffer.h"

#include <stddef.h>	
#include <stdlib.h>

void CircleBufferInit(struct CircleBuffer* buffer, unsigned capacity) {
	buffer->data = malloc(capacity);
	buffer->capacity = capacity;
	buffer->readPtr = buffer->data;
	buffer->writePtr = (int8_t*) buffer->data + capacity;
}

void CircleBufferDeinit(struct CircleBuffer* buffer) {
	free(buffer->data);
	buffer->data = 0;
}

unsigned CircleBufferSize(const struct CircleBuffer* buffer) {
	ptrdiff_t size = (int8_t*) buffer->readPtr - (int8_t*) buffer->writePtr;
	if (size < 0) {
		return buffer->capacity - size;
	}
	return size;
}

int CircleBufferWrite32(struct CircleBuffer* buffer, int32_t value) {
	uint32_t* data = buffer->writePtr;
	if ((int8_t*) buffer->writePtr + 1 == buffer->readPtr) {
		return 0;
	}
	if ((int8_t*) buffer->writePtr == (int8_t*) buffer->data + buffer->capacity - 1 && buffer->readPtr == buffer->data) {
		return 0;
	}
	*data = value;
	++data;
	ptrdiff_t size = (int8_t*) data - (int8_t*) buffer->data;
	if (size < buffer->capacity) {
		buffer->writePtr = data;
	} else {
		buffer->writePtr = buffer->data;
	}
	return 1;
}

int CircleBufferRead32(struct CircleBuffer* buffer, int32_t* value) {
	uint32_t* data = buffer->readPtr;
	if (buffer->readPtr == buffer->writePtr) {
		return 0;
	}
	*value = *data;
	++data;
	ptrdiff_t size = (int8_t*) data - (int8_t*) buffer->data;
	if (size < buffer->capacity) {
		buffer->readPtr = data;
	} else {
		buffer->readPtr = buffer->data;
	}
	return 1;
}
