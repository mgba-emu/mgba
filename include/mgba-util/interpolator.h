/* Copyright (c) 2013-2024 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_INTERPOLATOR_H
#define M_INTERPOLATOR_H

#include <mgba-util/common.h>

struct mInterpData {
	int16_t (*at)(const void* mInterpData, size_t index);
};

struct mInterpolator {
	int16_t (*interpolate)(const struct mInterpolator* interp, const struct mInterpData* data, double time, double sampleStep);
};

struct mInterpolatorSinc {
	struct mInterpolator d;

	unsigned resolution;
	unsigned width;
	double* sincLut;
	double* windowLut;
};

void mInterpolatorSincInit(struct mInterpolatorSinc* interp, unsigned resolution, unsigned width);
void mInterpolatorSincDeinit(struct mInterpolatorSinc* interp);

#endif
