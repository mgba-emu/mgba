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

M_TEST_SUITE_SETUP(GBMBC) {
	struct VFile* vf = VFileMemChunk(NULL, 2048);
	GBSynthesizeROM(vf);
	struct mCore* core = GBCoreCreate();
	core->init(core);
	mCoreInitConfig(core, NULL);
	core->loadROM(core, vf);
	*state = core;
	return 0;
}

M_TEST_SUITE_TEARDOWN(GBMBC) {
	if (!*state) {
		return 0;
	}
	struct mCore* core = *state;
	mCoreConfigDeinit(&core->config);
	core->deinit(core);
	return 0;
}
M_TEST_DEFINE(detectNone) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBCartridge* cart = (struct GBCartridge*) &gb->memory.rom[0x100];

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x00;
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC_NONE);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x08;
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC_NONE);


	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x09;
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC_NONE);


	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x0A;
	core->reset(core);
	assert_int_not_equal(gb->memory.mbcType, GB_MBC_NONE);
}

M_TEST_DEFINE(detect1) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBCartridge* cart = (struct GBCartridge*) &gb->memory.rom[0x100];

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x00;
	core->reset(core);
	assert_int_not_equal(gb->memory.mbcType, GB_MBC1);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x01;
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC1);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x02;
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC1);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x03;
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC1);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x04;
	core->reset(core);
	assert_int_not_equal(gb->memory.mbcType, GB_MBC1);
}

M_TEST_DEFINE(detect2) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBCartridge* cart = (struct GBCartridge*) &gb->memory.rom[0x100];

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x04;
	core->reset(core);
	assert_int_not_equal(gb->memory.mbcType, GB_MBC2);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x05;
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC2);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x06;
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC2);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x07;
	core->reset(core);
	assert_int_not_equal(gb->memory.mbcType, GB_MBC2);
}

M_TEST_DEFINE(detect3) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBCartridge* cart = (struct GBCartridge*) &gb->memory.rom[0x100];

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x0E;
	core->reset(core);
	assert_int_not_equal(gb->memory.mbcType, GB_MBC3);
	assert_int_not_equal(gb->memory.mbcType, GB_MBC3_RTC);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x0F;
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC3_RTC);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x10;
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC3_RTC);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x11;
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC3);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x12;
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC3);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x13;
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC3);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x14;
	core->reset(core);
	assert_int_not_equal(gb->memory.mbcType, GB_MBC3);
	assert_int_not_equal(gb->memory.mbcType, GB_MBC3_RTC);
}

M_TEST_DEFINE(detect5) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBCartridge* cart = (struct GBCartridge*) &gb->memory.rom[0x100];

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x19;
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC5);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x1A;
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC5);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x1B;
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC5);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x1C;
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC5_RUMBLE);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x1D;
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC5_RUMBLE);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x1E;
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC5_RUMBLE);
}

M_TEST_DEFINE(detect6) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBCartridge* cart = (struct GBCartridge*) &gb->memory.rom[0x100];

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x1F;
	core->reset(core);
	assert_int_not_equal(gb->memory.mbcType, GB_MBC6);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x20;
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC6);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x21;
	core->reset(core);
	assert_int_not_equal(gb->memory.mbcType, GB_MBC6);
}

M_TEST_DEFINE(detect7) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBCartridge* cart = (struct GBCartridge*) &gb->memory.rom[0x100];

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x21;
	core->reset(core);
	assert_int_not_equal(gb->memory.mbcType, GB_MBC7);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x22;
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC7);

	gb->memory.mbcType = GB_MBC_AUTODETECT;
	cart->type = 0x23;
	core->reset(core);
	assert_int_not_equal(gb->memory.mbcType, GB_MBC7);
}

M_TEST_SUITE_DEFINE_SETUP_TEARDOWN(GBMBC,
	cmocka_unit_test(detectNone),
	cmocka_unit_test(detect1),
	cmocka_unit_test(detect2),
	cmocka_unit_test(detect3),
	cmocka_unit_test(detect5),
	cmocka_unit_test(detect6),
	cmocka_unit_test(detect7))
