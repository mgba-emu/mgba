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

struct GBRTCTest {
	struct mRTCSource d;
	struct mCore* core;
	struct VFile* fakeSave;
	time_t nextTime;
};

static void _sampleRtc(struct GB* gb) {
	gb->memory.rtcLastLatch = 0;
	GBStore8(gb->cpu, 0x6000, 0);
	GBStore8(gb->cpu, 0x6000, 1);
}

static void _sampleRtcKeepLatch(struct GB* gb) {
	GBStore8(gb->cpu, 0x6000, 0);
	GBStore8(gb->cpu, 0x6000, 1);
}

static time_t _testTime(struct mRTCSource* source) {
	struct GBRTCTest* test = (struct GBRTCTest*) source;
	return test->nextTime;
}

M_TEST_SUITE_SETUP(GBRTC) {
	struct GBRTCTest* test = malloc(sizeof(*test));
	if (!test) {
		return -1;
	}
	test->core = GBCoreCreate();
	test->d.sample = NULL;
	test->d.unixTime = _testTime;
	test->nextTime = 0;
	if (!test->core) {
		*state = NULL;
		return -1;
	}
	test->core->init(test->core);
	mCoreInitConfig(test->core, NULL);
	struct VFile* vf = VFileMemChunk(NULL, 2048);
	GBSynthesizeROM(vf);
	test->core->loadROM(test->core, vf);
	mCoreSetRTC(test->core, &test->d);

	test->fakeSave = VFileMemChunk(NULL, 0);
	test->core->loadSave(test->core, test->fakeSave);

	struct GB* gb = test->core->board;
	gb->memory.mbcType = GB_MBC3_RTC;

	*state = test;
	return 0;
}

M_TEST_SUITE_TEARDOWN(GBRTC) {
	if (!*state) {
		return 0;
	}
	struct GBRTCTest* test = *state;
	mCoreConfigDeinit(&test->core->config);
	test->core->deinit(test->core);
	free(test);
	return 0;
}

M_TEST_DEFINE(create) {
	struct GBRTCTest* test = *state;
	test->core->reset(test->core);
	struct GB* gb = test->core->board;

	uint8_t expected[sizeof(gb->memory.rtcRegs)] = { 0, 0, 0, 0, 0 };
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
	assert_int_equal(gb->memory.mbcType, GB_MBC3_RTC);
}

