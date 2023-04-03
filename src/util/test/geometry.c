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

M_TEST_DEFINE(intersectRectNWNo) {
	/*
	 * A.....
	 * ......
	 * ..RR..
	 * ..RR..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 0,
		.y = 0,
		.width = 1,
		.height = 1
	};
	assert_false(mRectangleIntersection(&ref, &a));
}

M_TEST_DEFINE(intersectRectNWCorner0) {
	/*
	 * ......
	 * .A....
	 * ..RR..
	 * ..RR..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 1,
		.y = 1,
		.width = 1,
		.height = 1
	};
	assert_false(mRectangleIntersection(&ref, &a));
}

M_TEST_DEFINE(intersectRectNWCorner1) {
	/*
	 * ......
	 * .AA...
	 * .AXR..
	 * ..RR..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 1,
		.y = 1,
		.width = 2,
		.height = 2
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 2);
	assert_int_equal(ref.y, 2);
	assert_int_equal(ref.height, 1);
	assert_int_equal(ref.width, 1);
}

M_TEST_DEFINE(intersectRectNWFullN) {
	/*
	 * ......
	 * .AAA..
	 * .AXX..
	 * ..RR..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 1,
		.y = 1,
		.width = 3,
		.height = 2
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 2);
	assert_int_equal(ref.y, 2);
	assert_int_equal(ref.height, 1);
	assert_int_equal(ref.width, 2);
}

M_TEST_DEFINE(intersectRectNWFullW) {
	/*
	 * ......
	 * .AA...
	 * .AXR..
	 * .AXR..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 1,
		.y = 1,
		.width = 2,
		.height = 3
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 2);
	assert_int_equal(ref.y, 2);
	assert_int_equal(ref.height, 2);
	assert_int_equal(ref.width, 1);
}

M_TEST_DEFINE(intersectRectNNo) {
	/*
	 * ..AA..
	 * ......
	 * ..RR..
	 * ..RR..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 2,
		.y = 0,
		.width = 2,
		.height = 1
	};
	assert_false(mRectangleIntersection(&ref, &a));
}

M_TEST_DEFINE(intersectRectN0) {
	/*
	 * ......
	 * ..AA..
	 * ..RR..
	 * ..RR..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 2,
		.y = 1,
		.width = 2,
		.height = 1
	};
	assert_false(mRectangleIntersection(&ref, &a));
}

M_TEST_DEFINE(intersectRectN1) {
	/*
	 * ......
	 * ..AA..
	 * ..XX..
	 * ..RR..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 2,
		.y = 1,
		.width = 2,
		.height = 2
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 2);
	assert_int_equal(ref.y, 2);
	assert_int_equal(ref.height, 1);
	assert_int_equal(ref.width, 2);
}

M_TEST_DEFINE(intersectRectNFull) {
	/*
	 * ......
	 * ..AA..
	 * ..XX..
	 * ..XX..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 2,
		.y = 1,
		.width = 2,
		.height = 3
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 2);
	assert_int_equal(ref.y, 2);
	assert_int_equal(ref.height, 2);
	assert_int_equal(ref.width, 2);
}

M_TEST_DEFINE(intersectRectNENo) {
	/*
	 * .....A
	 * ......
	 * ..RR..
	 * ..RR..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 5,
		.y = 0,
		.width = 1,
		.height = 1
	};
	assert_false(mRectangleIntersection(&ref, &a));
}

M_TEST_DEFINE(intersectRectNECorner0) {
	/*
	 * ......
	 * ....A.
	 * ..RR..
	 * ..RR..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 4,
		.y = 0,
		.width = 1,
		.height = 1
	};
	assert_false(mRectangleIntersection(&ref, &a));
}

M_TEST_DEFINE(intersectRectNECorner1) {
	/*
	 * ......
	 * ..,AA.
	 * ..RXA.
	 * ..RR..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 3,
		.y = 1,
		.width = 2,
		.height = 2
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 3);
	assert_int_equal(ref.y, 2);
	assert_int_equal(ref.height, 1);
	assert_int_equal(ref.width, 1);
}

M_TEST_DEFINE(intersectRectNEFullN) {
	/*
	 * ......
	 * ..AAA.
	 * ..XXA.
	 * ..RR..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 2,
		.y = 1,
		.width = 3,
		.height = 2
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 2);
	assert_int_equal(ref.y, 2);
	assert_int_equal(ref.height, 1);
	assert_int_equal(ref.width, 2);
}

M_TEST_DEFINE(intersectRectNEFullE) {
	/*
	 * ......
	 * ...AA.
	 * ..RXA.
	 * ..RXA.
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 3,
		.y = 1,
		.width = 2,
		.height = 3
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 3);
	assert_int_equal(ref.y, 2);
	assert_int_equal(ref.height, 2);
	assert_int_equal(ref.width, 1);
}

M_TEST_DEFINE(intersectRectWNo) {
	/*
	 * ......
	 * ......
	 * A.RR..
	 * A.RR..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 0,
		.y = 2,
		.width = 1,
		.height = 1
	};
	assert_false(mRectangleIntersection(&ref, &a));
}

M_TEST_DEFINE(intersectRectW0) {
	/*
	 * ......
	 * ......
	 * .ARR..
	 * .ARR..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 1,
		.y = 2,
		.width = 1,
		.height = 2
	};
	assert_false(mRectangleIntersection(&ref, &a));
}

M_TEST_DEFINE(intersectRectW1) {
	/*
	 * ......
	 * ......
	 * .AXR..
	 * .AXR..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 1,
		.y = 2,
		.width = 2,
		.height = 2
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 2);
	assert_int_equal(ref.y, 2);
	assert_int_equal(ref.height, 2);
	assert_int_equal(ref.width, 1);
}

M_TEST_DEFINE(intersectRectWFull) {
	/*
	 * ......
	 * ......
	 * .AXX..
	 * .AXX..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 1,
		.y = 2,
		.width = 3,
		.height = 2
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 2);
	assert_int_equal(ref.y, 2);
	assert_int_equal(ref.height, 2);
	assert_int_equal(ref.width, 2);
}

M_TEST_DEFINE(intersectRectSWNo) {
	/*
	 * ......
	 * ......
	 * ..RR..
	 * ..RR..
	 * ......
	 * A.....
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 0,
		.y = 5,
		.width = 1,
		.height = 1
	};
	assert_false(mRectangleIntersection(&ref, &a));
}

M_TEST_DEFINE(intersectRectSWCorner0) {
	/*
	 * ......
	 * ......
	 * ..RR..
	 * ..RR..
	 * .A....
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 1,
		.y = 4,
		.width = 1,
		.height = 1
	};
	assert_false(mRectangleIntersection(&ref, &a));
}

M_TEST_DEFINE(intersectRectSWCorner1) {
	/*
	 * ......
	 * ......
	 * ..RR..
	 * .AXR..
	 * .AA...
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 1,
		.y = 3,
		.width = 2,
		.height = 2
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 2);
	assert_int_equal(ref.y, 3);
	assert_int_equal(ref.height, 1);
	assert_int_equal(ref.width, 1);
}

M_TEST_DEFINE(intersectRectSWFullS) {
	/*
	 * ......
	 * ......
	 * ..RR..
	 * .AXX..
	 * .AAA..
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 1,
		.y = 3,
		.width = 3,
		.height = 2
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 2);
	assert_int_equal(ref.y, 3);
	assert_int_equal(ref.height, 1);
	assert_int_equal(ref.width, 2);
}

M_TEST_DEFINE(intersectRectSWFullW) {
	/*
	 * ......
	 * ......
	 * .AXR..
	 * .AXR..
	 * .AA...
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 1,
		.y = 2,
		.width = 2,
		.height = 3
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 2);
	assert_int_equal(ref.y, 2);
	assert_int_equal(ref.height, 2);
	assert_int_equal(ref.width, 1);
}

M_TEST_DEFINE(intersectRectSNo) {
	/*
	 * ......
	 * ......
	 * ..RR..
	 * ..RR..
	 * ......
	 * ..AA..
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 2,
		.y = 5,
		.width = 2,
		.height = 1
	};
	assert_false(mRectangleIntersection(&ref, &a));
}

M_TEST_DEFINE(intersectRectS0) {
	/*
	 * ......
	 * ......
	 * ..RR..
	 * ..RR..
	 * ..AA..
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 2,
		.y = 4,
		.width = 2,
		.height = 1
	};
	assert_false(mRectangleIntersection(&ref, &a));
}

M_TEST_DEFINE(intersectRectS1) {
	/*
	 * ......
	 * ......
	 * ..RR..
	 * ..XX..
	 * ..AA..
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 2,
		.y = 3,
		.width = 2,
		.height = 2
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 2);
	assert_int_equal(ref.y, 3);
	assert_int_equal(ref.height, 1);
	assert_int_equal(ref.width, 2);
}

M_TEST_DEFINE(intersectRectSFull) {
	/*
	 * ......
	 * ......
	 * ..XX..
	 * ..XX..
	 * ..AA..
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 3
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 2);
	assert_int_equal(ref.y, 2);
	assert_int_equal(ref.height, 2);
	assert_int_equal(ref.width, 2);
}

M_TEST_DEFINE(intersectRectSENo) {
	/*
	 * ......
	 * ......
	 * ..RR..
	 * ..RR..
	 * ......
	 * .....A
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 5,
		.y = 5,
		.width = 1,
		.height = 1
	};
	assert_false(mRectangleIntersection(&ref, &a));
}

M_TEST_DEFINE(intersectRectSECorner0) {
	/*
	 * ......
	 * ......
	 * ..RR..
	 * ..RR..
	 * ....A.
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 4,
		.y = 4,
		.width = 1,
		.height = 1
	};
	assert_false(mRectangleIntersection(&ref, &a));
}

M_TEST_DEFINE(intersectRectSECorner1) {
	/*
	 * ......
	 * ..,...
	 * ..RR..
	 * ..RXA.
	 * ...AA.
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 3,
		.y = 3,
		.width = 2,
		.height = 2
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 3);
	assert_int_equal(ref.y, 3);
	assert_int_equal(ref.height, 1);
	assert_int_equal(ref.width, 1);
}

M_TEST_DEFINE(intersectRectSEFullS) {
	/*
	 * ......
	 * ......
	 * ..RR..
	 * ..XXA.
	 * ..AAA.
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 2,
		.y = 3,
		.width = 3,
		.height = 2
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 2);
	assert_int_equal(ref.y, 3);
	assert_int_equal(ref.height, 1);
	assert_int_equal(ref.width, 2);
}

M_TEST_DEFINE(intersectRectSEFullE) {
	/*
	 * ......
	 * ......
	 * ..RXA.
	 * ..RXA.
	 * ...AA.
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 3,
		.y = 2,
		.width = 2,
		.height = 3
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 3);
	assert_int_equal(ref.y, 2);
	assert_int_equal(ref.height, 2);
	assert_int_equal(ref.width, 1);
}

M_TEST_DEFINE(intersectRectENo) {
	/*
	 * ......
	 * ......
	 * ..RR.A
	 * ..RR.A
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 5,
		.y = 2,
		.width = 1,
		.height = 1
	};
	assert_false(mRectangleIntersection(&ref, &a));
}

M_TEST_DEFINE(intersectRectE0) {
	/*
	 * ......
	 * ......
	 * ..RRA.
	 * ..RRA.
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 4,
		.y = 2,
		.width = 1,
		.height = 2
	};
	assert_false(mRectangleIntersection(&ref, &a));
}

M_TEST_DEFINE(intersectRectE1) {
	/*
	 * ......
	 * ......
	 * ..RXA.
	 * ..RXA.
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 3,
		.y = 2,
		.width = 2,
		.height = 2
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 3);
	assert_int_equal(ref.y, 2);
	assert_int_equal(ref.height, 2);
	assert_int_equal(ref.width, 1);
}

M_TEST_DEFINE(intersectRectEFull) {
	/*
	 * ......
	 * ......
	 * ..XXA.
	 * ..XXA.
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 2,
		.y = 2,
		.width = 3,
		.height = 2
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 2);
	assert_int_equal(ref.y, 2);
	assert_int_equal(ref.height, 2);
	assert_int_equal(ref.width, 2);
}

M_TEST_DEFINE(intersectRectInternalNE) {
	/*
	 * ......
	 * ......
	 * ..XR..
	 * ..RR..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 2,
		.y = 2,
		.width = 1,
		.height = 1
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 2);
	assert_int_equal(ref.y, 2);
	assert_int_equal(ref.height, 1);
	assert_int_equal(ref.width, 1);
}

M_TEST_DEFINE(intersectRectInternalNW) {
	/*
	 * ......
	 * ......
	 * ..RX..
	 * ..RR..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 3,
		.y = 2,
		.width = 1,
		.height = 1
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 3);
	assert_int_equal(ref.y, 2);
	assert_int_equal(ref.height, 1);
	assert_int_equal(ref.width, 1);
}

M_TEST_DEFINE(intersectRectInternalSE) {
	/*
	 * ......
	 * ......
	 * ..RR..
	 * ..XR..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 2,
		.y = 3,
		.width = 1,
		.height = 1
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 2);
	assert_int_equal(ref.y, 3);
	assert_int_equal(ref.height, 1);
	assert_int_equal(ref.width, 1);
}

M_TEST_DEFINE(intersectRectInternalSW) {
	/*
	 * ......
	 * ......
	 * ..RR..
	 * ..RX..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 3,
		.y = 3,
		.width = 1,
		.height = 1
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 3);
	assert_int_equal(ref.y, 3);
	assert_int_equal(ref.height, 1);
	assert_int_equal(ref.width, 1);
}

M_TEST_DEFINE(intersectRectInternalN) {
	/*
	 * ......
	 * ......
	 * ..XX..
	 * ..RR..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 1
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 2);
	assert_int_equal(ref.y, 2);
	assert_int_equal(ref.height, 1);
	assert_int_equal(ref.width, 2);
}

M_TEST_DEFINE(intersectRectInternalW) {
	/*
	 * ......
	 * ......
	 * ..XR..
	 * ..XR..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 2,
		.y = 2,
		.width = 1,
		.height = 2
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 2);
	assert_int_equal(ref.y, 2);
	assert_int_equal(ref.height, 2);
	assert_int_equal(ref.width, 1);
}

M_TEST_DEFINE(intersectRectInternalS) {
	/*
	 * ......
	 * ......
	 * ..RR..
	 * ..XX..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 2,
		.y = 3,
		.width = 2,
		.height = 1
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 2);
	assert_int_equal(ref.y, 3);
	assert_int_equal(ref.height, 1);
	assert_int_equal(ref.width, 2);
}

M_TEST_DEFINE(intersectRectInternalE) {
	/*
	 * ......
	 * ......
	 * ..RX..
	 * ..RX..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 3,
		.y = 2,
		.width = 1,
		.height = 2
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 3);
	assert_int_equal(ref.y, 2);
	assert_int_equal(ref.height, 2);
	assert_int_equal(ref.width, 1);
}

M_TEST_DEFINE(intersectRectEqual) {
	/*
	 * ......
	 * ......
	 * ..XX..
	 * ..XX..
	 * ......
	 * ......
	 */
	struct mRectangle ref = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	struct mRectangle a = {
		.x = 2,
		.y = 2,
		.width = 2,
		.height = 2
	};
	assert_true(mRectangleIntersection(&ref, &a));
	assert_int_equal(ref.x, 2);
	assert_int_equal(ref.y, 2);
	assert_int_equal(ref.height, 2);
	assert_int_equal(ref.width, 2);
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
	cmocka_unit_test(intersectRectNWNo),
	cmocka_unit_test(intersectRectNWCorner0),
	cmocka_unit_test(intersectRectNWCorner1),
	cmocka_unit_test(intersectRectNWFullN),
	cmocka_unit_test(intersectRectNWFullW),
	cmocka_unit_test(intersectRectNNo),
	cmocka_unit_test(intersectRectN0),
	cmocka_unit_test(intersectRectN1),
	cmocka_unit_test(intersectRectNFull),
	cmocka_unit_test(intersectRectNENo),
	cmocka_unit_test(intersectRectNECorner0),
	cmocka_unit_test(intersectRectNECorner1),
	cmocka_unit_test(intersectRectNEFullN),
	cmocka_unit_test(intersectRectNEFullE),
	cmocka_unit_test(intersectRectWNo),
	cmocka_unit_test(intersectRectW0),
	cmocka_unit_test(intersectRectW1),
	cmocka_unit_test(intersectRectWFull),
	cmocka_unit_test(intersectRectSWNo),
	cmocka_unit_test(intersectRectSWCorner0),
	cmocka_unit_test(intersectRectSWCorner1),
	cmocka_unit_test(intersectRectSWFullS),
	cmocka_unit_test(intersectRectSWFullW),
	cmocka_unit_test(intersectRectSNo),
	cmocka_unit_test(intersectRectS0),
	cmocka_unit_test(intersectRectS1),
	cmocka_unit_test(intersectRectSFull),
	cmocka_unit_test(intersectRectSENo),
	cmocka_unit_test(intersectRectSECorner0),
	cmocka_unit_test(intersectRectSECorner1),
	cmocka_unit_test(intersectRectSEFullS),
	cmocka_unit_test(intersectRectSEFullE),
	cmocka_unit_test(intersectRectENo),
	cmocka_unit_test(intersectRectE0),
	cmocka_unit_test(intersectRectE1),
	cmocka_unit_test(intersectRectEFull),
	cmocka_unit_test(intersectRectInternalNE),
	cmocka_unit_test(intersectRectInternalNW),
	cmocka_unit_test(intersectRectInternalSE),
	cmocka_unit_test(intersectRectInternalSW),
	cmocka_unit_test(intersectRectInternalN),
	cmocka_unit_test(intersectRectInternalW),
	cmocka_unit_test(intersectRectInternalS),
	cmocka_unit_test(intersectRectInternalE),
	cmocka_unit_test(intersectRectEqual),
	cmocka_unit_test(centerRectBasic),
	cmocka_unit_test(centerRectRoundDown),
	cmocka_unit_test(centerRectRoundDown2),
	cmocka_unit_test(centerRectOffset),
)
