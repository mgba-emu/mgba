/* Copyright (c) 2013-2024 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/interpolator.h>

enum {
	mSINC_RESOLUTION = 8192,
	mSINC_WIDTH = 8,
};

static int16_t mInterpolatorSincInterpolate(const struct mInterpolator*, const struct mSampleBuffer* data, double time, double sampleStep);

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

	interp->sincLut[0] = 0;
	interp->windowLut[0] = 1;

	unsigned i;
	for (i = 1; i <= samples; ++i, x += dx, y += dy) {
		interp->sincLut[i] = x < width ? sin(x) / x : 0.0;
		// Three term Nuttall window with continuous first derivative
		interp->windowLut[i] = 0.40897 + 0.5 * cos(y) + 0.09103 * cos(2 * y);
	}
}

void mInterpolatorSincDeinit(struct mInterpolatorSinc* interp) {
	free(interp->sincLut);
	free(interp->windowLut);
}

int16_t mInterpolatorSincInterpolate(const struct mInterpolator* interpolator, const struct mSampleBuffer* data, double time, double sampleStep) {
	struct mInterpolatorSinc* interp = (struct mInterpolatorSinc*) interpolator;
	ssize_t index = (ssize_t) time;
	double subsample = time - floor(time);
	unsigned step = sampleStep > 1 ? interp->resolution * sampleStep : interp->resolution;
	unsigned yShift = subsample * step;
	unsigned xShift = subsample * interp->resolution;
	double sum = 0.0;
	double kernelSum = 0.0;
	double kernel;

	ssize_t i;
	for (i = 1 - (ssize_t) interp->width; i <= (ssize_t) interp->width; ++i) {
		unsigned window = i * interp->resolution;
		if (yShift > window) {
			window = yShift - window;
		} else {
			window -= yShift;
		}

		unsigned sinc = i * step;
		if (xShift > sinc) {
			sinc = xShift - sinc;
		} else {
			sinc -= xShift;
		}

		kernel = interp->sincLut[sinc] * interp->windowLut[window];
		kernelSum += kernel;
		if (index + i >= 0 && index + i < (ssize_t) data->samples) {
			sum += data->data[(index + i) * data->channels] * kernel;
		}
	}
	return sum / kernelSum;
}
