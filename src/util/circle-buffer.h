#ifndef CIRCLE_BUFFER_H
#define CIRCLE_BUFFER_H

#include <stdint.h>

struct CircleBuffer {
	void* data;
	unsigned capacity;
	void* readPtr;
	void* writePtr;
};

void CircleBufferInit(struct CircleBuffer* buffer, unsigned capacity);
void CircleBufferDeinit(struct CircleBuffer* buffer);
unsigned CircleBufferSize(const struct CircleBuffer* buffer);
int CircleBufferWrite32(struct CircleBuffer* buffer, int32_t value);
int CircleBufferRead32(struct CircleBuffer* buffer, int32_t* value);

#endif
