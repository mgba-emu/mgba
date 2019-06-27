/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SUITE_H
#define SUITE_H
#include <mgba-util/common.h>

#include <setjmp.h>
#include <cmocka.h>

#define M_TEST_DEFINE(NAME) static void NAME (void **state ATTRIBUTE_UNUSED)

#define M_TEST_SUITE_DEFINE(NAME, ...) M_TEST_SUITE_DEFINE_EX(NAME, NULL, NULL, __VA_ARGS__)
#define M_TEST_SUITE_DEFINE_SETUP(NAME, ...) M_TEST_SUITE_DEFINE_EX(NAME, _testSuite_setup_ ## NAME, NULL, __VA_ARGS__)
#define M_TEST_SUITE_DEFINE_TEARDOWN(NAME, ...) M_TEST_SUITE_DEFINE_EX(NAME, NULL, _testSuite_teardown_ ## NAME, __VA_ARGS__)
#define M_TEST_SUITE_DEFINE_SETUP_TEARDOWN(NAME, ...) M_TEST_SUITE_DEFINE_EX(NAME, _testSuite_setup_ ## NAME, _testSuite_teardown_ ## NAME, __VA_ARGS__)
#define M_TEST_SUITE_DEFINE_EX(NAME, SETUP, TEARDOWN, ...) \
	int main(void) { \
		static const struct CMUnitTest tests[] = { \
			__VA_ARGS__ \
		}; \
		return cmocka_run_group_tests_name(# NAME, tests, SETUP, TEARDOWN); \
	}

#define M_TEST_SUITE_SETUP(NAME) static int _testSuite_setup_ ## NAME (void **state ATTRIBUTE_UNUSED)
#define M_TEST_SUITE_TEARDOWN(NAME) static int _testSuite_teardown_ ## NAME (void **state ATTRIBUTE_UNUSED)

#endif
