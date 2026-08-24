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

void mInterpolatorSincInit(struct mInterpolatorSinc* interp, unsigned resolution, unsigned width) {
	interp->d.interpolate = mInterpolatorSincInterpolate;

	if (!resolution) {
		resolution = mSINC_RESOLUTION;
	}
	if (!width) {
		width = mSINC_WIDTH;
	}
	unsigned samples = resolution * width;
	double dy = M_PI / samples;
	double y = dy;
	double dx = dy * width;
	double x = dx;

	interp->sincLut = calloc(samples + 1, sizeof(double));
	interp->windowLut = calloc(samples + 1, sizeof(double));

	// Initialize the removable singularity and window origin
	interp->sincLut[0] = 1;
	interp->windowLut[0] = 1;

	interp->width = width;
	interp->resolution = resolution;

	unsigned i;
	// Build a normalized sinc table spanning the full window
	for (i = 1; i <= samples; ++i, x += dx, y += dy) {
		interp->sincLut[i] = sin(x) / x;
		// Three term Nuttall window with continuous first derivative
		interp->windowLut[i] = 0.40897 + 0.5 * cos(y) + 0.09103 * cos(2 * y);
	}
}

void mInterpolatorSincDeinit(struct mInterpolatorSinc* interp) {
	free(interp->sincLut);
	free(interp->windowLut);
}

int16_t mInterpolatorSincInterpolate(const struct mInterpolator* interpolator, const struct mInterpolationData* data, double time, double sampleStep) {
	struct mInterpolatorSinc* interp = (struct mInterpolatorSinc*) interpolator;
	int index = time;
	double subsample = time - floor(time);
	double cutoff = sampleStep > 1 ? 1.0 / sampleStep : 1.0;
	int shift = subsample * interp->resolution;
	double sum = 0.0;
	double kernelSum = 0.0;
	double kernel;

	int i;
	// Apply the windowed low-pass kernel around the fractional source position
	for (i = 1 - (int) interp->width; i <= (int) interp->width; ++i) {
		int offset = i * (int) interp->resolution - shift;
		unsigned window = offset < 0 ? -offset : offset;
		unsigned sinc = window * cutoff;

		kernel = interp->sincLut[sinc] * interp->windowLut[window];
		kernelSum += kernel;
		sum += data->at(index + i, data->context) * kernel;
	}
	return sum / kernelSum;
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