M_TEST_DEFINE(tickSecond) {
	struct GBRTCTest* test = *state;
	test->nextTime = 0;
	test->core->reset(test->core);
	struct GB* gb = test->core->board;
	test->nextTime = 1;

	uint8_t expected[sizeof(gb->memory.rtcRegs)] = { 0, 0, 0, 0, 0 };
	memset(gb->memory.rtcRegs, 0, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	expected[0] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 1;
	expected[0] = 2;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 59;
	gb->memory.rtcRegs[1] = 0;
	expected[0] = 0;
	expected[1] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
}

M_TEST_DEFINE(tick30Seconds) {
	struct GBRTCTest* test = *state;
	test->nextTime = 0;
	test->core->reset(test->core);
	struct GB* gb = test->core->board;
	test->nextTime = 30;

	uint8_t expected[sizeof(gb->memory.rtcRegs)] = { 0, 0, 0, 0, 0 };
	memset(gb->memory.rtcRegs, 0, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	expected[0] = 30;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 1;
	expected[0] = 31;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 30;
	gb->memory.rtcRegs[1] = 0;
	expected[0] = 0;
	expected[1] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 59;
	gb->memory.rtcRegs[1] = 0;
	expected[0] = 29;
	expected[1] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
}

M_TEST_DEFINE(tickMinute) {
	struct GBRTCTest* test = *state;
	test->nextTime = 0;
	test->core->reset(test->core);
	struct GB* gb = test->core->board;
	test->nextTime = 60;

	uint8_t expected[sizeof(gb->memory.rtcRegs)] = { 0, 0, 0, 0, 0 };
	memset(gb->memory.rtcRegs, 0, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 0;
	expected[0] = 0;
	expected[1] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 1;
	gb->memory.rtcRegs[1] = 0;
	expected[0] = 1;
	expected[1] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 59;
	gb->memory.rtcRegs[2] = 0;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 1;
	gb->memory.rtcRegs[1] = 59;
	gb->memory.rtcRegs[2] = 0;
	expected[0] = 1;
	expected[1] = 0;
	expected[2] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
}

M_TEST_DEFINE(tick90Seconds) {
	struct GBRTCTest* test = *state;
	test->nextTime = 0;
	test->core->reset(test->core);
	struct GB* gb = test->core->board;
	test->nextTime = 90;

	uint8_t expected[sizeof(gb->memory.rtcRegs)] = { 0, 0, 0, 0, 0 };
	memset(gb->memory.rtcRegs, 0, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 0;
	expected[0] = 30;
	expected[1] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 1;
	gb->memory.rtcRegs[1] = 0;
	expected[0] = 31;
	expected[1] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 30;
	gb->memory.rtcRegs[1] = 0;
	expected[0] = 0;
	expected[1] = 2;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 59;
	gb->memory.rtcRegs[1] = 0;
	expected[0] = 29;
	expected[1] = 2;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 59;
	gb->memory.rtcRegs[2] = 0;
	expected[0] = 30;
	expected[1] = 0;
	expected[2] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 1;
	gb->memory.rtcRegs[1] = 59;
	gb->memory.rtcRegs[2] = 0;
	expected[0] = 31;
	expected[1] = 0;
	expected[2] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
}

M_TEST_DEFINE(tick30Minutes) {
	struct GBRTCTest* test = *state;
	test->nextTime = 0;
	test->core->reset(test->core);
	struct GB* gb = test->core->board;
	test->nextTime = 1800;

	uint8_t expected[sizeof(gb->memory.rtcRegs)] = { 0, 0, 0, 0, 0 };
	memset(gb->memory.rtcRegs, 0, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 0;
	expected[0] = 0;
	expected[1] = 30;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 1;
	gb->memory.rtcRegs[1] = 0;
	expected[0] = 1;
	expected[1] = 30;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 59;
	gb->memory.rtcRegs[2] = 0;
	expected[0] = 0;
	expected[1] = 29;
	expected[2] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 1;
	gb->memory.rtcRegs[1] = 59;
	gb->memory.rtcRegs[2] = 0;
	expected[0] = 1;
	expected[1] = 29;
	expected[2] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
}

M_TEST_DEFINE(tickHour) {
	struct GBRTCTest* test = *state;
	test->nextTime = 0;
	test->core->reset(test->core);
	struct GB* gb = test->core->board;
	test->nextTime = 3600;

	uint8_t expected[sizeof(gb->memory.rtcRegs)] = { 0, 0, 0, 0, 0 };
	memset(gb->memory.rtcRegs, 0, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 0;
	gb->memory.rtcRegs[2] = 0;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 1;
	gb->memory.rtcRegs[1] = 0;
	gb->memory.rtcRegs[2] = 0;
	expected[0] = 1;
	expected[1] = 0;
	expected[2] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 1;
	gb->memory.rtcRegs[2] = 0;
	expected[0] = 0;
	expected[1] = 1;
	expected[2] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 0;
	gb->memory.rtcRegs[2] = 1;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 2;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 0;
	gb->memory.rtcRegs[2] = 23;
	gb->memory.rtcRegs[3] = 0;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
}

M_TEST_DEFINE(tick12Hours) {
	struct GBRTCTest* test = *state;
	test->nextTime = 0;
	test->core->reset(test->core);
	struct GB* gb = test->core->board;
	test->nextTime = 3600 * 12;

	uint8_t expected[sizeof(gb->memory.rtcRegs)] = { 0, 0, 0, 0, 0 };
	memset(gb->memory.rtcRegs, 0, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 0;
	gb->memory.rtcRegs[2] = 0;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 12;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 1;
	gb->memory.rtcRegs[1] = 0;
	gb->memory.rtcRegs[2] = 0;
	expected[0] = 1;
	expected[1] = 0;
	expected[2] = 12;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 1;
	gb->memory.rtcRegs[2] = 0;
	expected[0] = 0;
	expected[1] = 1;
	expected[2] = 12;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 1;
	gb->memory.rtcRegs[1] = 1;
	gb->memory.rtcRegs[2] = 0;
	expected[0] = 1;
	expected[1] = 1;
	expected[2] = 12;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 0;
	gb->memory.rtcRegs[2] = 12;
	gb->memory.rtcRegs[3] = 0;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 0;
	gb->memory.rtcRegs[2] = 23;
	gb->memory.rtcRegs[3] = 0;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 11;
	expected[3] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
}

M_TEST_DEFINE(tickDay) {
	struct GBRTCTest* test = *state;
	test->nextTime = 0;
	test->core->reset(test->core);
	struct GB* gb = test->core->board;
	test->nextTime = 3600 * 24;

	uint8_t expected[sizeof(gb->memory.rtcRegs)] = { 0, 0, 0, 0, 0 };
	memset(gb->memory.rtcRegs, 0, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 0;
	gb->memory.rtcRegs[2] = 0;
	gb->memory.rtcRegs[3] = 0;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 1;
	gb->memory.rtcRegs[1] = 0;
	gb->memory.rtcRegs[2] = 0;
	gb->memory.rtcRegs[3] = 0;
	expected[0] = 1;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 1;
	gb->memory.rtcRegs[2] = 0;
	gb->memory.rtcRegs[3] = 0;
	expected[0] = 0;
	expected[1] = 1;
	expected[2] = 0;
	expected[3] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 1;
	gb->memory.rtcRegs[1] = 1;
	gb->memory.rtcRegs[2] = 0;
	gb->memory.rtcRegs[3] = 0;
	expected[0] = 1;
	expected[1] = 1;
	expected[2] = 0;
	expected[3] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 0;
	gb->memory.rtcRegs[2] = 12;
	gb->memory.rtcRegs[3] = 0;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 12;
	expected[3] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 0;
	gb->memory.rtcRegs[2] = 23;
	gb->memory.rtcRegs[3] = 0;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 23;
	expected[3] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 0;
	gb->memory.rtcRegs[2] = 0;
	gb->memory.rtcRegs[3] = 1;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 2;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 0;
	gb->memory.rtcRegs[2] = 1;
	gb->memory.rtcRegs[3] = 1;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 1;
	expected[3] = 2;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
}

M_TEST_DEFINE(wideTickDay) {
	struct GBRTCTest* test = *state;
	test->nextTime = 0;
	test->core->reset(test->core);
	struct GB* gb = test->core->board;
	test->nextTime = 3600 * 24 * 2001;

	uint8_t expected[sizeof(gb->memory.rtcRegs)] = { 0, 0, 0, 0, 0 };
	memset(gb->memory.rtcRegs, 0, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 0;
	gb->memory.rtcRegs[2] = 0;
	gb->memory.rtcRegs[3] = 0;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 1;
	gb->memory.rtcLastLatch = 3600 * 24 * 2000;
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 1;
	gb->memory.rtcRegs[1] = 0;
	gb->memory.rtcRegs[2] = 0;
	gb->memory.rtcRegs[3] = 0;
	expected[0] = 1;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 1;
	gb->memory.rtcLastLatch = 3600 * 24 * 2000;
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 1;
	gb->memory.rtcRegs[2] = 0;
	gb->memory.rtcRegs[3] = 0;
	expected[0] = 0;
	expected[1] = 1;
	expected[2] = 0;
	expected[3] = 1;
	gb->memory.rtcLastLatch = 3600 * 24 * 2000;
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 1;
	gb->memory.rtcRegs[1] = 1;
	gb->memory.rtcRegs[2] = 0;
	gb->memory.rtcRegs[3] = 0;
	expected[0] = 1;
	expected[1] = 1;
	expected[2] = 0;
	expected[3] = 1;
	gb->memory.rtcLastLatch = 3600 * 24 * 2000;
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 0;
	gb->memory.rtcRegs[2] = 12;
	gb->memory.rtcRegs[3] = 0;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 12;
	expected[3] = 1;
	gb->memory.rtcLastLatch = 3600 * 24 * 2000;
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 0;
	gb->memory.rtcRegs[2] = 23;
	gb->memory.rtcRegs[3] = 0;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 23;
	expected[3] = 1;
	gb->memory.rtcLastLatch = 3600 * 24 * 2000;
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 0;
	gb->memory.rtcRegs[2] = 0;
	gb->memory.rtcRegs[3] = 1;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 2;
	gb->memory.rtcLastLatch = 3600 * 24 * 2000;
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 0;
	gb->memory.rtcRegs[1] = 0;
	gb->memory.rtcRegs[2] = 1;
	gb->memory.rtcRegs[3] = 1;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 1;
	expected[3] = 2;
	gb->memory.rtcLastLatch = 3600 * 24 * 2000;
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
}

M_TEST_DEFINE(rolloverSecond) {
	struct GBRTCTest* test = *state;
	test->nextTime = 0;
	test->core->reset(test->core);
	struct GB* gb = test->core->board;
	test->nextTime = 1;

	uint8_t expected[sizeof(gb->memory.rtcRegs)] = { 0, 0, 0, 0, 0 };
	memset(gb->memory.rtcRegs, 0, sizeof(expected));

	gb->memory.rtcRegs[0] = 59;
	gb->memory.rtcRegs[1] = 0;
	expected[0] = 0;
	expected[1] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 59;
	gb->memory.rtcRegs[1] = 59;
	gb->memory.rtcRegs[2] = 0;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 59;
	gb->memory.rtcRegs[1] = 59;
	gb->memory.rtcRegs[2] = 23;
	gb->memory.rtcRegs[3] = 0;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 59;
	gb->memory.rtcRegs[1] = 59;
	gb->memory.rtcRegs[2] = 23;
	gb->memory.rtcRegs[3] = 0xFF;
	gb->memory.rtcRegs[4] = 0;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 0;
	expected[4] = 1;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	gb->memory.rtcRegs[0] = 59;
	gb->memory.rtcRegs[1] = 59;
	gb->memory.rtcRegs[2] = 23;
	gb->memory.rtcRegs[3] = 0xFF;
	gb->memory.rtcRegs[4] = 1;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 0;
	expected[4] = 0x80;
	_sampleRtc(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
}

M_TEST_DEFINE(roundtrip0) {
	struct GBRTCTest* test = *state;
	test->nextTime = 0;
	test->fakeSave->truncate(test->fakeSave, 0);
	test->core->reset(test->core);
	struct GB* gb = test->core->board;

	uint8_t expected[sizeof(gb->memory.rtcRegs)] = { 0, 0, 0, 0, 0 };
	memset(gb->memory.rtcRegs, 0, sizeof(expected));
	GBMBCRTCWrite(gb);

	GBMBCRTCRead(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	GBMBCRTCRead(gb);
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	GBMBCRTCWrite(gb);
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
	GBMBCRTCRead(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	test->nextTime = 3600;
	expected[2] = 1;
	GBMBCRTCWrite(gb);
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
	GBMBCRTCRead(gb);
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	GBMBCRTCWrite(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
	GBMBCRTCRead(gb);
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	test->nextTime = 3600 * 2;
	expected[2] = 2;
	GBMBCRTCWrite(gb);
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
	GBMBCRTCRead(gb);
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
}

M_TEST_DEFINE(roundtripSecond) {
	struct GBRTCTest* test = *state;
	test->nextTime = 0;
	test->fakeSave->truncate(test->fakeSave, 0);
	test->core->reset(test->core);
	struct GB* gb = test->core->board;

	uint8_t expected[sizeof(gb->memory.rtcRegs)] = { 0, 0, 0, 0, 0 };
	memset(gb->memory.rtcRegs, 0, sizeof(expected));
	GBMBCRTCWrite(gb);

	test->nextTime = 1;
	GBMBCRTCRead(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	expected[0] = 1;
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	GBMBCRTCWrite(gb);

	test->nextTime = 2;
	GBMBCRTCRead(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
	_sampleRtcKeepLatch(gb);
	expected[0] = 2;
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	test->core->reset(test->core);
	expected[0] = 1;
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
	_sampleRtcKeepLatch(gb);
	expected[0] = 2;
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	test->core->reset(test->core);
	expected[0] = 1;
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
	GBMBCRTCRead(gb);
	_sampleRtcKeepLatch(gb);
	expected[0] = 2;
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	test->nextTime = 3;
	test->core->reset(test->core);
	expected[0] = 1;
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
	_sampleRtcKeepLatch(gb);
	expected[0] = 3;
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	test->nextTime = 1;
	test->core->reset(test->core);
	expected[0] = 1;
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
	test->nextTime = 4;
	_sampleRtcKeepLatch(gb);
	expected[0] = 4;
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
}

M_TEST_DEFINE(roundtripDay) {
	struct GBRTCTest* test = *state;
	test->nextTime = 0;
	test->fakeSave->truncate(test->fakeSave, 0);
	test->core->reset(test->core);
	struct GB* gb = test->core->board;

	uint8_t expected[sizeof(gb->memory.rtcRegs)] = { 0, 0, 0, 0, 0 };
	memset(gb->memory.rtcRegs, 0, sizeof(expected));
	GBMBCRTCWrite(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	test->nextTime = 3600 * 24;
	GBMBCRTCRead(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 1;
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	test->nextTime = 3600 * 24 * 2;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 2;
	GBMBCRTCRead(gb);
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	GBMBCRTCWrite(gb);

	test->nextTime = 3600 * 24 * 2;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 2;
	GBMBCRTCRead(gb);
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	test->nextTime = 3600 * 24 * 3;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 3;
	GBMBCRTCRead(gb);
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	test->nextTime = 3600 * 24 * 2;
	test->core->reset(test->core);
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 2;
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	test->nextTime = 3600 * 24 * 3;
	test->core->reset(test->core);
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 2;
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 3;
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
}

M_TEST_DEFINE(roundtripHuge) {
	struct GBRTCTest* test = *state;
	test->nextTime = 1500000000;
	test->fakeSave->truncate(test->fakeSave, 0);
	test->core->reset(test->core);
	struct GB* gb = test->core->board;

	uint8_t expected[sizeof(gb->memory.rtcRegs)] = { 0, 0, 0, 0, 0 };
	memset(gb->memory.rtcRegs, 0, sizeof(expected));
	GBMBCRTCWrite(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	test->nextTime = 1500000000 + 3600 * 24;
	GBMBCRTCRead(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 1;
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	test->nextTime = 1500000000 + 3600 * 24 * 2;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 2;
	GBMBCRTCRead(gb);
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	GBMBCRTCWrite(gb);

	test->nextTime = 1500000000 + 3600 * 24 * 2;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 2;
	GBMBCRTCRead(gb);
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	test->nextTime = 1500000000 + 3600 * 24 * 3;
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 3;
	GBMBCRTCRead(gb);
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	test->nextTime = 1500000000 + 3600 * 24 * 2;
	test->core->reset(test->core);
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 2;
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));

	test->nextTime = 1500000000 + 3600 * 24 * 3;
	test->core->reset(test->core);
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 2;
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
	expected[0] = 0;
	expected[1] = 0;
	expected[2] = 0;
	expected[3] = 3;
	_sampleRtcKeepLatch(gb);
	assert_memory_equal(gb->memory.rtcRegs, expected, sizeof(expected));
}

M_TEST_SUITE_DEFINE_SETUP_TEARDOWN(GBRTC,
	cmocka_unit_test(create),
	cmocka_unit_test(tickSecond),
	cmocka_unit_test(tick30Seconds),
	cmocka_unit_test(tickMinute),
	cmocka_unit_test(tick90Seconds),
	cmocka_unit_test(tick30Minutes),
	cmocka_unit_test(tickHour),
	cmocka_unit_test(tick12Hours),
	cmocka_unit_test(tickDay),
	cmocka_unit_test(wideTickDay),
	cmocka_unit_test(rolloverSecond),
	cmocka_unit_test(roundtrip0),
	cmocka_unit_test(roundtripSecond),
	cmocka_unit_test(roundtripDay),
	cmocka_unit_test(roundtripHuge))
