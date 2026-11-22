/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba-util/string.h>

M_TEST_DEFINE(strlenASCII) {
	assert_int_equal(utf8strlen(""), 0);
	assert_int_equal(utf8strlen("a"), 1);
	assert_int_equal(utf8strlen("aa"), 2);
	assert_int_equal(utf8strlen("aaa"), 3);
}

M_TEST_DEFINE(strlenMultibyte) {
	assert_int_equal(utf8strlen("\300\200"), 1);
	assert_int_equal(utf8strlen("a\300\200"), 2);
	assert_int_equal(utf8strlen("\300\200a"), 2);
	assert_int_equal(utf8strlen("a\300\200a"), 3);

	assert_int_equal(utf8strlen("\300\200\300\200"), 2);
	assert_int_equal(utf8strlen("a\300\200\300\200"), 3);
	assert_int_equal(utf8strlen("\300\200a\300\200"), 3);
	assert_int_equal(utf8strlen("\300\200\300\200a"), 3);

	assert_int_equal(utf8strlen("\340\200\200"), 1);
	assert_int_equal(utf8strlen("a\340\200\200"), 2);
	assert_int_equal(utf8strlen("\340\200\200a"), 2);
	assert_int_equal(utf8strlen("a\340\200\200a"), 3);

	assert_int_equal(utf8strlen("\340\200\200\340\200\200"), 2);
	assert_int_equal(utf8strlen("a\340\200\200\340\200\200"), 3);
	assert_int_equal(utf8strlen("\340\200\200a\340\200\200"), 3);
	assert_int_equal(utf8strlen("\340\200\200\340\200\200a"), 3);

	assert_int_equal(utf8strlen("\340\200\200\300\200"), 2);
	assert_int_equal(utf8strlen("\300\200\340\200\200"), 2);

	assert_int_equal(utf8strlen("\360\200\200\200"), 1);
	assert_int_equal(utf8strlen("a\360\200\200\200"), 2);
	assert_int_equal(utf8strlen("\360\200\200\200a"), 2);
	assert_int_equal(utf8strlen("a\360\200\200\200a"), 3);

	assert_int_equal(utf8strlen("\360\200\200\200\360\200\200\200"), 2);
	assert_int_equal(utf8strlen("a\360\200\200\200\360\200\200\200"), 3);
	assert_int_equal(utf8strlen("\360\200\200\200a\360\200\200\200"), 3);
	assert_int_equal(utf8strlen("\360\200\200\200\360\200\200\200a"), 3);

	assert_int_equal(utf8strlen("\360\200\200\200\300\200"), 2);
	assert_int_equal(utf8strlen("\300\200\360\200\200\200"), 2);

	assert_int_equal(utf8strlen("\360\200\200\200\340\200\200"), 2);
	assert_int_equal(utf8strlen("\340\200\200\360\200\200\200"), 2);
}

M_TEST_DEFINE(strlenDegenerate) {
	assert_int_equal(utf8strlen("\200"), 1);
	assert_int_equal(utf8strlen("\200a"), 2);

	assert_int_equal(utf8strlen("\300"), 1);
	assert_int_equal(utf8strlen("\300a"), 2);

	assert_int_equal(utf8strlen("\300\300"), 2);
	assert_int_equal(utf8strlen("\300\300a"), 3);
	assert_int_equal(utf8strlen("\300\300\200"), 2);

	assert_int_equal(utf8strlen("\300\200\200"), 2);
	assert_int_equal(utf8strlen("\300\200\200a"), 3);

	assert_int_equal(utf8strlen("\340"), 1);
	assert_int_equal(utf8strlen("\340a"), 2);
	assert_int_equal(utf8strlen("\340\300"), 2);
	assert_int_equal(utf8strlen("\340\300a"), 3);
	assert_int_equal(utf8strlen("\340\300\200"), 2);
	assert_int_equal(utf8strlen("\340\200"), 1);
	assert_int_equal(utf8strlen("\340\200a"), 2);
	assert_int_equal(utf8strlen("\340\200\200\200"), 2);
	assert_int_equal(utf8strlen("\340\200\200\200a"), 3);

	assert_int_equal(utf8strlen("\360"), 1);
	assert_int_equal(utf8strlen("\360a"), 2);
	assert_int_equal(utf8strlen("\360\300"), 2);
	assert_int_equal(utf8strlen("\360\300a"), 3);
	assert_int_equal(utf8strlen("\360\300\200"), 2);
	assert_int_equal(utf8strlen("\360\200"), 1);
	assert_int_equal(utf8strlen("\360\200a"), 2);
	assert_int_equal(utf8strlen("\360\200\300"), 2);
	assert_int_equal(utf8strlen("\360\200\300a"), 3);
	assert_int_equal(utf8strlen("\360\200\300\200"), 2);
	assert_int_equal(utf8strlen("\360\200\200"), 1);
	assert_int_equal(utf8strlen("\360\200\200a"), 2);

	assert_int_equal(utf8strlen("\370"), 1);
	assert_int_equal(utf8strlen("\370a"), 2);
	assert_int_equal(utf8strlen("\370\200"), 2);
	assert_int_equal(utf8strlen("\370\200a"), 3);
	assert_int_equal(utf8strlen("\374"), 1);
	assert_int_equal(utf8strlen("\374a"), 2);
	assert_int_equal(utf8strlen("\374\200"), 2);
	assert_int_equal(utf8strlen("\374\200a"), 3);
	assert_int_equal(utf8strlen("\376"), 1);
	assert_int_equal(utf8strlen("\376a"), 2);
	assert_int_equal(utf8strlen("\376\200"), 2);
	assert_int_equal(utf8strlen("\376\200a"), 3);
	assert_int_equal(utf8strlen("\377"), 1);
	assert_int_equal(utf8strlen("\377a"), 2);
	assert_int_equal(utf8strlen("\377\200"), 2);
	assert_int_equal(utf8strlen("\377\200a"), 3);
}

