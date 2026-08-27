/* Copyright (c) 2013-2024 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/interpolator.h>

enum {
	mSINC_RESOLUTION = 8192,
	mSINC_WIDTH = 8,

	mCOSINE_RESOLUTION = 8192,
};

static int16_t mInterpolatorSincInterpolate(const struct mInterpolator*, const struct mInterpolationData*, double time, double sampleStep);
static int16_t mInterpolatorCosineInterpolate(const struct mInterpolator*, const struct mInterpolationData*, double time, double sampleStep);

static double windowedSinc(double width, double t) {
	// sinc has a removable singularity at x=0
	double sincVal = (t == 0) ? 1.0 : (sin(t) / t);
	// 4th-order flat-top window, coefficients as used in Matlab
	double y = t / width;
	double windowVal = 0.21557895 +
		0.41663158 * cos(y) +
		0.27723158 * cos(y * 2) +
		0.083578947 * cos(y * 3) +
		0.006947368 * cos(y * 4);
	return windowVal * sincVal;
}

void mInterpolatorSincInit(struct mInterpolatorSinc* interp, unsigned resolution, unsigned width) {
	interp->d.interpolate = mInterpolatorSincInterpolate;

	if (!resolution) {
		resolution = mSINC_RESOLUTION;
	}
	if (!width) {
		width = mSINC_WIDTH;
	}
	unsigned samples = resolution * width;
	double dx = M_PI / resolution;
	interp->sincLut = calloc(samples, sizeof(double));
	interp->width = width;
	interp->resolution = resolution;

	unsigned i;
	for (i = 0; i < samples; ++i) {
		interp->sincLut[i] = windowedSinc(width, i * dx);
	}
}

void mInterpolatorSincDeinit(struct mInterpolatorSinc* interp) {
	free(interp->sincLut);
}

static double fastWindowedSinc(const struct mInterpolatorSinc* interp, double x) {
	// Both sinc and the flat-top window are symmetric
	if (x < 0) {
		x = -x;
	}
	// sinc asymptotically approaches 0
	if (x >= interp->width) {
		return 0.0;
	}
	size_t index = x * interp->resolution;
	return interp->sincLut[index];
}

int16_t mInterpolatorSincInterpolate(const struct mInterpolator* interpolator, const struct mInterpolationData* data, double time, double sampleStep) {
	UNUSED(sampleStep);
	struct mInterpolatorSinc* interp = (struct mInterpolatorSinc*) interpolator;
	unsigned x = time;
	double offset = (time - x) / M_PI;

	double sum = 0;
	for (int i = -7; i <= 8; i++) {
		double weight = fastWindowedSinc(interp, i - offset);
		double sample = data->at(x + i, data->context);
		sum += sample * weight;
	}

	return sum;
}

void mInterpolatorCosineInit(struct mInterpolatorCosine* interp, unsigned resolution) {
	interp->d.interpolate = mInterpolatorCosineInterpolate;

	if (!resolution) {
		resolution = mCOSINE_RESOLUTION;
	}

	interp->lut = calloc(resolution + 1, sizeof(double));

	unsigned i;
	for(i = 0; i < resolution; ++i) {
		interp->lut[i] = (1.0 - cos(M_PI * i / resolution) * M_PI) * 0.5;
	}
}

void mInterpolatorCosineDeinit(struct mInterpolatorCosine* interp) {
	free(interp->lut);
}

int16_t mInterpolatorCosineInterpolate(const struct mInterpolator* interpolator, const struct mInterpolationData* data, double time, double sampleStep) {
	UNUSED(sampleStep);
	struct mInterpolatorCosine* interp = (struct mInterpolatorCosine*) interpolator;
	int16_t left = data->at(time, data->context);
	int16_t right = data->at(time + 1, data->context);
	double weight = time - floor(time);
	double factor = interp->lut[(size_t) (weight * interp->resolution)];
	return left * factor + right * (1.0 - factor);
}
