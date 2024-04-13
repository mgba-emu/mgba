/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef CIRCLE_BUFFER_H
#define CIRCLE_BUFFER_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct mCircleBuffer {
	void* data;
	size_t capacity;
	size_t size;
	void* readPtr;
	void* writePtr;
};

void mCircleBufferInit(struct mCircleBuffer* buffer, unsigned capacity);
void mCircleBufferDeinit(struct mCircleBuffer* buffer);
size_t mCircleBufferSize(const struct mCircleBuffer* buffer);
size_t mCircleBufferCapacity(const struct mCircleBuffer* buffer);
void mCircleBufferClear(struct mCircleBuffer* buffer);
int mCircleBufferWrite8(struct mCircleBuffer* buffer, int8_t value);
int mCircleBufferWrite16(struct mCircleBuffer* buffer, int16_t value);
int mCircleBufferWrite32(struct mCircleBuffer* buffer, int32_t value);
size_t mCircleBufferWrite(struct mCircleBuffer* buffer, const void* input, size_t length);
size_t mCircleBufferWriteTruncate(struct mCircleBuffer* buffer, const void* input, size_t length);
int mCircleBufferRead8(struct mCircleBuffer* buffer, int8_t* value);
int mCircleBufferRead16(struct mCircleBuffer* buffer, int16_t* value);
int mCircleBufferRead32(struct mCircleBuffer* buffer, int32_t* value);
size_t mCircleBufferRead(struct mCircleBuffer* buffer, void* output, size_t length);
size_t mCircleBufferDump(const struct mCircleBuffer* buffer, void* output, size_t length, size_t offset);

CXX_GUARD_END

#endif
