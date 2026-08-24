/* Copyright (c) 2013-2026 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba-util/interpolator.h>

struct TestSamples {
	const int16_t* samples;
	size_t count;
};

static int16_t _sampleAt(int index, const void* context) {
	const struct TestSamples* samples = context;

	// Return silence outside the test signal
	if (index < 0 || (size_t) index >= samples->count) {
		return 0;
	}
	return samples->samples[index];
}

M_TEST_DEFINE(sincAtIntegerPosition) {
	// Prepare an asymmetric signal that exposes a displaced kernel center
	static const int16_t source[] = { 0, 100, -300, 700, 1200, -900, 400, 2000, -1500, 600, 0, 300, -100, 50, 0, 0, 0 };
	struct TestSamples samples = {
		.samples = source,
		.count = sizeof(source) / sizeof(*source),
	};
	struct mInterpolationData data = {
		.at = _sampleAt,
		.context = &samples,
	};
	struct mInterpolatorSinc interp;
	mInterpolatorSincInit(&interp, 0, 0);

	// Verify exact source positions survive both equal-rate and upsampled paths
	assert_int_equal(interp.d.interpolate(&interp.d, &data, 7.0, 1.0), source[7]);
	assert_int_equal(interp.d.interpolate(&interp.d, &data, 7.0, 32768.0 / 44100.0), source[7]);

	// Release the interpolation tables
	mInterpolatorSincDeinit(&interp);
}

M_TEST_DEFINE(sincFractionalSymmetry) {
	// Prepare a centered impulse for comparing opposite fractional phases
	int16_t source[17] = {0};
	source[8] = 12000;
	struct TestSamples samples = {
		.samples = source,
		.count = sizeof(source) / sizeof(*source),
	};
	struct mInterpolationData data = {
		.at = _sampleAt,
		.context = &samples,
	};
	struct mInterpolatorSinc interp;
	mInterpolatorSincInit(&interp, 0, 0);

	// Verify the kernel has matching responses on either side of the impulse
	int16_t left = interp.d.interpolate(&interp.d, &data, 7.75, 32768.0 / 44100.0);
	int16_t right = interp.d.interpolate(&interp.d, &data, 8.25, 32768.0 / 44100.0);
	assert_int_equal(left, right);

	// Release the interpolation tables
	mInterpolatorSincDeinit(&interp);
}

M_TEST_DEFINE(sincDownsampleLowPass) {
	// Prepare a Nyquist-frequency signal that must be rejected when halving the rate
	int16_t source[33];
	size_t i;
	for (i = 0; i < sizeof(source) / sizeof(*source); ++i) {
		source[i] = i & 1 ? -12000 : 12000;
	}
	struct TestSamples samples = {
		.samples = source,
		.count = sizeof(source) / sizeof(*source),
	};
	struct mInterpolationData data = {
		.at = _sampleAt,
		.context = &samples,
	};
	struct mInterpolatorSinc interp;
	mInterpolatorSincInit(&interp, 0, 0);

	// Verify downsampling applies a low-pass cutoff instead of aliasing Nyquist to DC
	int16_t sample = interp.d.interpolate(&interp.d, &data, 16.0, 2.0);
	assert_in_range(sample, -100, 100);

	// Release the interpolation tables
	mInterpolatorSincDeinit(&interp);
}

M_TEST_SUITE_DEFINE(Interpolator,
	cmocka_unit_test(sincAtIntegerPosition),
	cmocka_unit_test(sincFractionalSymmetry),
	cmocka_unit_test(sincDownsampleLowPass))
