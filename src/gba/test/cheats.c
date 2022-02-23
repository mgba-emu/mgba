/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/core/cheats.h>
#include <mgba/core/core.h>
#include <mgba/gba/core.h>
#include <mgba/internal/gba/cheats.h>

#include "gba/cheats/parv3.h"
#include "gba/cheats/gameshark.h"

static int cheatsSetup(void** state) {
	struct mCore* core = GBACoreCreate();
	core->init(core);
	mCoreInitConfig(core, NULL);
	core->cheatDevice(core);
	*state = core;
	return 0;
}

static int cheatsTeardown(void** state) {
	if (!*state) {
		return 0;
	}
	struct mCore* core = *state;
	mCoreConfigDeinit(&core->config);
	core->deinit(core);
	return 0;
}

M_TEST_DEFINE(createSet) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	mCheatSetDeinit(set);
}

M_TEST_DEFINE(addRawPARv3) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "80000000 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_false(set->addLine(set, "43000000 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3Assign) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00300000 00000078", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "02300002 00005678", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "04300004 12345678", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead16(core, 0x03000002, -1), 0);
	assert_int_equal(core->rawRead32(core, 0x03000004, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x78);
	assert_int_equal(core->rawRead16(core, 0x03000002, -1), 0x5678);
	assert_int_equal(core->rawRead32(core, 0x03000004, -1), 0x12345678);

	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3Slide1) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00000000 80300000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000001 01020002", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 2);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0);

	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3Slide2) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00000000 82300000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000001 01020002", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead16(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead16(core, 0x03000002, -1), 0);
	assert_int_equal(core->rawRead16(core, 0x03000004, -1), 0);
	assert_int_equal(core->rawRead16(core, 0x03000006, -1), 0);
	assert_int_equal(core->rawRead16(core, 0x03000008, -1), 0);
	assert_int_equal(core->rawRead16(core, 0x0300000A, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead16(core, 0x03000000, -1), 1);
	assert_int_equal(core->rawRead16(core, 0x03000002, -1), 0);
	assert_int_equal(core->rawRead16(core, 0x03000004, -1), 2);
	assert_int_equal(core->rawRead16(core, 0x03000006, -1), 0);
	assert_int_equal(core->rawRead16(core, 0x03000008, -1), 0);
	assert_int_equal(core->rawRead16(core, 0x0300000A, -1), 0);

	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3Slide4) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00000000 84300000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000001 01020002", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead32(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead32(core, 0x03000004, -1), 0);
	assert_int_equal(core->rawRead32(core, 0x03000008, -1), 0);
	assert_int_equal(core->rawRead32(core, 0x0300000C, -1), 0);
	assert_int_equal(core->rawRead32(core, 0x03000010, -1), 0);
	assert_int_equal(core->rawRead32(core, 0x03000014, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead16(core, 0x03000000, -1), 1);
	assert_int_equal(core->rawRead16(core, 0x03000004, -1), 0);
	assert_int_equal(core->rawRead16(core, 0x03000008, -1), 2);
	assert_int_equal(core->rawRead16(core, 0x0300000C, -1), 0);
	assert_int_equal(core->rawRead16(core, 0x03000010, -1), 0);
	assert_int_equal(core->rawRead16(core, 0x03000014, -1), 0);

	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3If1) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00300001 00000011", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "08300000 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300001 00000012", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300000 00000001", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x12);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x11);

	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3If1x1) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00300002 00000021", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300003 00000031", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "08300000 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300002 00000022", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "08300001 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300003 00000032", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300000 00000001", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x22);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x32);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x32);

	core->reset(core);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x22);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x31);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x31);

	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3If2) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00300001 00000011", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300002 00000021", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "48300000 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300001 00000012", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300002 00000022", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300000 00000001", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x12);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x22);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x11);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);

	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3If2x2) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00300002 00000021", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300003 00000031", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300004 00000041", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300005 00000051", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "48300000 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300002 00000022", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300003 00000032", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "48300001 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300004 00000042", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300005 00000052", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300000 00000001", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x22);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x32);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x42);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x52);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x31);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x42);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x52);

	core->reset(core);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x22);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x32);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x41);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x51);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x31);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x41);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x51);
	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3If2Contain1) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00300002 00000021", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "48300000 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "08300001 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300002 00000022", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300000 00000001", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x22);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);

	core->reset(core);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);
	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3IfX) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00300001 00000011", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "88300000 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300001 00000012", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 40000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300000 00000001", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x12);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x11);
	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3IfXxX) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00300002 00000021", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300003 00000031", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300004 00000041", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "88300000 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300002 00000022", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 40000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300003 00000032", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "88300001 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300004 00000042", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 40000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300000 00000001", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x22);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x32);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x42);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x32);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x42);

	core->reset(core);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x22);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x32);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x41);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x32);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x41);
	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3IfXElse) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00300001 00000011", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300002 00000021", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "88300000 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300001 00000012", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 60000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300002 00000022", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 40000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300000 00000001", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x12);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x11);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x22);
	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3IfXElsexX) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00300002 00000021", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300003 00000031", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300004 00000041", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300005 00000051", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "88300000 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300002 00000022", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 60000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300003 00000032", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 40000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300004 00000042", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "88300001 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300005 00000052", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 40000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300000 00000001", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x22);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x31);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x42);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x52);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x32);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x42);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x52);

	core->reset(core);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x22);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x31);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x42);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x51);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x32);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x42);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x51);
	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3IfXElsexXElse) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00300002 00000021", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300003 00000031", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300004 00000041", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300005 00000051", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300006 00000061", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "88300000 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300002 00000022", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 60000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300003 00000032", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 40000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300004 00000042", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "88300001 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300005 00000052", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 60000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300006 00000062", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 40000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300000 00000001", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000006, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x22);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x31);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x42);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x52);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x32);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x42);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x52);
	assert_int_equal(core->rawRead8(core, 0x03000006, -1), 0x61);

	core->reset(core);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x22);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x31);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x42);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x51);
	assert_int_equal(core->rawRead8(core, 0x03000006, -1), 0x62);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x32);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x42);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x51);
	assert_int_equal(core->rawRead8(core, 0x03000006, -1), 0x62);
	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3IfXContain1) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00300002 00000021", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300003 00000031", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300004 00000041", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "88300000 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300002 00000022", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "08300001 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300003 00000032", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300004 00000042", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 40000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300000 00000001", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x22);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x32);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x42);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x31);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x41);

	core->reset(core);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x22);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x31);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x42);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x31);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x41);
	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3IfXContain1Else) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00300002 00000021", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300003 00000031", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300004 00000041", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300005 00000051", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "88300000 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300002 00000022", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "08300001 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300003 00000032", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300004 00000042", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 60000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300005 00000052", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 40000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300000 00000001", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x22);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x32);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x42);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x51);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x31);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x41);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x52);

	core->reset(core);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x22);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x31);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x42);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x51);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x31);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x41);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x52);
	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3IfXElseContain1) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00300002 00000021", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300003 00000031", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300004 00000041", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300005 00000051", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "88300000 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300002 00000022", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 60000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300003 00000032", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "08300001 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300004 00000042", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300005 00000052", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 40000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300000 00000001", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x22);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x31);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x41);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x51);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x32);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x42);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x52);

	core->reset(core);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x22);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x31);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x41);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x51);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x32);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x41);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x52);
	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3IfXContain1ElseContain1) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00300003 00000031", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300004 00000041", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300005 00000051", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300006 00000061", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300007 00000071", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300008 00000081", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "88300000 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300003 00000032", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "08300001 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300004 00000042", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300005 00000052", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 60000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300006 00000062", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "08300002 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300007 00000072", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300008 00000082", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 40000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300000 00000001", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000006, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000007, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000008, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x32);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x42);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x52);
	assert_int_equal(core->rawRead8(core, 0x03000006, -1), 0x61);
	assert_int_equal(core->rawRead8(core, 0x03000007, -1), 0x71);
	assert_int_equal(core->rawRead8(core, 0x03000008, -1), 0x81);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x31);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x41);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x51);
	assert_int_equal(core->rawRead8(core, 0x03000006, -1), 0x62);
	assert_int_equal(core->rawRead8(core, 0x03000007, -1), 0x72);
	assert_int_equal(core->rawRead8(core, 0x03000008, -1), 0x82);

	core->reset(core);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x32);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x41);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x52);
	assert_int_equal(core->rawRead8(core, 0x03000006, -1), 0x61);
	assert_int_equal(core->rawRead8(core, 0x03000007, -1), 0x71);
	assert_int_equal(core->rawRead8(core, 0x03000008, -1), 0x81);

	core->reset(core);
	core->rawWrite8(core, 0x03000002, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x32);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x42);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x52);
	assert_int_equal(core->rawRead8(core, 0x03000006, -1), 0x61);
	assert_int_equal(core->rawRead8(core, 0x03000007, -1), 0x71);
	assert_int_equal(core->rawRead8(core, 0x03000008, -1), 0x81);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x31);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x41);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x51);
	assert_int_equal(core->rawRead8(core, 0x03000006, -1), 0x62);
	assert_int_equal(core->rawRead8(core, 0x03000007, -1), 0x72);
	assert_int_equal(core->rawRead8(core, 0x03000008, -1), 0x82);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	core->rawWrite8(core, 0x03000002, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x31);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x41);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x51);
	assert_int_equal(core->rawRead8(core, 0x03000006, -1), 0x62);
	assert_int_equal(core->rawRead8(core, 0x03000007, -1), 0x71);
	assert_int_equal(core->rawRead8(core, 0x03000008, -1), 0x82);

	core->reset(core);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	core->rawWrite8(core, 0x03000002, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x32);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x41);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x52);
	assert_int_equal(core->rawRead8(core, 0x03000006, -1), 0x61);
	assert_int_equal(core->rawRead8(core, 0x03000007, -1), 0x71);
	assert_int_equal(core->rawRead8(core, 0x03000008, -1), 0x81);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	core->rawWrite8(core, 0x03000001, -1, 0x1);
	core->rawWrite8(core, 0x03000002, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000003, -1), 0x31);
	assert_int_equal(core->rawRead8(core, 0x03000004, -1), 0x41);
	assert_int_equal(core->rawRead8(core, 0x03000005, -1), 0x51);
	assert_int_equal(core->rawRead8(core, 0x03000006, -1), 0x62);
	assert_int_equal(core->rawRead8(core, 0x03000007, -1), 0x71);
	assert_int_equal(core->rawRead8(core, 0x03000008, -1), 0x82);
	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3IfButton) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00000000 10300000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000001 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);

	mCheatPressButton(device, true);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);

	mCheatPressButton(device, false);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);

	core->rawWrite8(core, 0x03000000, -1, 0);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);
	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3Never1) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00300001 00000011", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "0E300000 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300001 00000012", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300000 00000001", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x11);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x11);

	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3Never2) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00300001 00000011", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300002 00000021", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "4E300000 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300001 00000012", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300002 00000022", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300000 00000001", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x11);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x11);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x21);

	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3NeverX) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00300001 00000011", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "8E300000 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300001 00000012", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 40000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300000 00000001", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x11);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x11);
	mCheatSetDeinit(set);
}

