/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include "util/test/util.h"
#include "core/test/core.h"
#ifdef M_CORE_GBA
#include "gba/test/gba.h"
#endif
#ifdef M_CORE_GB
#include "gb/test/gb.h"
#endif

int main() {
	int failures = TestRunUtil();
	failures += TestRunCore();
#ifdef M_CORE_GBA
	failures += TestRunGBA();
#endif
#ifdef M_CORE_GB
	failures += TestRunGB();
#endif
	return failures != 0;
}
