/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/core/core.h>
#include <mgba/gb/core.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/mbc.h>
#include <mgba-util/vfs.h>

M_TEST_SUITE_SETUP(GBMemory) {
	struct VFile* vf = VFileMemChunk(NULL, GB_SIZE_CART_BANK0 * 4);
	GBSynthesizeROM(vf);
	struct mCore* core = GBCoreCreate();
	core->init(core);
	mCoreInitConfig(core, NULL);
	core->loadROM(core, vf);
	*state = core;
	return 0;
}

M_TEST_SUITE_TEARDOWN(GBMemory) {
	if (!*state) {
		return 0;
	}
	struct mCore* core = *state;
	mCoreConfigDeinit(&core->config);
	core->deinit(core);
	return 0;
}

M_TEST_DEFINE(patchROMBank0) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	int8_t oldExpected = gb->memory.rom[0];
	int8_t old;
	int8_t newExpected = 0x40;

	core->reset(core);
	GBPatch8(gb->cpu, 0, newExpected, &old, 0);
	assert_int_equal(old, oldExpected);
	assert_int_equal(gb->memory.rom[0], newExpected);
	assert_int_equal(GBView8(gb->cpu, 0, 0), newExpected);
}

M_TEST_DEFINE(patchROMBank1) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	int8_t oldExpected = gb->memory.rom[GB_SIZE_CART_BANK0];
	int8_t old;
	int8_t newExpected = 0x41;

	core->reset(core);
	GBPatch8(gb->cpu, GB_SIZE_CART_BANK0, newExpected, &old, 1);
	assert_int_equal(old, oldExpected);
	assert_int_equal(gb->memory.rom[GB_SIZE_CART_BANK0], newExpected);
	assert_int_equal(GBView8(gb->cpu, GB_SIZE_CART_BANK0, 1), newExpected);
}

M_TEST_DEFINE(patchROMBank2) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	int8_t oldExpected = gb->memory.rom[GB_SIZE_CART_BANK0 * 2];
	int8_t old;
	int8_t newExpected = 0x42;

	core->reset(core);
	GBPatch8(gb->cpu, GB_SIZE_CART_BANK0, newExpected, &old, 2);
	assert_int_equal(old, oldExpected);
	assert_int_equal(gb->memory.rom[GB_SIZE_CART_BANK0 * 2], newExpected);
	assert_int_equal(GBView8(gb->cpu, GB_SIZE_CART_BANK0, 2), newExpected);
}

M_TEST_SUITE_DEFINE_SETUP_TEARDOWN(GBMemory,
	cmocka_unit_test(patchROMBank0),
	cmocka_unit_test(patchROMBank1),
	cmocka_unit_test(patchROMBank2))
