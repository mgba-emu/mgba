/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba-util/image.h>

M_TEST_DEFINE(channelSwap32) {
	assert_int_equal(mColorConvert(0xFFAABBCC, mCOLOR_ARGB8, mCOLOR_ABGR8), 0xFFCCBBAA);
	assert_int_equal(mColorConvert(0xFFAABBCC, mCOLOR_ABGR8, mCOLOR_ARGB8), 0xFFCCBBAA);
	assert_int_equal(mColorConvert(0xFFAABBCC, mCOLOR_XRGB8, mCOLOR_XBGR8), 0xFFCCBBAA);
	assert_int_equal(mColorConvert(0xFFAABBCC, mCOLOR_XBGR8, mCOLOR_XRGB8), 0xFFCCBBAA);
	assert_int_equal(mColorConvert(0xAABBCC, mCOLOR_RGB8, mCOLOR_BGR8), 0xCCBBAA);
	assert_int_equal(mColorConvert(0xAABBCC, mCOLOR_BGR8, mCOLOR_RGB8), 0xCCBBAA);

	assert_int_equal(mColorConvert(0xFFAABBCC, mCOLOR_ARGB8, mCOLOR_RGBA8), 0xAABBCCFF);
	assert_int_equal(mColorConvert(0xFFAABBCC, mCOLOR_ABGR8, mCOLOR_BGRA8), 0xAABBCCFF);
	assert_int_equal(mColorConvert(0xAABBCCFF, mCOLOR_RGBA8, mCOLOR_ARGB8), 0xFFAABBCC);
	assert_int_equal(mColorConvert(0xAABBCCFF, mCOLOR_BGRA8, mCOLOR_ABGR8), 0xFFAABBCC);

	assert_int_equal(mColorConvert(0xFFAABBCC, mCOLOR_ARGB8, mCOLOR_BGRA8), 0xCCBBAAFF);
	assert_int_equal(mColorConvert(0xFFAABBCC, mCOLOR_ABGR8, mCOLOR_RGBA8), 0xCCBBAAFF);
	assert_int_equal(mColorConvert(0xFFAABBCC, mCOLOR_RGBA8, mCOLOR_ABGR8), 0xCCBBAAFF);
	assert_int_equal(mColorConvert(0xFFAABBCC, mCOLOR_BGRA8, mCOLOR_ARGB8), 0xCCBBAAFF);
}

M_TEST_DEFINE(channelSwap16) {
	assert_int_equal(mColorConvert(0xFFE0, mCOLOR_ARGB5, mCOLOR_ABGR5), 0x83FF);
	assert_int_equal(mColorConvert(0xFFE0, mCOLOR_ABGR5, mCOLOR_ARGB5), 0x83FF);
	assert_int_equal(mColorConvert(0x7FE0, mCOLOR_RGB5, mCOLOR_BGR5), 0x03FF);
	assert_int_equal(mColorConvert(0x7FE0, mCOLOR_BGR5, mCOLOR_RGB5), 0x03FF);
	assert_int_equal(mColorConvert(0xFFE0, mCOLOR_RGB565, mCOLOR_BGR565), 0x07FF);
	assert_int_equal(mColorConvert(0xFFE0, mCOLOR_BGR565, mCOLOR_RGB565), 0x07FF);

	assert_int_equal(mColorConvert(0xFFE0, mCOLOR_ARGB5, mCOLOR_RGBA5), 0xFFC1);
	assert_int_equal(mColorConvert(0xFFE0, mCOLOR_RGBA5, mCOLOR_ARGB5), 0x7FF0);
}