M_TEST_DEFINE(roundtrip) {
	uint32_t unichar;
	char buf[8] = {0};
	for (unichar = 0; unichar < 0x110000; ++unichar) {
		memset(buf, 0, sizeof(buf));
		size_t len = toUtf8(unichar, buf) + 1;
		const char* ptr = buf;
		assert_true(len);
		assert_false(buf[len]);
		assert_int_equal(utf8Char(&ptr, &len), unichar);
		assert_int_equal(len, 1);
	}
}

M_TEST_DEFINE(roundtripUnpadded) {
	uint32_t unichar;
	char buf[8] = {0};
	for (unichar = 0; unichar < 0x110000; ++unichar) {
		memset(buf, 0, sizeof(buf));
		size_t len = toUtf8(unichar, buf);
		const char* ptr = buf;
		assert_true(len);
		assert_false(buf[len]);
		assert_int_equal(utf8Char(&ptr, &len), unichar);
		assert_int_equal(len, 0);
	}
}

M_TEST_DEFINE(utf16to8Ascii) {
	const uint16_t hello[] = { 'H', 'e', 'l', 'l', 'o', 0};
	char* hello8 = utf16to8(hello, sizeof(hello));
	assert_string_equal(hello8, "Hello");
	free(hello8);
}

M_TEST_DEFINE(utf16to8Cjk) {
	const uint16_t hello[] = { 0x4F60, 0x597D, 0};
	char* hello8 = utf16to8(hello, sizeof(hello));
	const char utf8[] = { 0xE4, 0xBD, 0xA0, 0xE5, 0xA5, 0xBD, 0 };
	assert_string_equal(hello8, utf8);
	free(hello8);
}

M_TEST_DEFINE(utf8to16Ascii) {
	const char* hello = "Hello";
	uint16_t* hello16 = utf8to16(hello, sizeof(hello));
	assert_memory_equal(hello16, ((const uint16_t[]) { 'H', 'e', 'l', 'l', 'o', 0}), 12);
	free(hello16);
}

M_TEST_DEFINE(utf8to16Cjk) {
	const char hello[] = { 0xE4, 0xBD, 0xA0, 0xE5, 0xA5, 0xBD, 0 };
	uint16_t* hello16 = utf8to16(hello, sizeof(hello));
	const uint16_t utf16[] = { 0x4F60, 0x597D, 0};
	assert_memory_equal(hello16, utf16, 6);
	free(hello16);
}

M_TEST_SUITE_DEFINE(StringUTF8,
	cmocka_unit_test(strlenASCII),
	cmocka_unit_test(strlenMultibyte),
	cmocka_unit_test(strlenDegenerate),
	cmocka_unit_test(roundtrip),
	cmocka_unit_test(roundtripUnpadded),
	cmocka_unit_test(utf16to8Ascii),
	cmocka_unit_test(utf16to8Cjk),
	cmocka_unit_test(utf8to16Ascii),
	cmocka_unit_test(utf8to16Cjk),
)
