/* Copyright (c) 2013-2024 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba-util/circle-buffer.h>

M_TEST_DEFINE(basicCircle) {
	struct CircleBuffer buffer;

	CircleBufferInit(&buffer, 64);

	int8_t i;
	for (i = 0; i < 63; ++i) {
		assert_int_equal(CircleBufferWrite8(&buffer, i), 1);
	}
	for (i = 0; i < 63; ++i) {
		int8_t value;
		assert_int_equal(CircleBufferRead8(&buffer, &value), 1);
		assert_int_equal(value, i);
	}

	for (i = 0; i < 63; ++i) {
		assert_int_equal(CircleBufferWrite8(&buffer, i), 1);
	}
	for (i = 0; i < 63; ++i) {
		int8_t value;
		assert_int_equal(CircleBufferRead8(&buffer, &value), 1);
		assert_int_equal(value, i);
	}

	CircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(basicAlignment16) {
	struct CircleBuffer buffer;

	CircleBufferInit(&buffer, 64);

	int16_t i;
	for (i = 0; i < 29; ++i) {
		assert_int_equal(CircleBufferWrite16(&buffer, i), 2);
	}
	for (i = 0; i < 29; ++i) {
		int16_t value;
		assert_int_equal(CircleBufferRead16(&buffer, &value), 2);
		assert_int_equal(value, i);
	}

	int8_t i8;
	assert_int_equal(CircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(CircleBufferRead8(&buffer, &i8), 1);

	for (i = 0; i < 29; ++i) {
		assert_int_equal(CircleBufferWrite16(&buffer, i), 2);
	}
	for (i = 0; i < 29; ++i) {
		int16_t value;
		assert_int_equal(CircleBufferRead16(&buffer, &value), 2);
		assert_int_equal(value, i);
	}

	CircleBufferDeinit(&buffer);
}


M_TEST_DEFINE(basicAlignment32) {
	struct CircleBuffer buffer;

	CircleBufferInit(&buffer, 64);

	int32_t i;
	for (i = 0; i < 15; ++i) {
		assert_int_equal(CircleBufferWrite32(&buffer, i), 4);
	}
	for (i = 0; i < 15; ++i) {
		int32_t value;
		assert_int_equal(CircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i);
	}

	int8_t i8;
	assert_int_equal(CircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(CircleBufferRead8(&buffer, &i8), 1);

	for (i = 0; i < 15; ++i) {
		assert_int_equal(CircleBufferWrite32(&buffer, i), 4);
	}
	for (i = 0; i < 15; ++i) {
		int32_t value;
		assert_int_equal(CircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i);
	}

	assert_int_equal(CircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(CircleBufferRead8(&buffer, &i8), 1);

	for (i = 0; i < 15; ++i) {
		assert_int_equal(CircleBufferWrite32(&buffer, i), 4);
	}
	for (i = 0; i < 15; ++i) {
		int32_t value;
		assert_int_equal(CircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i);
	}

	assert_int_equal(CircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(CircleBufferRead8(&buffer, &i8), 1);

	for (i = 0; i < 15; ++i) {
		assert_int_equal(CircleBufferWrite32(&buffer, i), 4);
	}
	for (i = 0; i < 15; ++i) {
		int32_t value;
		assert_int_equal(CircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i);
	}

	CircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(capacity) {
	struct CircleBuffer buffer;

	CircleBufferInit(&buffer, 64);

	int8_t i;
	for (i = 0; i < 64; ++i) {
		assert_int_equal(CircleBufferWrite8(&buffer, i), 1);
	}
	for (i = 0; i < 64; ++i) {
		int8_t value;
		assert_int_equal(CircleBufferRead8(&buffer, &value), 1);
		assert_int_equal(value, i);
	}

	for (i = 0; i < 64; ++i) {
		assert_int_equal(CircleBufferWrite8(&buffer, i), 1);
	}
	assert_int_equal(CircleBufferWrite8(&buffer, 64), 0);

	for (i = 0; i < 64; ++i) {
		int8_t value;
		assert_int_equal(CircleBufferRead8(&buffer, &value), 1);
		assert_int_equal(value, i);
	}

	CircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(overCapacity16) {
	struct CircleBuffer buffer;

	CircleBufferInit(&buffer, 64);

	int8_t i;
	for (i = 0; i < 63; ++i) {
		assert_int_equal(CircleBufferWrite8(&buffer, i), 1);
	}
	assert_int_equal(CircleBufferWrite16(&buffer, 0xFFFF), 0);

	CircleBufferDeinit(&buffer);
}

M_TEST_SUITE_DEFINE(CircleBuffer,
	cmocka_unit_test(basicCircle),
	cmocka_unit_test(basicAlignment16),
	cmocka_unit_test(basicAlignment32),
	cmocka_unit_test(capacity),
	cmocka_unit_test(overCapacity16),
)