M_TEST_DEFINE(convertQuantizeOpaque) {
	assert_int_equal(mColorConvert(0xA0B0C0, mCOLOR_XRGB8, mCOLOR_RGB5), (0xA << 11) | (0xB << 6) | (0xC << 1));
	assert_int_equal(mColorConvert(0xA1B1C1, mCOLOR_XRGB8, mCOLOR_RGB5), (0xA << 11) | (0xB << 6) | (0xC << 1));
	assert_int_equal(mColorConvert(0xA2B2C2, mCOLOR_XRGB8, mCOLOR_RGB5), (0xA << 11) | (0xB << 6) | (0xC << 1));
	assert_int_equal(mColorConvert(0xA4B4C4, mCOLOR_XRGB8, mCOLOR_RGB5), (0xA << 11) | (0xB << 6) | (0xC << 1));
	assert_int_equal(mColorConvert(0xA8B8C8, mCOLOR_XRGB8, mCOLOR_RGB5), (0xA << 11) | (0x1B << 6) | (0x1C << 1) | 1);

	assert_int_equal(mColorConvert(0xA0B0C0, mCOLOR_XRGB8, mCOLOR_BGR5), (0xC << 11) | (0xB << 6) | (0xA << 1));
	assert_int_equal(mColorConvert(0xA1B1C1, mCOLOR_XRGB8, mCOLOR_BGR5), (0xC << 11) | (0xB << 6) | (0xA << 1));
	assert_int_equal(mColorConvert(0xA2B2C2, mCOLOR_XRGB8, mCOLOR_BGR5), (0xC << 11) | (0xB << 6) | (0xA << 1));
	assert_int_equal(mColorConvert(0xA4B4C4, mCOLOR_XRGB8, mCOLOR_BGR5), (0xC << 11) | (0xB << 6) | (0xA << 1));
	assert_int_equal(mColorConvert(0xA8B8C8, mCOLOR_XRGB8, mCOLOR_BGR5), (0xC << 11) | (0x1B << 6) | (0x1A << 1) | 1);

	assert_int_equal(mColorConvert(0xA0B0C0, mCOLOR_XRGB8, mCOLOR_RGB565), (0xA << 12) | (0xB << 7) | (0xC << 1));
	assert_int_equal(mColorConvert(0xA1B1C1, mCOLOR_XRGB8, mCOLOR_RGB565), (0xA << 12) | (0xB << 7) | (0xC << 1));
	assert_int_equal(mColorConvert(0xA2B2C2, mCOLOR_XRGB8, mCOLOR_RGB565), (0xA << 12) | (0xB << 7) | (0xC << 1));
	assert_int_equal(mColorConvert(0xA4B4C4, mCOLOR_XRGB8, mCOLOR_RGB565), (0xA << 12) | (0xB << 7) | (0x1C << 1));
	assert_int_equal(mColorConvert(0xA8B8C8, mCOLOR_XRGB8, mCOLOR_RGB565), (0xA << 12) | (0x1B << 7) | (0x2C << 1) | 1);
	assert_int_equal(mColorConvert(0xACBCCC, mCOLOR_XRGB8, mCOLOR_RGB565), (0xA << 12) | (0x1B << 7) | (0x3C << 1) | 1);

	assert_int_equal(mColorConvert(0xA0B0C0, mCOLOR_XRGB8, mCOLOR_BGR565), (0xC << 12) | (0xB << 7) | (0xA << 1));
	assert_int_equal(mColorConvert(0xA1B1C1, mCOLOR_XRGB8, mCOLOR_BGR565), (0xC << 12) | (0xB << 7) | (0xA << 1));
	assert_int_equal(mColorConvert(0xA2B2C2, mCOLOR_XRGB8, mCOLOR_BGR565), (0xC << 12) | (0xB << 7) | (0xA << 1));
	assert_int_equal(mColorConvert(0xA4B4C4, mCOLOR_XRGB8, mCOLOR_BGR565), (0xC << 12) | (0xB << 7) | (0x1A << 1));
	assert_int_equal(mColorConvert(0xA8B8C8, mCOLOR_XRGB8, mCOLOR_BGR565), (0xC << 12) | (0x1B << 7) | (0x2A << 1) | 1);
	assert_int_equal(mColorConvert(0xACBCCC, mCOLOR_XRGB8, mCOLOR_BGR565), (0xC << 12) | (0x1B << 7) | (0x3A << 1) | 1);
}

