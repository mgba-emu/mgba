/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SUITE_H
#define SUITE_H
#include "util/common.h"

#include <setjmp.h>
#include <cmocka.h>

#define M_TEST_DEFINE(NAME) static void NAME (void **state ATTRIBUTE_UNUSED)

#define M_TEST_SUITE(NAME) _testSuite_ ## NAME
#define M_TEST_SUITE_RUN(NAME) M_TEST_SUITE(NAME)()
#define M_TEST_SUITE_DEFINE(NAME, ...) \
	int M_TEST_SUITE(NAME) (void) { \
		const static struct CMUnitTest tests[] = { \
			__VA_ARGS__ \
		}; \
		return cmocka_run_group_tests_name(# NAME, tests, NULL, NULL); \
	}

#define M_TEST_SUITE_DECLARE(NAME) extern int M_TEST_SUITE(NAME) (void)

#endif