/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef CIRCLE_BUFFER_H
#define CIRCLE_BUFFER_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct CircleBuffer {
	void* data;
	size_t capacity;
	size_t size;
	void* readPtr;
	void* writePtr;
};

void CircleBufferInit(struct CircleBuffer* buffer, unsigned capacity);
void CircleBufferDeinit(struct CircleBuffer* buffer);
size_t CircleBufferSize(const struct CircleBuffer* buffer);
size_t CircleBufferCapacity(const struct CircleBuffer* buffer);
void CircleBufferClear(struct CircleBuffer* buffer);
int CircleBufferWrite8(struct CircleBuffer* buffer, int8_t value);
int CircleBufferWrite16(struct CircleBuffer* buffer, int16_t value);
int CircleBufferWrite32(struct CircleBuffer* buffer, int32_t value);
size_t CircleBufferWrite(struct CircleBuffer* buffer, const void* input, size_t length);
int CircleBufferRead8(struct CircleBuffer* buffer, int8_t* value);
int CircleBufferRead16(struct CircleBuffer* buffer, int16_t* value);
int CircleBufferRead32(struct CircleBuffer* buffer, int32_t* value);
size_t CircleBufferRead(struct CircleBuffer* buffer, void* output, size_t length);
size_t CircleBufferDump(const struct CircleBuffer* buffer, void* output, size_t length);

CXX_GUARD_END

#endif