M_TEST_DEFINE(convertQuantizeTransparent) {
	assert_int_equal(mColorConvert(0xFFA0B0C0, mCOLOR_ARGB8, mCOLOR_ARGB5), 0x8000 | (0xA << 11) | (0xB << 6) | (0xC << 1));
	assert_int_equal(mColorConvert(0x00A0B0C0, mCOLOR_ARGB8, mCOLOR_ARGB5), (0xA << 11) | (0xB << 6) | (0xC << 1));
	assert_int_equal(mColorConvert(0xFEA0B0C0, mCOLOR_ARGB8, mCOLOR_ARGB5), 0x8000 | (0xA << 11) | (0xB << 6) | (0xC << 1));
	assert_int_equal(mColorConvert(0x01A0B0C0, mCOLOR_ARGB8, mCOLOR_ARGB5), 0x8000 | (0xA << 11) | (0xB << 6) | (0xC << 1));

	assert_int_equal(mColorConvert(0xFFA0B0C0, mCOLOR_ARGB8, mCOLOR_ABGR5), 0x8000 | (0xC << 11) | (0xB << 6) | (0xA << 1));
	assert_int_equal(mColorConvert(0x00A0B0C0, mCOLOR_ARGB8, mCOLOR_ABGR5), (0xC << 11) | (0xB << 6) | (0xA << 1));
	assert_int_equal(mColorConvert(0xFEA0B0C0, mCOLOR_ARGB8, mCOLOR_ABGR5), 0x8000 | (0xC << 11) | (0xB << 6) | (0xA << 1));
	assert_int_equal(mColorConvert(0x01A0B0C0, mCOLOR_ARGB8, mCOLOR_ABGR5), 0x8000 | (0xC << 11) | (0xB << 6) | (0xA << 1));

	assert_int_equal(mColorConvert(0xFFA0B0C0, mCOLOR_ARGB8, mCOLOR_RGBA5), 1 | (0xA << 12) | (0xB << 7) | (0xC << 2));
	assert_int_equal(mColorConvert(0x00A0B0C0, mCOLOR_ARGB8, mCOLOR_RGBA5), (0xA << 12) | (0xB << 7) | (0xC << 2));
	assert_int_equal(mColorConvert(0xFEA0B0C0, mCOLOR_ARGB8, mCOLOR_RGBA5), 1 | (0xA << 12) | (0xB << 7) | (0xC << 2));
	assert_int_equal(mColorConvert(0x01A0B0C0, mCOLOR_ARGB8, mCOLOR_RGBA5), 1 | (0xA << 12) | (0xB << 7) | (0xC << 2));

	assert_int_equal(mColorConvert(0xFFA0B0C0, mCOLOR_ARGB8, mCOLOR_BGRA5), 1 | (0xC << 12) | (0xB << 7) | (0xA << 2));
	assert_int_equal(mColorConvert(0x00A0B0C0, mCOLOR_ARGB8, mCOLOR_BGRA5), (0xC << 12) | (0xB << 7) | (0xA << 2));
	assert_int_equal(mColorConvert(0xFEA0B0C0, mCOLOR_ARGB8, mCOLOR_BGRA5), 1 | (0xC << 12) | (0xB << 7) | (0xA << 2));
	assert_int_equal(mColorConvert(0x01A0B0C0, mCOLOR_ARGB8, mCOLOR_BGRA5), 1 | (0xC << 12) | (0xB << 7) | (0xA << 2));
}

M_TEST_DEFINE(convertToOpaque) {
	assert_int_equal(mColorConvert(0xFFAABBCC, mCOLOR_ARGB8, mCOLOR_XRGB8), 0xFFAABBCC);
	assert_int_equal(mColorConvert(0xFEAABBCC, mCOLOR_ARGB8, mCOLOR_XRGB8), 0xFFAABBCC);
	assert_int_equal(mColorConvert(0x01AABBCC, mCOLOR_ARGB8, mCOLOR_XRGB8), 0xFFAABBCC);
	assert_int_equal(mColorConvert(0x00AABBCC, mCOLOR_ARGB8, mCOLOR_XRGB8), 0xFFAABBCC);

	assert_int_equal(mColorConvert(0xFFAABBCC, mCOLOR_ARGB8, mCOLOR_RGB8), 0xAABBCC);
	assert_int_equal(mColorConvert(0xFEAABBCC, mCOLOR_ARGB8, mCOLOR_RGB8), 0xAABBCC);
	assert_int_equal(mColorConvert(0x01AABBCC, mCOLOR_ARGB8, mCOLOR_RGB8), 0xAABBCC);
	assert_int_equal(mColorConvert(0x00AABBCC, mCOLOR_ARGB8, mCOLOR_RGB8), 0xAABBCC);

	assert_int_equal(mColorConvert(0xAABBCCFF, mCOLOR_RGBA8, mCOLOR_XRGB8), 0xFFAABBCC);
	assert_int_equal(mColorConvert(0xAABBCCFE, mCOLOR_RGBA8, mCOLOR_XRGB8), 0xFFAABBCC);
	assert_int_equal(mColorConvert(0xAABBCC01, mCOLOR_RGBA8, mCOLOR_XRGB8), 0xFFAABBCC);
	assert_int_equal(mColorConvert(0xAABBCC00, mCOLOR_RGBA8, mCOLOR_XRGB8), 0xFFAABBCC);

	assert_int_equal(mColorConvert(0xAABBCCFF, mCOLOR_RGBA8, mCOLOR_RGB8), 0xAABBCC);
	assert_int_equal(mColorConvert(0xAABBCCFE, mCOLOR_RGBA8, mCOLOR_RGB8), 0xAABBCC);
	assert_int_equal(mColorConvert(0xAABBCC01, mCOLOR_RGBA8, mCOLOR_RGB8), 0xAABBCC);
	assert_int_equal(mColorConvert(0xAABBCC00, mCOLOR_RGBA8, mCOLOR_RGB8), 0xAABBCC);

	assert_int_equal(mColorConvert(0x7FFF, mCOLOR_ARGB5, mCOLOR_RGB5), 0x7FFF);
	assert_int_equal(mColorConvert(0xFFFF, mCOLOR_ARGB5, mCOLOR_RGB5), 0x7FFF);

	assert_int_equal(mColorConvert(0xFFFE, mCOLOR_RGBA5, mCOLOR_RGB5), 0x7FFF);
	assert_int_equal(mColorConvert(0xFFFF, mCOLOR_RGBA5, mCOLOR_RGB5), 0x7FFF);
}

