#include "circle-buffer.h"

#include <stddef.h>	
#include <stdlib.h>

void CircleBufferInit(struct CircleBuffer* buffer, unsigned capacity) {
	buffer->data = malloc(capacity);
	buffer->capacity = capacity;
	buffer->size = 0;
	buffer->readPtr = buffer->data;
	buffer->writePtr = buffer->data;
}

void CircleBufferDeinit(struct CircleBuffer* buffer) {
	free(buffer->data);
	buffer->data = 0;
}

unsigned CircleBufferSize(const struct CircleBuffer* buffer) {
	return buffer->size;
}

int CircleBufferWrite8(struct CircleBuffer* buffer, int8_t value) {
	int8_t* data = buffer->writePtr;
	if (buffer->size + sizeof(int8_t) > buffer->capacity) {
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
	buffer->size += sizeof(int8_t);
	return 1;
}

int CircleBufferWrite32(struct CircleBuffer* buffer, int32_t value) {
	int32_t* data = buffer->writePtr;
	if (buffer->size + sizeof(int32_t) > buffer->capacity) {
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
	buffer->size += sizeof(int32_t);
	return 1;
}

int CircleBufferRead8(struct CircleBuffer* buffer, int8_t* value) {
	int8_t* data = buffer->readPtr;
	if (buffer->size < sizeof(int8_t)) {
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
	buffer->size -= sizeof(int8_t);
	return 1;
}

int CircleBufferRead32(struct CircleBuffer* buffer, int32_t* value) {
	int32_t* data = buffer->readPtr;
	if (buffer->size < sizeof(int32_t)) {
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
	buffer->size -= sizeof(int32_t);
	return 1;
}