M_TEST_DEFINE(doPARv3NeverXElse) {
	struct mCore* core = *state;
	struct mCheatDevice* device = core->cheatDevice(core);
	assert_non_null(device);
	struct mCheatSet* set = device->createSet(device, NULL);
	assert_non_null(set);
	GBACheatSetGameSharkVersion((struct GBACheatSet*) set, GBA_GS_PARV3_RAW);
	assert_true(set->addLine(set, "00300001 00000011", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300002 00000021", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "8E300000 00000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300001 00000012", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 60000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300002 00000022", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00000000 40000000", GBA_CHEAT_PRO_ACTION_REPLAY));
	assert_true(set->addLine(set, "00300000 00000001", GBA_CHEAT_PRO_ACTION_REPLAY));

	core->reset(core);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0);

	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x11);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x22);

	core->reset(core);
	core->rawWrite8(core, 0x03000000, -1, 0x1);
	mCheatRefresh(device, set);
	assert_int_equal(core->rawRead8(core, 0x03000000, -1), 0x1);
	assert_int_equal(core->rawRead8(core, 0x03000001, -1), 0x11);
	assert_int_equal(core->rawRead8(core, 0x03000002, -1), 0x22);
	mCheatSetDeinit(set);
}

M_TEST_SUITE_DEFINE(GBACheats,
	cmocka_unit_test_setup_teardown(createSet, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(addRawPARv3, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3Assign, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3Slide1, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3Slide2, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3Slide4, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3If1, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3If1x1, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3If2, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3If2x2, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3If2Contain1, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3IfX, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3IfXxX, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3IfXElse, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3IfXElsexX, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3IfXElsexXElse, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3IfXContain1, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3IfXContain1Else, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3IfXElseContain1, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3IfXContain1ElseContain1, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3IfButton, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3Never1, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3Never2, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3NeverX, cheatsSetup, cheatsTeardown),
	cmocka_unit_test_setup_teardown(doPARv3NeverXElse, cheatsSetup, cheatsTeardown))