M_TEST_DEFINE(convertToAlpha) {
	assert_int_equal(mColorConvert(0xFFAABBCC, mCOLOR_XRGB8, mCOLOR_ARGB8), 0xFFAABBCC);
	assert_int_equal(mColorConvert(0xFEAABBCC, mCOLOR_XRGB8, mCOLOR_ARGB8), 0xFFAABBCC);
	assert_int_equal(mColorConvert(0x01AABBCC, mCOLOR_XRGB8, mCOLOR_ARGB8), 0xFFAABBCC);
	assert_int_equal(mColorConvert(0x00AABBCC, mCOLOR_XRGB8, mCOLOR_ARGB8), 0xFFAABBCC);

	assert_int_equal(mColorConvert(0xFFAABBCC, mCOLOR_RGB8, mCOLOR_ARGB8), 0xFFAABBCC);
	assert_int_equal(mColorConvert(0xFEAABBCC, mCOLOR_RGB8, mCOLOR_ARGB8), 0xFFAABBCC);
	assert_int_equal(mColorConvert(0x01AABBCC, mCOLOR_RGB8, mCOLOR_ARGB8), 0xFFAABBCC);
	assert_int_equal(mColorConvert(0x00AABBCC, mCOLOR_RGB8, mCOLOR_ARGB8), 0xFFAABBCC);

	assert_int_equal(mColorConvert(0x7FFF, mCOLOR_RGB5, mCOLOR_ARGB5), 0xFFFF);
	assert_int_equal(mColorConvert(0xFFFF, mCOLOR_RGB5, mCOLOR_ARGB5), 0xFFFF);

	assert_int_equal(mColorConvert(0x7FFF, mCOLOR_RGB5, mCOLOR_RGBA5), 0xFFFF);
	assert_int_equal(mColorConvert(0xFFFF, mCOLOR_RGB5, mCOLOR_RGBA5), 0xFFFF);
}

M_TEST_DEFINE(convertFromGray) {
	int i;
	for (i = 0; i < 256; ++i) {
		assert_int_equal(mColorConvert(i, mCOLOR_L8, mCOLOR_RGB8), (i << 16) | (i << 8) | i);
		assert_int_equal(mColorConvert(i, mCOLOR_L8, mCOLOR_ARGB8), 0xFF000000 | (i << 16) | (i << 8) | i);
	}
}

M_TEST_DEFINE(convertToGray) {
	int i;
	for (i = 0; i < 256; ++i) {
		assert_int_equal(mColorConvert((i << 16) | (i << 8) | i, mCOLOR_RGB8, mCOLOR_L8), i);
		assert_int_equal(mColorConvert((i << 16) | (i << 8) | i, mCOLOR_ARGB8, mCOLOR_L8), i);
		assert_int_equal(mColorConvert(0xFF000000 | (i << 16) | (i << 8) | i, mCOLOR_ARGB8, mCOLOR_L8), i);
	}
}

M_TEST_DEFINE(alphaBlendARGB8) {
	assert_int_equal(mColorMixARGB8(0xFF012345, 0xFF987654), 0xFF012345);
	assert_int_equal(mColorMixARGB8(0x00012345, 0xFF987654), 0xFF987654);
	assert_int_equal(mColorMixARGB8(0x80012345, 0xFF987654), 0xFF4C4C4C);
	assert_int_equal(mColorMixARGB8(0x80012345, 0x40987654), 0xC04C4C4C);
	assert_int_equal(mColorMixARGB8(0x01012345, 0xFF987654), 0xFF977553);
	assert_int_equal(mColorMixARGB8(0x01012345, 0xFD987654), 0xFE977553);
}

M_TEST_SUITE_DEFINE(Color,
	cmocka_unit_test(channelSwap32),
	cmocka_unit_test(channelSwap16),
	cmocka_unit_test(convertQuantizeOpaque),
	cmocka_unit_test(convertQuantizeTransparent),
	cmocka_unit_test(convertToOpaque),
	cmocka_unit_test(convertToAlpha),
	cmocka_unit_test(convertFromGray),
	cmocka_unit_test(convertToGray),
	cmocka_unit_test(alphaBlendARGB8),
)
