/* Copyright (c) 2013-2024 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_AUDIO_RESAMPLER_H
#define M_AUDIO_RESAMPLER_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba-util/interpolator.h>

struct mAudioBuffer;
struct mAudioResampler {
	struct mAudioBuffer* source;
	struct mAudioBuffer* destination;
	double sourceRate;
	double destRate;
	double timestamp;
	double lowWaterMark;
	double highWaterMark;
	enum mInterpolatorType interpType;
	union {
		struct mInterpolator interp;
		struct mInterpolatorSinc sinc;
		struct mInterpolatorCosine cosine;
	};
	bool consume;
};

void mAudioResamplerInit(struct mAudioResampler*, enum mInterpolatorType);
void mAudioResamplerDeinit(struct mAudioResampler*);
void mAudioResamplerSetSource(struct mAudioResampler*, struct mAudioBuffer* source, double rate, bool consume);
void mAudioResamplerSetDestination(struct mAudioResampler*, struct mAudioBuffer* destination, double rate);
size_t mAudioResamplerProcess(struct mAudioResampler*);

CXX_GUARD_END

#endif

