/* Copyright (c) 2013-2024 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba-util/circle-buffer.h>

M_TEST_DEFINE(basicCircle) {
	struct mCircleBuffer buffer;

	mCircleBufferInit(&buffer, 64);

	int8_t i;
	for (i = 0; i < 63; ++i) {
		assert_int_equal(mCircleBufferWrite8(&buffer, i), 1);
	}
	for (i = 0; i < 63; ++i) {
		int8_t value;
		assert_int_equal(mCircleBufferRead8(&buffer, &value), 1);
		assert_int_equal(value, i);
	}

	mCircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(basicAlignment16) {
	struct mCircleBuffer buffer;
	int8_t i8;

	mCircleBufferInit(&buffer, 64);

	// Aligned buffer
	int16_t i;
	for (i = 0; i < 31; ++i) {
		assert_int_equal(mCircleBufferWrite16(&buffer, i), 2);
	}
	for (i = 0; i < 31; ++i) {
		int16_t value;
		assert_int_equal(mCircleBufferRead16(&buffer, &value), 2);
		assert_int_equal(value, i);
	}

	// Misaligned buffer
	mCircleBufferClear(&buffer);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);

	for (i = 0; i < 31; ++i) {
		assert_int_equal(mCircleBufferWrite16(&buffer, i), 2);
	}

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 31; ++i) {
		int16_t value;
		assert_int_equal(mCircleBufferRead16(&buffer, &value), 2);
		assert_int_equal(value, i);
	}

	mCircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(basicAlignment32) {
	struct mCircleBuffer buffer;

	mCircleBufferInit(&buffer, 64);

	// Aligned buffer
	int32_t i;
	for (i = 0; i < 15; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i), 4);
	}
	for (i = 0; i < 15; ++i) {
		int32_t value;
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i);
	}

	// Singly misaligned buffer
	int8_t i8;
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);

	for (i = 0; i < 15; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i), 4);
	}

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 15; ++i) {
		int32_t value;
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i);
	}

	// Doubly misaligned buffer
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 1), 1);

	for (i = 0; i < 15; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i), 4);
	}

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);

	for (i = 0; i < 15; ++i) {
		int32_t value;
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i);
	}

	// Triply misaligned buffer
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 1), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 2), 1);

	for (i = 0; i < 15; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i), 4);
	}

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);

	for (i = 0; i < 15; ++i) {
		int32_t value;
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i);
	}

	mCircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(capacity) {
	struct mCircleBuffer buffer;

	mCircleBufferInit(&buffer, 64);

	int8_t i;
	for (i = 0; i < 64; ++i) {
		assert_int_equal(mCircleBufferWrite8(&buffer, i), 1);
	}
	for (i = 0; i < 64; ++i) {
		int8_t value;
		assert_int_equal(mCircleBufferRead8(&buffer, &value), 1);
		assert_int_equal(value, i);
	}

	for (i = 0; i < 64; ++i) {
		assert_int_equal(mCircleBufferWrite8(&buffer, i), 1);
	}
	assert_int_equal(mCircleBufferWrite8(&buffer, 64), 0);

	for (i = 0; i < 64; ++i) {
		int8_t value;
		assert_int_equal(mCircleBufferRead8(&buffer, &value), 1);
		assert_int_equal(value, i);
	}

	mCircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(overflowWrap8) {
	struct mCircleBuffer buffer;

	mCircleBufferInit(&buffer, 64);

	int8_t value;
	int8_t i;
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferRead8(&buffer, &value), 1);
	for (i = 0; i < 63; ++i) {
		assert_int_equal(mCircleBufferWrite8(&buffer, i), 1);
	}
	assert_int_equal(mCircleBufferRead8(&buffer, &value), 1);
	for (i = 0; i < 63; ++i) {
		assert_int_equal(mCircleBufferRead8(&buffer, &value), 1);
		assert_int_equal(value, i);
	}

	mCircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(overflowWrap16) {
	struct mCircleBuffer buffer;

	mCircleBufferInit(&buffer, 64);

	int16_t value;
	int16_t i;
	assert_int_equal(mCircleBufferWrite16(&buffer, 0), 2);
	assert_int_equal(mCircleBufferWrite16(&buffer, 0), 2);
	assert_int_equal(mCircleBufferRead16(&buffer, &value), 2);
	for (i = 0; i < 31; ++i) {
		assert_int_equal(mCircleBufferWrite16(&buffer, i), 2);
	}
	assert_int_equal(mCircleBufferRead16(&buffer, &value), 2);
	for (i = 0; i < 31; ++i) {
		assert_int_equal(mCircleBufferRead16(&buffer, &value), 2);
		assert_int_equal(value, i);
	}

	mCircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(overflowWrap16_1) {
	struct mCircleBuffer buffer;

	mCircleBufferInit(&buffer, 64);

	int8_t i8;
	int16_t value;
	int16_t i;
	assert_int_equal(mCircleBufferWrite16(&buffer, 0), 2);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferRead16(&buffer, &value), 2);
	for (i = 0; i < 31; ++i) {
		assert_int_equal(mCircleBufferWrite16(&buffer, i), 2);
	}
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 31; ++i) {
		assert_int_equal(mCircleBufferRead16(&buffer, &value), 2);
		assert_int_equal(value, i);
	}

	mCircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(overflowWrap32) {
	struct mCircleBuffer buffer;

	mCircleBufferInit(&buffer, 64);

	int32_t value;
	int32_t i;
	assert_int_equal(mCircleBufferWrite32(&buffer, 0), 4);
	assert_int_equal(mCircleBufferWrite32(&buffer, 0), 4);
	assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
	for (i = 0; i < 15; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i), 4);
	}
	assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
	for (i = 0; i < 15; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i);
	}

	mCircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(overflowWrap32_1) {
	struct mCircleBuffer buffer;

	mCircleBufferInit(&buffer, 64);

	int8_t i8;
	int32_t value;
	int32_t i;
	assert_int_equal(mCircleBufferWrite32(&buffer, 0), 4);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
	for (i = 0; i < 15; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i), 4);
	}
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 15; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i);
	}

	mCircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(overflowWrap32_2) {
	struct mCircleBuffer buffer;

	mCircleBufferInit(&buffer, 64);

	int16_t i16;
	int32_t value;
	int32_t i;
	assert_int_equal(mCircleBufferWrite32(&buffer, 0), 4);
	assert_int_equal(mCircleBufferWrite16(&buffer, 0), 2);
	assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
	for (i = 0; i < 15; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i), 4);
	}
	assert_int_equal(mCircleBufferRead16(&buffer, &i16), 2);
	for (i = 0; i < 15; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i);
	}

	mCircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(overflowWrap32_3) {
	struct mCircleBuffer buffer;

	mCircleBufferInit(&buffer, 64);

	int8_t i8;
	int32_t value;
	int32_t i;
	assert_int_equal(mCircleBufferWrite32(&buffer, 0), 4);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
	for (i = 0; i < 15; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i), 4);
	}
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 15; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i);
	}

	mCircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(weirdSize16) {
	struct mCircleBuffer buffer;

	mCircleBufferInit(&buffer, 15);

	// Aligned, no overflow wrap
	int16_t value;
	int16_t i;
	for (i = 0; i < 7; ++i) {
		assert_int_equal(mCircleBufferWrite16(&buffer, i * 0x102), 2);
	}
	assert_int_equal(mCircleBufferWrite16(&buffer, 7), 0);
	for (i = 0; i < 7; ++i) {
		assert_int_equal(mCircleBufferRead16(&buffer, &value), 2);
		assert_int_equal(value, i * 0x102);
	}

	// Misaligned, no overflow wrap
	mCircleBufferClear(&buffer);
	int8_t i8;
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);

	for (i = 0; i < 7; ++i) {
		assert_int_equal(mCircleBufferWrite16(&buffer, i * 0x102), 2);
	}

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 7; ++i) {
		assert_int_equal(mCircleBufferRead16(&buffer, &value), 2);
		assert_int_equal(value, i * 0x102);
	}

	// Aligned, overflow wrap
	mCircleBufferClear(&buffer);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);

	for (i = 0; i < 6; ++i) {
		assert_int_equal(mCircleBufferWrite16(&buffer, i * 0x102), 2);
	}
	assert_int_equal(mCircleBufferWrite16(&buffer, 6 * 0x102), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite16(&buffer, 6 * 0x102), 2);

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 7; ++i) {
		assert_int_equal(mCircleBufferRead16(&buffer, &value), 2);
		assert_int_equal(value, i * 0x102);
	}

	// Misaligned, overflow wrap
	mCircleBufferClear(&buffer);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);

	for (i = 0; i < 6; ++i) {
		assert_int_equal(mCircleBufferWrite16(&buffer, i * 0x102), 2);
	}
	assert_int_equal(mCircleBufferWrite16(&buffer, 6 * 0x102), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite16(&buffer, 6 * 0x102), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite16(&buffer, 6 * 0x102), 2);

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 7; ++i) {
		assert_int_equal(mCircleBufferRead16(&buffer, &value), 2);
		assert_int_equal(value, i * 0x102);
	}

	mCircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(weirdSize32_1) {
	struct mCircleBuffer buffer;

	mCircleBufferInit(&buffer, 13);

	// Aligned, no overflow wrap
	int32_t value;
	int32_t i;
	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i * 0x1020304), 4);
	}
	assert_int_equal(mCircleBufferWrite32(&buffer, 3), 0);
	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i * 0x1020304);
	}

	// Misaligned, no overflow wrap
	mCircleBufferClear(&buffer);
	int8_t i8;
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);

	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i * 0x1020304), 4);
	}

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i * 0x1020304);
	}

	// Aligned, overflow wrap
	mCircleBufferClear(&buffer);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);

	for (i = 0; i < 2; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i * 0x1020304), 4);
	}
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 4);

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i * 0x1020304);
	}

	// Misaligned, overflow wrap
	mCircleBufferClear(&buffer);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);

	for (i = 0; i < 2; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i * 0x1020304), 4);
	}
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 4);

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i * 0x1020304);
	}

	mCircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(weirdSize32_2) {
	struct mCircleBuffer buffer;

	mCircleBufferInit(&buffer, 14);

	// Aligned, no overflow wrap
	int32_t value;
	int8_t i8;
	int32_t i;
	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i * 0x1020304), 4);
	}
	assert_int_equal(mCircleBufferWrite32(&buffer, 3), 0);
	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i * 0x1020304);
	}

	// Singly misaligned, no overflow wrap
	mCircleBufferClear(&buffer);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);

	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i * 0x1020304), 4);
	}

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i * 0x1020304);
	}

	// Doubly misaligned, no overflow wrap
	mCircleBufferClear(&buffer);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);

	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i * 0x1020304), 4);
	}

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i * 0x1020304);
	}

	// Aligned, overflow wrap
	mCircleBufferClear(&buffer);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);

	for (i = 0; i < 2; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i * 0x1020304), 4);
	}
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 4);

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i * 0x1020304);
	}

	// Singly misaligned, overflow wrap
	mCircleBufferClear(&buffer);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);

	for (i = 0; i < 2; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i * 0x1020304), 4);
	}
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 4);

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i * 0x1020304);
	}

	// Doubly misaligned, overflow wrap
	mCircleBufferClear(&buffer);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);

	for (i = 0; i < 2; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i * 0x1020304), 4);
	}
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 4);

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i * 0x1020304);
	}

	mCircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(weirdSize32_3) {
	struct mCircleBuffer buffer;

	mCircleBufferInit(&buffer, 15);

	// Aligned, no overflow wrap
	int32_t value;
	int8_t i8;
	int32_t i;
	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i * 0x1020304), 4);
	}
	assert_int_equal(mCircleBufferWrite32(&buffer, 3), 0);
	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i * 0x1020304);
	}

	// Singly misaligned, no overflow wrap
	mCircleBufferClear(&buffer);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);

	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i * 0x1020304), 4);
	}

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i * 0x1020304);
	}

	// Doubly misaligned, no overflow wrap
	mCircleBufferClear(&buffer);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);

	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i * 0x1020304), 4);
	}

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i * 0x1020304);
	}

	// Triply misaligned, no overflow wrap
	mCircleBufferClear(&buffer);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);

	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i * 0x1020304), 4);
	}

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i * 0x1020304);
	}

	// Aligned, overflow wrap
	mCircleBufferClear(&buffer);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);

	for (i = 0; i < 2; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i * 0x1020304), 4);
	}
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 4);

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i * 0x1020304);
	}

	// Singly misaligned, overflow wrap
	mCircleBufferClear(&buffer);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);

	for (i = 0; i < 2; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i * 0x1020304), 4);
	}
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 4);

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i * 0x1020304);
	}

	// Doubly misaligned, overflow wrap
	mCircleBufferClear(&buffer);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);

	for (i = 0; i < 2; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i * 0x1020304), 4);
	}
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 4);

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i * 0x1020304);
	}

	// Triply misaligned, overflow wrap
	mCircleBufferClear(&buffer);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);
	assert_int_equal(mCircleBufferWrite8(&buffer, 0), 1);

	for (i = 0; i < 2; ++i) {
		assert_int_equal(mCircleBufferWrite32(&buffer, i * 0x1020304), 4);
	}
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 0);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferWrite32(&buffer, 2 * 0x1020304), 4);

	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	assert_int_equal(mCircleBufferRead8(&buffer, &i8), 1);
	for (i = 0; i < 3; ++i) {
		assert_int_equal(mCircleBufferRead32(&buffer, &value), 4);
		assert_int_equal(value, i * 0x1020304);
	}

	mCircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(overCapacity16) {
	struct mCircleBuffer buffer;

	mCircleBufferInit(&buffer, 64);

	int8_t i;
	for (i = 0; i < 63; ++i) {
		assert_int_equal(mCircleBufferWrite8(&buffer, i), 1);
	}
	assert_int_equal(mCircleBufferWrite16(&buffer, 0xFFFF), 0);

	mCircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(writeLenCapacity) {
	struct mCircleBuffer buffer;
	const char* data = " Lorem ipsum dolor sit amet, consectetur adipiscing elit placerat.";
	char databuf[64];

	mCircleBufferInit(&buffer, 64);

	assert_int_equal(mCircleBufferWrite(&buffer, data, 64), 64);
	assert_int_equal(mCircleBufferSize(&buffer), 64);
	assert_int_equal(mCircleBufferRead(&buffer, databuf, 64), 64);
	assert_int_equal(mCircleBufferSize(&buffer), 0);
	assert_memory_equal(data, databuf, 64);

	assert_int_equal(mCircleBufferWrite(&buffer, data, 48), 48);
	assert_int_equal(mCircleBufferSize(&buffer), 48);
	assert_int_equal(mCircleBufferWrite(&buffer, data, 48), 0);
	assert_int_equal(mCircleBufferSize(&buffer), 48);
	assert_int_equal(mCircleBufferRead(&buffer, databuf, 64), 48);
	assert_memory_equal(data, databuf, 48);

	assert_int_equal(mCircleBufferWrite(&buffer, data, 48), 48);
	assert_int_equal(mCircleBufferSize(&buffer), 48);
	assert_int_equal(mCircleBufferWrite(&buffer, data, 16), 16);
	assert_int_equal(mCircleBufferSize(&buffer), 64);
	assert_int_equal(mCircleBufferRead(&buffer, databuf, 64), 64);
	assert_int_equal(mCircleBufferSize(&buffer), 0);
	assert_memory_equal(data, databuf, 48);
	assert_memory_equal(data, &databuf[48], 16);

	mCircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(writeTruncate) {
	struct mCircleBuffer buffer;
	const char* data = " Lorem ipsum dolor sit amet, consectetur adipiscing elit placerat.";
	char databuf[64];

	mCircleBufferInit(&buffer, 64);

	assert_int_equal(mCircleBufferWriteTruncate(&buffer, data, 64), 64);
	assert_int_equal(mCircleBufferSize(&buffer), 64);
	assert_int_equal(mCircleBufferRead(&buffer, databuf, 64), 64);
	assert_int_equal(mCircleBufferSize(&buffer), 0);
	assert_memory_equal(data, databuf, 64);

	assert_int_equal(mCircleBufferWriteTruncate(&buffer, data, 48), 48);
	assert_int_equal(mCircleBufferSize(&buffer), 48);
	assert_int_equal(mCircleBufferWrite(&buffer, data, 48), 0);
	assert_int_equal(mCircleBufferSize(&buffer), 48);
	assert_int_equal(mCircleBufferWriteTruncate(&buffer, data, 48), 16);
	assert_int_equal(mCircleBufferSize(&buffer), 64);
	assert_int_equal(mCircleBufferRead(&buffer, databuf, 64), 64);
	assert_memory_equal(data, databuf, 48);
	assert_memory_equal(data, &databuf[48], 16);

	mCircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(dumpBasic) {
	struct mCircleBuffer buffer;
	const char* data = " Lorem ipsum dolor sit amet, consectetur adipiscing elit placerat.";
	char databuf[64];

	mCircleBufferInit(&buffer, 64);

	assert_int_equal(mCircleBufferWrite(&buffer, data, 64), 64);
	assert_int_equal(mCircleBufferSize(&buffer), 64);
	assert_int_equal(mCircleBufferDump(&buffer, databuf, 64, 0), 64);
	assert_int_equal(mCircleBufferSize(&buffer), 64);
	assert_memory_equal(data, databuf, 64);
	assert_int_equal(mCircleBufferRead(&buffer, databuf, 64), 64);
	assert_int_equal(mCircleBufferSize(&buffer), 0);
	assert_memory_equal(data, databuf, 64);

	assert_int_equal(mCircleBufferWrite(&buffer, data, 48), 48);
	assert_int_equal(mCircleBufferSize(&buffer), 48);
	assert_int_equal(mCircleBufferDump(&buffer, databuf, 48, 0), 48);
	assert_int_equal(mCircleBufferSize(&buffer), 48);
	assert_memory_equal(data, databuf, 48);
	assert_int_equal(mCircleBufferRead(&buffer, databuf, 16), 16);
	assert_int_equal(mCircleBufferSize(&buffer), 32);
	assert_memory_equal(data, databuf, 16);
	assert_int_equal(mCircleBufferDump(&buffer, databuf, 48, 0), 32);
	assert_int_equal(mCircleBufferSize(&buffer), 32);
	assert_memory_equal(&data[16], databuf, 32);

	assert_int_equal(mCircleBufferWrite(&buffer, data, 32), 32);
	assert_int_equal(mCircleBufferSize(&buffer), 64);
	assert_int_equal(mCircleBufferDump(&buffer, databuf, 64, 0), 64);
	assert_int_equal(mCircleBufferSize(&buffer), 64);
	assert_memory_equal(&data[16], databuf, 32);
	assert_memory_equal(data, &databuf[32], 32);
	assert_int_equal(mCircleBufferRead(&buffer, databuf, 64), 64);
	assert_memory_equal(&data[16], databuf, 32);
	assert_memory_equal(data, &databuf[32], 32);
	assert_int_equal(mCircleBufferSize(&buffer), 0);

	mCircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(dumpOffset) {
	struct mCircleBuffer buffer;
	const char* data = " Lorem ipsum dolor sit amet, consectetur adipiscing elit placerat.";
	char databuf[64];

	mCircleBufferInit(&buffer, 64);

	assert_int_equal(mCircleBufferWrite(&buffer, data, 48), 48);
	assert_int_equal(mCircleBufferSize(&buffer), 48);
	assert_int_equal(mCircleBufferDump(&buffer, databuf, 32, 0), 32);
	assert_memory_equal(data, databuf, 32);
	assert_int_equal(mCircleBufferDump(&buffer, databuf, 32, 16), 32);
	assert_memory_equal(&data[16], databuf, 32);
	assert_int_equal(mCircleBufferDump(&buffer, databuf, 32, 32), 16);
	assert_memory_equal(&data[32], databuf, 16);

	assert_int_equal(mCircleBufferRead(&buffer, databuf, 16), 16);
	assert_int_equal(mCircleBufferSize(&buffer), 32);
	assert_memory_equal(data, databuf, 16);
	assert_int_equal(mCircleBufferDump(&buffer, databuf, 32, 0), 32);
	assert_memory_equal(&data[16], databuf, 32);
	assert_int_equal(mCircleBufferDump(&buffer, databuf, 32, 16), 16);
	assert_memory_equal(&data[32], databuf, 16);

	assert_int_equal(mCircleBufferWrite(&buffer, data, 32), 32);
	assert_int_equal(mCircleBufferSize(&buffer), 64);
	assert_int_equal(mCircleBufferDump(&buffer, databuf, 32, 0), 32);
	assert_memory_equal(&data[16], databuf, 32);
	assert_int_equal(mCircleBufferDump(&buffer, databuf, 32, 16), 32);
	assert_memory_equal(&data[32], databuf, 16);
	assert_memory_equal(data, &databuf[16], 16);
	assert_int_equal(mCircleBufferDump(&buffer, databuf, 32, 32), 32);
	assert_memory_equal(data, databuf, 32);
	assert_int_equal(mCircleBufferDump(&buffer, databuf, 32, 48), 16);
	assert_memory_equal(&data[16], databuf, 16);

	mCircleBufferDeinit(&buffer);
}

M_TEST_DEFINE(dumpOffsetWrap) {
	struct mCircleBuffer buffer;
	const char* data = " Lorem ipsum dolor sit amet, consectetur adipiscing elit placerat.";
	char databuf[64];

	mCircleBufferInit(&buffer, 64);

	assert_int_equal(mCircleBufferWrite(&buffer, data, 64), 64);
	assert_int_equal(mCircleBufferSize(&buffer), 64);
	assert_int_equal(mCircleBufferRead(&buffer, databuf, 48), 48);
	assert_memory_equal(data, databuf, 48);
	assert_int_equal(mCircleBufferSize(&buffer), 16);
	assert_int_equal(mCircleBufferWrite(&buffer, data, 16), 16);
	assert_int_equal(mCircleBufferSize(&buffer), 32);

	assert_int_equal(mCircleBufferDump(&buffer, databuf, 64, 0), 32);
	assert_memory_equal(&data[48], databuf, 16);
	assert_memory_equal(data, &databuf[16], 16);

	assert_int_equal(mCircleBufferDump(&buffer, databuf, 64, 8), 24);
	assert_memory_equal(&data[56], databuf, 8);
	assert_memory_equal(data, &databuf[8], 16);

	assert_int_equal(mCircleBufferDump(&buffer, databuf, 64, 16), 16);
	assert_memory_equal(data, databuf, 16);

	assert_int_equal(mCircleBufferDump(&buffer, databuf, 64, 24), 8);
	assert_memory_equal(&data[8], databuf, 8);

	mCircleBufferDeinit(&buffer);
}

M_TEST_SUITE_DEFINE(mCircleBuffer,
	cmocka_unit_test(basicCircle),
	cmocka_unit_test(basicAlignment16),
	cmocka_unit_test(basicAlignment32),
	cmocka_unit_test(capacity),
	cmocka_unit_test(overflowWrap8),
	cmocka_unit_test(overflowWrap16),
	cmocka_unit_test(overflowWrap16_1),
	cmocka_unit_test(overflowWrap32),
	cmocka_unit_test(overflowWrap32_1),
	cmocka_unit_test(overflowWrap32_2),
	cmocka_unit_test(overflowWrap32_3),
	cmocka_unit_test(weirdSize16),
	cmocka_unit_test(weirdSize32_1),
	cmocka_unit_test(weirdSize32_2),
	cmocka_unit_test(weirdSize32_3),
	cmocka_unit_test(overCapacity16),
	cmocka_unit_test(writeLenCapacity),
	cmocka_unit_test(writeTruncate),
	cmocka_unit_test(dumpBasic),
	cmocka_unit_test(dumpOffset),
	cmocka_unit_test(dumpOffsetWrap),
)
