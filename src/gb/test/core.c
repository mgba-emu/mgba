/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/core/core.h>
#include <mgba/gb/core.h>
#include <mgba/internal/gb/gb.h>
#include <mgba-util/vfs.h>

M_TEST_DEFINE(create) {
	struct mCore* core = GBCoreCreate();
	assert_non_null(core);
	assert_true(core->init(core));
	core->deinit(core);
}

M_TEST_DEFINE(platform) {
	struct mCore* core = GBCoreCreate();
	assert_non_null(core);
	assert_int_equal(core->platform(core), mPLATFORM_GB);
	assert_true(core->init(core));
	core->deinit(core);
}

M_TEST_DEFINE(reset) {
	struct mCore* core = GBCoreCreate();
	assert_non_null(core);
	assert_true(core->init(core));
	mCoreInitConfig(core, NULL);
	core->reset(core);
	mCoreConfigDeinit(&core->config);
	core->deinit(core);
}

M_TEST_DEFINE(loadNullROM) {
	struct mCore* core = GBCoreCreate();
	assert_non_null(core);
	assert_true(core->init(core));
	mCoreInitConfig(core, NULL);
	assert_false(core->loadROM(core, NULL));
	core->reset(core);
	mCoreConfigDeinit(&core->config);
	core->deinit(core);
}

M_TEST_DEFINE(isROM) {
	struct VFile* vf = VFileMemChunk(NULL, 2048);
	GBSynthesizeROM(vf);
	assert_true(GBIsROM(vf));
	struct mCore* core = mCoreFindVF(vf);
	assert_non_null(core);
	assert_int_equal(core->platform(core), mPLATFORM_GB);
	vf->close(vf);
	assert_true(core->init(core));

	core->deinit(core);
}

M_TEST_SUITE_DEFINE(GBCore,
	cmocka_unit_test(create),
	cmocka_unit_test(platform),
	cmocka_unit_test(reset),
	cmocka_unit_test(loadNullROM),
	cmocka_unit_test(isROM))
