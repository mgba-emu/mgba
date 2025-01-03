/* Copyright (c) 2013-2024 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/audio-buffer.h>

#include <mgba-util/memory.h>

void mAudioBufferInit(struct mAudioBuffer* buffer, size_t capacity, unsigned channels) {
	mCircleBufferInit(&buffer->data, capacity * channels * sizeof(int16_t));
	buffer->channels = channels;
}

void mAudioBufferDeinit(struct mAudioBuffer* buffer) {
	mCircleBufferDeinit(&buffer->data);
}

size_t mAudioBufferAvailable(const struct mAudioBuffer* buffer) {
	return mCircleBufferSize(&buffer->data) / (buffer->channels * sizeof(int16_t));
}

size_t mAudioBufferCapacity(const struct mAudioBuffer* buffer) {
	return mCircleBufferCapacity(&buffer->data) / (buffer->channels * sizeof(int16_t));
}

void mAudioBufferClear(struct mAudioBuffer* buffer) {
	mCircleBufferClear(&buffer->data);
}

int16_t mAudioBufferPeek(const struct mAudioBuffer* buffer, unsigned channel, size_t offset) {
	int16_t sample;
	if (!mCircleBufferDump(&buffer->data, &sample, sizeof(int16_t), (offset * buffer->channels + channel) * sizeof(int16_t))) {
		return 0;
	}
	return sample;
}

size_t mAudioBufferDump(const struct mAudioBuffer* buffer, int16_t* samples, size_t count, size_t offset) {
	return mCircleBufferDump(&buffer->data,
	                         samples,
	                         count * buffer->channels * sizeof(int16_t),
	                         offset * buffer->channels * sizeof(int16_t)) /
	       (buffer->channels * sizeof(int16_t));
}

size_t mAudioBufferRead(struct mAudioBuffer* buffer, int16_t* samples, size_t count) {
	return mCircleBufferRead(&buffer->data, samples, count * buffer->channels * sizeof(int16_t)) /
	       (buffer->channels * sizeof(int16_t));
}

size_t mAudioBufferWrite(struct mAudioBuffer* buffer, const int16_t* samples, size_t count) {
	size_t free = mCircleBufferCapacity(&buffer->data) - mCircleBufferSize(&buffer->data);
	if (count * buffer->channels * sizeof(int16_t) > free) {
		if (!free) {
			return 0;
		}
		count = free / (buffer->channels * sizeof(int16_t));
	}
	return mCircleBufferWrite(&buffer->data, samples, count * buffer->channels * sizeof(int16_t)) /
	       (buffer->channels * sizeof(int16_t));
}
