/* Copyright (c) 2013-2021 Jeffrey Pfau
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

#define GBX_FOOTER_SIZE 0x40

struct GBXParams {
	uint32_t major;
	uint32_t minor;
	char fourcc[4];
	bool battery;
	bool rumble;
	bool timer;
	uint32_t romSize;
	uint32_t ramSize;
	union {
		uint8_t u8[32];
		uint32_t u32[8];
	} mapperVars;
};

static struct VFile* makeGBX(const struct GBXParams* params, unsigned padding) {
	struct VFile* vf = VFileMemChunk(NULL, padding + GBX_FOOTER_SIZE);
	uint8_t bool2flag[2] = {0, 1};
	vf->seek(vf, -GBX_FOOTER_SIZE, SEEK_END);
	vf->write(vf, params->fourcc, 4);
	vf->write(vf, &bool2flag[(int) params->battery], 1);
	vf->write(vf, &bool2flag[(int) params->rumble], 1);
	vf->write(vf, &bool2flag[(int) params->timer], 1);
	vf->write(vf, &bool2flag[0], 1); // Reserved

	uint32_t beint;
	STORE_32BE(params->romSize, 0, &beint);
	vf->write(vf, &beint, 4);
	STORE_32BE(params->ramSize, 0, &beint);
	vf->write(vf, &beint, 4);
	vf->write(vf, &params->mapperVars, 0x20);

	STORE_32BE(0x40, 0, &beint); // Footer size
	vf->write(vf, &beint, 4);

	STORE_32BE(params->major, 0, &beint);
	vf->write(vf, &beint, 4);

	STORE_32BE(params->minor, 0, &beint);
	vf->write(vf, &beint, 4);

	vf->write(vf, "GBX!", 4); // Magic
	return vf;
}

M_TEST_SUITE_SETUP(GBGBX) {
	struct mCore* core = GBCoreCreate();
	core->init(core);
	mCoreInitConfig(core, NULL);
	*state = core;
	return 0;
}

M_TEST_SUITE_TEARDOWN(GBGBX) {
	if (!*state) {
		return 0;
	}
	struct mCore* core = *state;
	mCoreConfigDeinit(&core->config);
	core->deinit(core);
	return 0;
}

M_TEST_DEFINE(failTooSmall) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	char truncGbx[0x3F] = {
		[0x32] = 0x40,
		[0x36] = 0x1,
		[0x3B] = 'G',
		[0x3C] = 'B',
		[0x3D] = 'X',
		[0x3E] = '!',
	};
	struct VFile* vf = VFileFromConstMemory(truncGbx, sizeof(truncGbx));
	bool loaded = core->loadROM(core, vf);
	assert_false(loaded && gb->pristineRomSize == 0);
}

M_TEST_DEFINE(failNoMagic) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	char gbx[0x40] = {
		[0x0] = 'R',
		[0x1] = 'O',
		[0x2] = 'M',
		[0x33] = 0x40,
		[0x37] = 0x1,
		[0x3C] = 'G',
		[0x3D] = 'B',
		[0x3E] = 'X',
	};
	struct VFile* vf = VFileFromConstMemory(gbx, sizeof(gbx));
	bool loaded = core->loadROM(core, vf);
	assert_false(loaded && gb->pristineRomSize == 0);
}

M_TEST_DEFINE(invalidVersionLow) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 0,
		.fourcc = "ROM",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_false(loaded && gb->pristineRomSize == 0x8000);
}

M_TEST_DEFINE(invalidVersionHigh) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 2,
		.fourcc = "ROM",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_false(loaded && gb->pristineRomSize == 0x8000);
}

M_TEST_DEFINE(mbcInvalidNone) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "INVL",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_MBC_NONE);
}

M_TEST_DEFINE(mbcInvalidFallback) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "INVL",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	vf->seek(vf, 0x147, SEEK_SET);
	char one = 1;
	vf->write(vf, &one, 1);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_MBC1);
}

M_TEST_DEFINE(mbcRom) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "ROM",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_MBC_NONE);
}

M_TEST_DEFINE(mbc1) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "MBC1",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_MBC1);
	assert_int_equal(gb->memory.mbcState.mbc1.multicartStride, 5);
}

M_TEST_DEFINE(mbc2) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "MBC2",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_MBC2);
}

M_TEST_DEFINE(mbc3) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "MBC3",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_MBC3);
}

M_TEST_DEFINE(mbc3Rtc) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "MBC3",
		.timer = true,
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_MBC3_RTC);
}

M_TEST_DEFINE(mbc5) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "MBC5",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_MBC5);
}

M_TEST_DEFINE(mbc5Rumble) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "MBC5",
		.romSize = 0x8000,
		.rumble = true
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_MBC5_RUMBLE);
}

M_TEST_DEFINE(mbc6) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "MBC6",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_MBC6);
}

M_TEST_DEFINE(mbc7) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "MBC7",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_MBC7);
}

M_TEST_DEFINE(mbc1m) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "MB1M",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_MBC1);
	assert_int_equal(gb->memory.mbcState.mbc1.multicartStride, 4);
}

M_TEST_DEFINE(mmm01) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "MMM1",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_MMM01);
}

M_TEST_DEFINE(pocketCam) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "CAMR",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_POCKETCAM);
}

M_TEST_DEFINE(huc1) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "HUC1",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_HuC1);
}

M_TEST_DEFINE(huc3) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "HUC3",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_HuC3);
}

M_TEST_DEFINE(tama5) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "TAM5",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_TAMA5);
}

M_TEST_DEFINE(bbd) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "BBD",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_UNL_BBD);
}

M_TEST_DEFINE(hitek) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "HITK",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_UNL_HITEK);
}

M_TEST_DEFINE(ntNew) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "NTN",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_UNL_NT_NEW);
}

M_TEST_DEFINE(pkjd) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "PKJD",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_UNL_PKJD);
}

M_TEST_DEFINE(wisdomTree) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "WISD",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_UNL_WISDOM_TREE);
}

M_TEST_DEFINE(sachenMmc1) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "SAM1",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_UNL_SACHEN_MMC1);
}

M_TEST_DEFINE(sachenMmc2) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "SAM2",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_int_equal(gb->memory.mbcType, GB_UNL_SACHEN_MMC2);
}

M_TEST_DEFINE(resetMbc1m) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "MB1M",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	core->reset(core);
	assert_int_equal(gb->memory.mbcType, GB_MBC1);
	assert_int_equal(gb->memory.mbcState.mbc1.multicartStride, 4);
}

M_TEST_DEFINE(fakeRomSize) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "MBC1",
		.romSize = 0x8000
	};
	struct VFile* vf = makeGBX(&params, 0x10000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
}

M_TEST_DEFINE(fakeRamSize) {
	struct mCore* core = *state;
	struct GB* gb = core->board;
	struct GBXParams params = {
		.major = 1,
		.fourcc = "MBC1",
		.romSize = 0x8000,
		.ramSize = 0x4000
	};
	struct VFile* vf = makeGBX(&params, 0x8000);
	bool loaded = core->loadROM(core, vf);
	assert_true(loaded && gb->pristineRomSize == 0x8000);
	assert_true(gb->sramSize == 0x4000);
}

M_TEST_SUITE_DEFINE_SETUP_TEARDOWN(GBGBX,
	cmocka_unit_test(failTooSmall),
	cmocka_unit_test(failNoMagic),
	cmocka_unit_test(invalidVersionLow),
	cmocka_unit_test(invalidVersionHigh),
	cmocka_unit_test(mbcInvalidNone),
	cmocka_unit_test(mbcInvalidFallback),
	cmocka_unit_test(mbcRom),
	cmocka_unit_test(mbc1),
	cmocka_unit_test(mbc2),
	cmocka_unit_test(mbc3),
	cmocka_unit_test(mbc3Rtc),
	cmocka_unit_test(mbc5),
	cmocka_unit_test(mbc5Rumble),
	cmocka_unit_test(mbc6),
	cmocka_unit_test(mbc7),
	cmocka_unit_test(mbc1m),
	cmocka_unit_test(mmm01),
	cmocka_unit_test(pocketCam),
	cmocka_unit_test(huc1),
	cmocka_unit_test(huc3),
	cmocka_unit_test(tama5),
	cmocka_unit_test(bbd),
	cmocka_unit_test(hitek),
	cmocka_unit_test(ntNew),
	cmocka_unit_test(pkjd),
	cmocka_unit_test(wisdomTree),
	cmocka_unit_test(sachenMmc1),
	cmocka_unit_test(sachenMmc2),
	cmocka_unit_test(resetMbc1m),
	cmocka_unit_test(fakeRomSize),
	cmocka_unit_test(fakeRamSize))
