/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba-util/geometry.h>

M_TEST_DEFINE(unionRectOrigin) {
	struct Rectangle a = {
		.x = 0,
		.y = 0,
		.width = 1,
		.height = 1
	};
	struct Rectangle b = {
		.x = 1,
		.y = 1,
		.width = 1,
		.height = 1
	};
	RectangleUnion(&a, &b);
	assert_int_equal(a.x, 0);
	assert_int_equal(a.y, 0);
	assert_int_equal(a.width, 2);
	assert_int_equal(a.height, 2);
}

M_TEST_DEFINE(unionRectOriginSwapped) {
	struct Rectangle a = {
		.x = 1,
		.y = 1,
		.width = 1,
		.height = 1
	};
	struct Rectangle b = {
		.x = 0,
		.y = 0,
		.width = 1,
		.height = 1
	};
	RectangleUnion(&a, &b);
	assert_int_equal(a.x, 0);
	assert_int_equal(a.y, 0);
	assert_int_equal(a.width, 2);
	assert_int_equal(a.height, 2);
}

M_TEST_DEFINE(unionRectNonOrigin) {
	struct Rectangle a = {
		.x = 1,
		.y = 1,
		.width = 1,
		.height = 1
	};
	struct Rectangle b = {
		.x = 2,
		.y = 2,
		.width = 1,
		.height = 1
	};
	RectangleUnion(&a, &b);
	assert_int_equal(a.x, 1);
	assert_int_equal(a.y, 1);
	assert_int_equal(a.width, 2);
	assert_int_equal(a.height, 2);
}

M_TEST_DEFINE(unionRectOverlapping) {
	struct Rectangle a = {
		.x = 0,
		.y = 0,
		.width = 2,
		.height = 2
	};
	struct Rectangle b = {
		.x = 1,
		.y = 1,
		.width = 2,
		.height = 2
	};
	RectangleUnion(&a, &b);
	assert_int_equal(a.x, 0);
	assert_int_equal(a.y, 0);
	assert_int_equal(a.width, 3);
	assert_int_equal(a.height, 3);
}

M_TEST_DEFINE(unionRectSubRect) {
	struct Rectangle a = {
		.x = 0,
		.y = 0,
		.width = 3,
		.height = 3
	};
	struct Rectangle b = {
		.x = 1,
		.y = 1,
		.width = 1,
		.height = 1
	};
	RectangleUnion(&a, &b);
	assert_int_equal(a.x, 0);
	assert_int_equal(a.y, 0);
	assert_int_equal(a.width, 3);
	assert_int_equal(a.height, 3);
}

M_TEST_DEFINE(unionRectNegativeOrigin) {
	struct Rectangle a = {
		.x = -1,
		.y = -1,
		.width = 1,
		.height = 1
	};
	struct Rectangle b = {
		.x = 0,
		.y = 0,
		.width = 1,
		.height = 1
	};
	RectangleUnion(&a, &b);
	assert_int_equal(a.x, -1);
	assert_int_equal(a.y, -1);
	assert_int_equal(a.width, 2);
	assert_int_equal(a.height, 2);
}

M_TEST_DEFINE(centerRectBasic) {
	struct Rectangle ref = {
		.x = 0,
		.y = 0,
		.width = 4,
		.height = 4
	};
	struct Rectangle a = {
		.x = 0,
		.y = 0,
		.width = 2,
		.height = 2
	};
	RectangleCenter(&ref, &a);
	assert_int_equal(a.x, 1);
	assert_int_equal(a.y, 1);
}

M_TEST_DEFINE(centerRectRoundDown) {
	struct Rectangle ref = {
		.x = 0,
		.y = 0,
		.width = 4,
		.height = 4
	};
	struct Rectangle a = {
		.x = 0,
		.y = 0,
		.width = 1,
		.height = 1
	};
	RectangleCenter(&ref, &a);
	assert_int_equal(a.x, 1);
	assert_int_equal(a.y, 1);
}

M_TEST_DEFINE(centerRectRoundDown2) {
	struct Rectangle ref = {
		.x = 0,
		.y = 0,
		.width = 4,
		.height = 4
	};
	struct Rectangle a = {
		.x = 0,
		.y = 0,
		.width = 3,
		.height = 2
	};
	RectangleCenter(&ref, &a);
	assert_int_equal(a.x, 0);
	assert_int_equal(a.y, 1);
}

M_TEST_DEFINE(centerRectOffset) {
	struct Rectangle ref = {
		.x = 1,
		.y = 1,
		.width = 4,
		.height = 4
	};
	struct Rectangle a = {
		.x = 0,
		.y = 0,
		.width = 2,
		.height = 2
	};
	RectangleCenter(&ref, &a);
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
