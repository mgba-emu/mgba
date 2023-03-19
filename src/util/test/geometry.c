/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba-util/geometry.h>

M_TEST_DEFINE(unionRectOrigin) {
	struct mRectangle a = {
		.x = 0,
		.y = 0,
		.width = 1,
		.height = 1
	};
	struct mRectangle b = {
		.x = 1,
		.y = 1,
		.width = 1,
		.height = 1
	};
	mRectangleUnion(&a, &b);
	assert_int_equal(a.x, 0);
	assert_int_equal(a.y, 0);
	assert_int_equal(a.width, 2);
	assert_int_equal(a.height, 2);
}

M_TEST_DEFINE(unionRectOriginSwapped) {
	struct mRectangle a = {
		.x = 1,
		.y = 1,
		.width = 1,
		.height = 1
	};
	struct mRectangle b = {
		.x = 0,
		.y = 0,
		.width = 1,
		.height = 1
	};
	mRectangleUnion(&a, &b);
	assert_int_equal(a.x, 0);
	assert_int_equal(a.y, 0);
	assert_int_equal(a.width, 2);
	assert_int_equal(a.height, 2);
}

M_TEST_DEFINE(unionRectNonOrigin) {
	struct mRectangle a = {
		.x = 1,
		.y = 1,
		.width = 1,
		.height = 1
	};
	struct mRectangle b = {
		.x = 2,
		.y = 2,
		.width = 1,
		.height = 1
	};
	mRectangleUnion(&a, &b);
	assert_int_equal(a.x, 1);
	assert_int_equal(a.y, 1);
	assert_int_equal(a.width, 2);
	assert_int_equal(a.height, 2);
}

M_TEST_DEFINE(unionRectOverlapping) {
	struct mRectangle a = {
		.x = 0,
		.y = 0,
		.width = 2,
		.height = 2
	};
	struct mRectangle b = {
		.x = 1,
		.y = 1,
		.width = 2,
		.height = 2
	};
	mRectangleUnion(&a, &b);
	assert_int_equal(a.x, 0);
	assert_int_equal(a.y, 0);
	assert_int_equal(a.width, 3);
	assert_int_equal(a.height, 3);
}

M_TEST_DEFINE(unionRectSubRect) {
	struct mRectangle a = {
		.x = 0,
		.y = 0,
		.width = 3,
		.height = 3
	};
	struct mRectangle b = {
		.x = 1,
		.y = 1,
		.width = 1,
		.height = 1
	};
	mRectangleUnion(&a, &b);
	assert_int_equal(a.x, 0);
	assert_int_equal(a.y, 0);
	assert_int_equal(a.width, 3);
	assert_int_equal(a.height, 3);
}

M_TEST_DEFINE(unionRectNegativeOrigin) {
	struct mRectangle a = {
		.x = -1,
		.y = -1,
		.width = 1,
		.height = 1
	};
	struct mRectangle b = {
		.x = 0,
		.y = 0,
		.width = 1,
		.height = 1
	};
	mRectangleUnion(&a, &b);
	assert_int_equal(a.x, -1);
	assert_int_equal(a.y, -1);
	assert_int_equal(a.width, 2);
	assert_int_equal(a.height, 2);
}

M_TEST_DEFINE(centerRectBasic) {
	struct mRectangle ref = {
		.x = 0,
		.y = 0,
		.width = 4,
		.height = 4
	};
	struct mRectangle a = {
		.x = 0,
		.y = 0,
		.width = 2,
		.height = 2
	};
	mRectangleCenter(&ref, &a);
	assert_int_equal(a.x, 1);
	assert_int_equal(a.y, 1);
}

M_TEST_DEFINE(centerRectRoundDown) {
	struct mRectangle ref = {
		.x = 0,
		.y = 0,
		.width = 4,
		.height = 4
	};
	struct mRectangle a = {
		.x = 0,
		.y = 0,
		.width = 1,
		.height = 1
	};
	mRectangleCenter(&ref, &a);
	assert_int_equal(a.x, 1);
	assert_int_equal(a.y, 1);
}

M_TEST_DEFINE(centerRectRoundDown2) {
	struct mRectangle ref = {
		.x = 0,
		.y = 0,
		.width = 4,
		.height = 4
	};
	struct mRectangle a = {
		.x = 0,
		.y = 0,
		.width = 3,
		.height = 2
	};
	mRectangleCenter(&ref, &a);
	assert_int_equal(a.x, 0);
	assert_int_equal(a.y, 1);
}

M_TEST_DEFINE(centerRectOffset) {
	struct mRectangle ref = {
		.x = 1,
		.y = 1,
		.width = 4,
		.height = 4
	};
	struct mRectangle a = {
		.x = 0,
		.y = 0,
		.width = 2,
		.height = 2
	};
	mRectangleCenter(&ref, &a);
	assert_int_equal(a.x, 2);
	assert_int_equal(a.y, 2);
}

M_TEST_SUITE_DEFINE(Geometry,
	cmocka_unit_test(unionRectOrigin),
	cmocka_unit_test(unionRectOriginSwapped),
	cmocka_unit_test(unionRectNonOrigin),
	cmocka_unit_test(unionRectOverlapping),
	cmocka_unit_test(unionRectSubRect),
	cmocka_unit_test(unionRectNegativeOrigin),
	cmocka_unit_test(centerRectBasic),
	cmocka_unit_test(centerRectRoundDown),
	cmocka_unit_test(centerRectRoundDown2),
	cmocka_unit_test(centerRectOffset),
)
