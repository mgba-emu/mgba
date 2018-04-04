/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/core/core.h>
#include <mgba-util/vfs.h>

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
M_TEST_DEFINE(findNullPath) {
	struct mCore* core = mCoreFind(NULL);
	assert_null(core);
}
#endif

M_TEST_DEFINE(findNullVF) {
	struct mCore* core = mCoreFindVF(NULL);
	assert_null(core);
}

M_TEST_DEFINE(findEmpty) {
	struct VFile* vf = VFileMemChunk(NULL, 0);
	assert_non_null(vf);
	struct mCore* core = mCoreFindVF(vf);
	assert_null(core);
	vf->close(vf);
}

M_TEST_SUITE_DEFINE(mCore,
#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	cmocka_unit_test(findNullPath),
#endif
	cmocka_unit_test(findNullVF),
	cmocka_unit_test(findEmpty))
