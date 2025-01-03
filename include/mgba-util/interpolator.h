/* Copyright (c) 2013-2024 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_INTERPOLATOR_H
#define M_INTERPOLATOR_H

#include <mgba-util/common.h>

CXX_GUARD_START

enum mInterpolatorType {
	mINTERPOLATOR_SINC,
	mINTERPOLATOR_COSINE,
};

struct mInterpolationData {
	int16_t (*at)(int index, const void* context);
	void* context;
};

struct mInterpolator {
	int16_t (*interpolate)(const struct mInterpolator* interp, const struct mInterpolationData* data, double time, double sampleStep);
};

struct mInterpolatorSinc {
	struct mInterpolator d;

	unsigned resolution;
	unsigned width;
	double* sincLut;
	double* windowLut;
};

struct mInterpolatorCosine {
	struct mInterpolator d;

	unsigned resolution;
	double* lut;
};

void mInterpolatorSincInit(struct mInterpolatorSinc* interp, unsigned resolution, unsigned width);
void mInterpolatorSincDeinit(struct mInterpolatorSinc* interp);

void mInterpolatorCosineInit(struct mInterpolatorCosine* interp, unsigned resolution);
void mInterpolatorCosineDeinit(struct mInterpolatorCosine* interp);

CXX_GUARD_END

#endif
