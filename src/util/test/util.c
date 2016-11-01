/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

M_TEST_SUITE_DECLARE(TextCodec);
M_TEST_SUITE_DECLARE(VFS);

int TestRunUtil(void) {
	int failures = 0;
	failures += M_TEST_SUITE_RUN(TextCodec);
	failures += M_TEST_SUITE_RUN(VFS);
	return failures;
}
