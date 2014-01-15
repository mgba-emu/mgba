#ifndef CIRCLE_BUFFER_H
#define CIRCLE_BUFFER_H

#include <stdint.h>
#include <string.h>

struct CircleBuffer {
	void* data;
	unsigned capacity;
	unsigned size;
	void* readPtr;
	void* writePtr;
};

void CircleBufferInit(struct CircleBuffer* buffer, unsigned capacity);
void CircleBufferDeinit(struct CircleBuffer* buffer);
unsigned CircleBufferSize(const struct CircleBuffer* buffer);
int CircleBufferWrite8(struct CircleBuffer* buffer, int8_t value);
int CircleBufferWrite32(struct CircleBuffer* buffer, int32_t value);
int CircleBufferRead8(struct CircleBuffer* buffer, int8_t* value);
int CircleBufferRead32(struct CircleBuffer* buffer, int32_t* value);
int CircleBufferRead(struct CircleBuffer* buffer, void* output, size_t length);

#endif
