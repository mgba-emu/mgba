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

M_TEST_DEFINE(writeLenCapacity) {
	struct CircleBuffer buffer;
	const char* data = " Lorem ipsum dolor sit amet, consectetur adipiscing elit placerat.";
	char databuf[64];

	CircleBufferInit(&buffer, 64);

	assert_int_equal(CircleBufferWrite(&buffer, data, 64), 64);
	assert_int_equal(CircleBufferSize(&buffer), 64);
	assert_int_equal(CircleBufferRead(&buffer, databuf, 64), 64);
	assert_int_equal(CircleBufferSize(&buffer), 0);
	assert_memory_equal(data, databuf, 64);

	assert_int_equal(CircleBufferWrite(&buffer, data, 48), 48);
	assert_int_equal(CircleBufferSize(&buffer), 48);
	assert_int_equal(CircleBufferWrite(&buffer, data, 48), 0);
	assert_int_equal(CircleBufferSize(&buffer), 48);
	assert_int_equal(CircleBufferRead(&buffer, databuf, 64), 48);
	assert_memory_equal(data, databuf, 48);

	assert_int_equal(CircleBufferWrite(&buffer, data, 48), 48);
	assert_int_equal(CircleBufferSize(&buffer), 48);
	assert_int_equal(CircleBufferWrite(&buffer, data, 16), 16);
	assert_int_equal(CircleBufferSize(&buffer), 64);
	assert_int_equal(CircleBufferRead(&buffer, databuf, 64), 64);
	assert_int_equal(CircleBufferSize(&buffer), 0);
	assert_memory_equal(data, databuf, 48);
	assert_memory_equal(data, &databuf[48], 16);

	CircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(writeTruncate) {
	struct CircleBuffer buffer;
	const char* data = " Lorem ipsum dolor sit amet, consectetur adipiscing elit placerat.";
	char databuf[64];

	CircleBufferInit(&buffer, 64);

	assert_int_equal(CircleBufferWriteTruncate(&buffer, data, 64), 64);
	assert_int_equal(CircleBufferSize(&buffer), 64);
	assert_int_equal(CircleBufferRead(&buffer, databuf, 64), 64);
	assert_int_equal(CircleBufferSize(&buffer), 0);
	assert_memory_equal(data, databuf, 64);

	assert_int_equal(CircleBufferWriteTruncate(&buffer, data, 48), 48);
	assert_int_equal(CircleBufferSize(&buffer), 48);
	assert_int_equal(CircleBufferWrite(&buffer, data, 48), 0);
	assert_int_equal(CircleBufferSize(&buffer), 48);
	assert_int_equal(CircleBufferWriteTruncate(&buffer, data, 48), 16);
	assert_int_equal(CircleBufferSize(&buffer), 64);
	assert_int_equal(CircleBufferRead(&buffer, databuf, 64), 64);
	assert_memory_equal(data, databuf, 48);
	assert_memory_equal(data, &databuf[48], 16);

	CircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(dumpBasic) {
	struct CircleBuffer buffer;
	const char* data = " Lorem ipsum dolor sit amet, consectetur adipiscing elit placerat.";
	char databuf[64];

	CircleBufferInit(&buffer, 64);

	assert_int_equal(CircleBufferWrite(&buffer, data, 64), 64);
	assert_int_equal(CircleBufferSize(&buffer), 64);
	assert_int_equal(CircleBufferDump(&buffer, databuf, 64, 0), 64);
	assert_int_equal(CircleBufferSize(&buffer), 64);
	assert_memory_equal(data, databuf, 64);
	assert_int_equal(CircleBufferRead(&buffer, databuf, 64), 64);
	assert_int_equal(CircleBufferSize(&buffer), 0);
	assert_memory_equal(data, databuf, 64);

	assert_int_equal(CircleBufferWrite(&buffer, data, 48), 48);
	assert_int_equal(CircleBufferSize(&buffer), 48);
	assert_int_equal(CircleBufferDump(&buffer, databuf, 48, 0), 48);
	assert_int_equal(CircleBufferSize(&buffer), 48);
	assert_memory_equal(data, databuf, 48);
	assert_int_equal(CircleBufferRead(&buffer, databuf, 16), 16);
	assert_int_equal(CircleBufferSize(&buffer), 32);
	assert_memory_equal(data, databuf, 16);
	assert_int_equal(CircleBufferDump(&buffer, databuf, 48, 0), 32);
	assert_int_equal(CircleBufferSize(&buffer), 32);
	assert_memory_equal(&data[16], databuf, 32);

	assert_int_equal(CircleBufferWrite(&buffer, data, 32), 32);
	assert_int_equal(CircleBufferSize(&buffer), 64);
	assert_int_equal(CircleBufferDump(&buffer, databuf, 64, 0), 64);
	assert_int_equal(CircleBufferSize(&buffer), 64);
	assert_memory_equal(&data[16], databuf, 32);
	assert_memory_equal(data, &databuf[32], 32);
	assert_int_equal(CircleBufferRead(&buffer, databuf, 64), 64);
	assert_memory_equal(&data[16], databuf, 32);
	assert_memory_equal(data, &databuf[32], 32);
	assert_int_equal(CircleBufferSize(&buffer), 0);

	CircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(dumpOffset) {
	struct CircleBuffer buffer;
	const char* data = " Lorem ipsum dolor sit amet, consectetur adipiscing elit placerat.";
	char databuf[64];

	CircleBufferInit(&buffer, 64);

	assert_int_equal(CircleBufferWrite(&buffer, data, 48), 48);
	assert_int_equal(CircleBufferSize(&buffer), 48);
	assert_int_equal(CircleBufferDump(&buffer, databuf, 32, 0), 32);
	assert_memory_equal(data, databuf, 32);
	assert_int_equal(CircleBufferDump(&buffer, databuf, 32, 16), 32);
	assert_memory_equal(&data[16], databuf, 32);
	assert_int_equal(CircleBufferDump(&buffer, databuf, 32, 32), 16);
	assert_memory_equal(&data[32], databuf, 16);

	assert_int_equal(CircleBufferRead(&buffer, databuf, 16), 16);
	assert_int_equal(CircleBufferSize(&buffer), 32);
	assert_memory_equal(data, databuf, 16);
	assert_int_equal(CircleBufferDump(&buffer, databuf, 32, 0), 32);
	assert_memory_equal(&data[16], databuf, 32);
	assert_int_equal(CircleBufferDump(&buffer, databuf, 32, 16), 16);
	assert_memory_equal(&data[32], databuf, 16);

	assert_int_equal(CircleBufferWrite(&buffer, data, 32), 32);
	assert_int_equal(CircleBufferSize(&buffer), 64);
	assert_int_equal(CircleBufferDump(&buffer, databuf, 32, 0), 32);
	assert_memory_equal(&data[16], databuf, 32);
	assert_int_equal(CircleBufferDump(&buffer, databuf, 32, 16), 32);
	assert_memory_equal(&data[32], databuf, 16);
	assert_memory_equal(data, &databuf[16], 16);
	assert_int_equal(CircleBufferDump(&buffer, databuf, 32, 32), 32);
	assert_memory_equal(data, databuf, 32);
	assert_int_equal(CircleBufferDump(&buffer, databuf, 32, 48), 16);
	assert_memory_equal(&data[16], databuf, 16);

	CircleBufferDeinit(&buffer);
}

M_TEST_SUITE_DEFINE(CircleBuffer,
	cmocka_unit_test(basicCircle),
	cmocka_unit_test(basicAlignment16),
	cmocka_unit_test(basicAlignment32),
	cmocka_unit_test(capacity),
	cmocka_unit_test(overCapacity16),
	cmocka_unit_test(writeLenCapacity),
	cmocka_unit_test(writeTruncate),
	cmocka_unit_test(dumpBasic),
	cmocka_unit_test(dumpOffset),
)
