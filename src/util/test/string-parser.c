/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba-util/string.h>

M_TEST_DEFINE(nullString) {
	const char* unparsed = "";
	char parsed[2];
	assert_int_equal(parseQuotedString(unparsed, 1, parsed, sizeof(parsed)), -1);
}

M_TEST_DEFINE(emptyString2) {
	const char* unparsed = "\"\"";
	char parsed[2];
	assert_int_equal(parseQuotedString(unparsed, strlen(unparsed) + 1, parsed, sizeof(parsed)), 0);
}

M_TEST_DEFINE(emptyString) {
	const char* unparsed = "''";
	char parsed[2];
	assert_int_equal(parseQuotedString(unparsed, strlen(unparsed) + 1, parsed, sizeof(parsed)), 0);
}

M_TEST_DEFINE(plainString) {
	const char* unparsed = "\"plain\"";
	char parsed[32];
	assert_int_equal(parseQuotedString(unparsed, strlen(unparsed) + 1, parsed, sizeof(parsed)), strlen("plain"));
	assert_string_equal(parsed, "plain");
}

M_TEST_DEFINE(plainString2) {
	const char* unparsed = "\"plain\"";
	char parsed[32];
	assert_int_equal(parseQuotedString(unparsed, strlen(unparsed), parsed, sizeof(parsed)), strlen("plain"));
	assert_string_equal(parsed, "plain");
}

M_TEST_DEFINE(trailingString) {
	const char* unparsed = "\"trailing\"T";
	char parsed[32];
	assert_int_equal(parseQuotedString(unparsed, strlen(unparsed) + 1, parsed, sizeof(parsed)), strlen("trailing"));
	assert_string_equal(parsed, "trailing");
}

M_TEST_DEFINE(leadingString) {
	const char* unparsed = "L\"leading\"";
	char parsed[32];
	assert_int_equal(parseQuotedString(unparsed, strlen(unparsed) + 1, parsed, sizeof(parsed)), -1);
}

M_TEST_DEFINE(backslashString) {
	const char* unparsed = "\"back\\\\slash\"";
	char parsed[32];
	assert_int_equal(parseQuotedString(unparsed, strlen(unparsed) + 1, parsed, sizeof(parsed)), strlen("back\\slash"));
	assert_string_equal(parsed, "back\\slash");
}

M_TEST_DEFINE(doubleBackslashString) {
	const char* unparsed = "\"back\\\\\\\\slash\"";
	char parsed[32];
	assert_int_equal(parseQuotedString(unparsed, strlen(unparsed) + 1, parsed, sizeof(parsed)), strlen("back\\\\slash"));
	assert_string_equal(parsed, "back\\\\slash");
}

M_TEST_DEFINE(escapeCharsString) {
	const char* unparsed = "\"\\\"\\'\\n\\r\\\\\"";
	char parsed[32];
	assert_int_equal(parseQuotedString(unparsed, strlen(unparsed) + 1, parsed, sizeof(parsed)), strlen("\"'\n\r\\"));
	assert_string_equal(parsed, "\"'\n\r\\");
}

M_TEST_DEFINE(invalidEscapeCharString) {
	const char* unparsed = "\"\\z\"";
	char parsed[32];
	assert_int_equal(parseQuotedString(unparsed, strlen(unparsed) + 1, parsed, sizeof(parsed)), -1);
}

M_TEST_DEFINE(overflowString) {
	const char* unparsed = "\"longstring\"";
	char parsed[4];
	assert_int_equal(parseQuotedString(unparsed, strlen(unparsed) + 1, parsed, sizeof(parsed)), -1);
}

M_TEST_DEFINE(unclosedString) {
	const char* unparsed = "\"forever";
	char parsed[32];
	assert_int_equal(parseQuotedString(unparsed, strlen(unparsed) + 1, parsed, sizeof(parsed)), -1);
}

M_TEST_DEFINE(unclosedString2) {
	const char* unparsed = "\"forever";
	char parsed[32];
	assert_int_equal(parseQuotedString(unparsed, 4, parsed, sizeof(parsed)), -1);
}

M_TEST_DEFINE(unclosedBackslashString) {
	const char* unparsed = "\"backslash\\";
	char parsed[32];
	assert_int_equal(parseQuotedString(unparsed, strlen(unparsed), parsed, sizeof(parsed)), -1);
}

M_TEST_DEFINE(unclosedBackslashString2) {
	const char* unparsed = "\"backslash\\";
	char parsed[32];
	assert_int_equal(parseQuotedString(unparsed, strlen(unparsed) + 1, parsed, sizeof(parsed)), -1);
}

M_TEST_DEFINE(highCharacterString) {
	const char* unparsed = "\"\200\"";
	char parsed[32];
	assert_int_equal(parseQuotedString(unparsed, strlen(unparsed) + 1, parsed, sizeof(parsed)), 1);
	assert_string_equal(parsed, "\200");
}

M_TEST_SUITE_DEFINE(StringParser,
	cmocka_unit_test(nullString),
	cmocka_unit_test(emptyString),
	cmocka_unit_test(emptyString2),
	cmocka_unit_test(plainString),
	cmocka_unit_test(plainString2),
	cmocka_unit_test(leadingString),
	cmocka_unit_test(trailingString),
	cmocka_unit_test(backslashString),
	cmocka_unit_test(doubleBackslashString),
	cmocka_unit_test(escapeCharsString),
	cmocka_unit_test(invalidEscapeCharString),
	cmocka_unit_test(overflowString),
	cmocka_unit_test(unclosedString),
	cmocka_unit_test(unclosedString2),
	cmocka_unit_test(unclosedBackslashString),
	cmocka_unit_test(unclosedBackslashString2),
	cmocka_unit_test(highCharacterString))
