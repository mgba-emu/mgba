/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/core/scripting.h>
#include <mgba/internal/script/lua.h>
#include <mgba/script/context.h>
#include <mgba/script/types.h>

#include "script/test.h"

#ifdef M_CORE_GBA
#include <mgba/internal/gba/memory.h>
#define TEST_PLATFORM mPLATFORM_GBA
#define RAM_BASE GBA_BASE_IWRAM
#elif defined(M_CORE_GB)
#include <mgba/internal/gb/memory.h>
#define TEST_PLATFORM mPLATFORM_GB
#define RAM_BASE GB_BASE_WORKING_RAM_BANK0
#else
#error "Need a valid platform for testing"
#endif

struct mScriptTestLogger {
	struct mLogger d;
	char* log;
	char* warn;
	char* error;
};

static const uint8_t _fakeGBROM[0x4000] = {
	[0x100] = 0x18, // Loop forever
	[0x101] = 0xFE, // jr, $-2
	[0x102] = 0xCE, // Enough of the header to fool the core
	[0x103] = 0xED,
	[0x104] = 0x66,
	[0x105] = 0x66,
};

#define SETUP_LUA \
	struct mScriptContext context; \
	mScriptContextInit(&context); \
	struct mScriptEngineContext* lua = mScriptContextRegisterEngine(&context, mSCRIPT_ENGINE_LUA)

#define CREATE_CORE \
	struct mCore* core = mCoreCreate(TEST_PLATFORM); \
	assert_non_null(core); \
	assert_true(core->init(core)); \
	switch (core->platform(core)) { \
	case mPLATFORM_GBA: \
		core->busWrite32(core, 0x020000C0, 0xEAFFFFFE); \
		break; \
	case mPLATFORM_GB: \
		assert_true(core->loadROM(core, VFileFromConstMemory(_fakeGBROM, sizeof(_fakeGBROM)))); \
		break; \
	case mPLATFORM_NONE: \
		break; \
	} \
	mCoreInitConfig(core, NULL); \
	mScriptContextAttachCore(&context, core)

#define TEARDOWN_CORE \
	mCoreConfigDeinit(&core->config); \
	core->deinit(core)

static void _mScriptTestLog(struct mLogger* log, int category, enum mLogLevel level, const char* format, va_list args) {
	UNUSED(category);
	struct mScriptTestLogger* logger = (struct mScriptTestLogger*) log;

	char* message;
#ifdef HAVE_VASPRINTF
	vasprintf(&message, format, args);
#else
	char messageBuf[64];
	vsnprintf(messageBuf, format, args);
	message = strdup(messageBuf);
#endif
	switch (level) {
	case mLOG_INFO:
		if (logger->log) {
			free(logger->log);
		}
		logger->log = message;
		break;
	case mLOG_WARN:
		if (logger->warn) {
			free(logger->warn);
		}
		logger->warn = message;
		break;
	case mLOG_ERROR:
		if (logger->error) {
			free(logger->error);
		}
		logger->error = message;
		break;
	default:
		free(message);
	}
}

static void mScriptTestLoggerInit(struct mScriptTestLogger* logger) {
	memset(logger, 0, sizeof(*logger));
	logger->d.log = _mScriptTestLog;
}

static void mScriptTestLoggerDeinit(struct mScriptTestLogger* logger) {
	if (logger->log) {
		free(logger->log);
	}
	if (logger->warn) {
		free(logger->warn);
	}
	if (logger->error) {
		free(logger->error);
	}
}

M_TEST_SUITE_SETUP(mScriptCore) {
	if (mSCRIPT_ENGINE_LUA->init) {
		mSCRIPT_ENGINE_LUA->init(mSCRIPT_ENGINE_LUA);
	}
	return 0;
}

M_TEST_SUITE_TEARDOWN(mScriptCore) {
	if (mSCRIPT_ENGINE_LUA->deinit) {
		mSCRIPT_ENGINE_LUA->deinit(mSCRIPT_ENGINE_LUA);
	}
	return 0;
}

M_TEST_DEFINE(globals) {
	SETUP_LUA;
	CREATE_CORE;
	core->reset(core);

	LOAD_PROGRAM("assert(emu)");
	assert_true(lua->run(lua));

	TEARDOWN_CORE;
	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(infoFuncs) {
	SETUP_LUA;
	CREATE_CORE;
	core->reset(core);

	LOAD_PROGRAM(
		"frequency = emu:frequency()\n"
		"frameCycles = emu:frameCycles()\n"
	);
	assert_true(lua->run(lua));

	TEST_VALUE(S32, "frequency", core->frequency(core));
	TEST_VALUE(S32, "frameCycles", core->frameCycles(core));

	TEARDOWN_CORE;
	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(detach) {
	SETUP_LUA;
	CREATE_CORE;
	core->reset(core);

	LOAD_PROGRAM(
		"assert(emu)\n"
		"assert(emu.memory)\n"
		"a = emu\n"
		"b = emu.memory\n"
	);
	assert_true(lua->run(lua));

	mScriptContextDetachCore(&context);

	LOAD_PROGRAM(
		"assert(not emu)\n"
	);
	assert_true(lua->run(lua));

	LOAD_PROGRAM(
		"a:frequency()\n"
	);
	assert_false(lua->run(lua));

	LOAD_PROGRAM(
		"assert(memory.cart0)\n"
	);
	assert_false(lua->run(lua));

	TEARDOWN_CORE;
	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(runFrame) {
	SETUP_LUA;
	CREATE_CORE;
	core->reset(core);

	LOAD_PROGRAM(
		"frame = emu:currentFrame()\n"
		"emu:runFrame()\n"
	);

	int i;
	for (i = 0; i < 5; ++i) {
		assert_true(lua->run(lua));
		TEST_VALUE(S32, "frame", i);
	}

	TEARDOWN_CORE;
	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(memoryRead) {
	SETUP_LUA;
	CREATE_CORE;
	core->reset(core);

	LOAD_PROGRAM(
		"a8 = emu:read8(base + 0)\n"
		"b8 = emu:read8(base + 1)\n"
		"c8 = emu:read8(base + 2)\n"
		"d8 = emu:read8(base + 3)\n"
		"a16 = emu:read16(base + 4)\n"
		"b16 = emu:read16(base + 6)\n"
		"a32 = emu:read32(base + 8)\n"
	);

	int i;
	for (i = 0; i < 12; ++i) {
		core->busWrite8(core, RAM_BASE + i, i + 1);
	}
	struct mScriptValue base = mSCRIPT_MAKE_S32(RAM_BASE);
	lua->setGlobal(lua, "base", &base);
	assert_true(lua->run(lua));

	TEST_VALUE(S32, "a8", 1);
	TEST_VALUE(S32, "b8", 2);
	TEST_VALUE(S32, "c8", 3);
	TEST_VALUE(S32, "d8", 4);
	TEST_VALUE(S32, "a16", 0x0605);
	TEST_VALUE(S32, "b16", 0x0807);
	TEST_VALUE(S32, "a32", 0x0C0B0A09);

	TEARDOWN_CORE;
	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(memoryWrite) {
	SETUP_LUA;
	CREATE_CORE;
	core->reset(core);

	LOAD_PROGRAM(
		"emu:write8(base + 0, 1)\n"
		"emu:write8(base + 1, 2)\n"
		"emu:write8(base + 2, 3)\n"
		"emu:write8(base + 3, 4)\n"
		"emu:write16(base + 4, 0x0605)\n"
		"emu:write16(base + 6, 0x0807)\n"
		"emu:write32(base + 8, 0x0C0B0A09)\n"
	);

	struct mScriptValue base = mSCRIPT_MAKE_S32(RAM_BASE);
	lua->setGlobal(lua, "base", &base);
	assert_true(lua->run(lua));

	int i;
	for (i = 0; i < 12; ++i) {
		assert_int_equal(core->busRead8(core, RAM_BASE + i), i + 1);
	}

	TEARDOWN_CORE;
	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(logging) {
	SETUP_LUA;
	struct mScriptTestLogger logger;
	mScriptTestLoggerInit(&logger);

	mScriptContextAttachLogger(&context, &logger.d);

	LOAD_PROGRAM(
		"assert(console)\n"
		"console:log(\"log\")\n"
		"console:warn(\"warn\")\n"
		"console:error(\"error\")\n"
		"a = console\n"
	);

	assert_true(lua->run(lua));
	assert_non_null(logger.log);
	assert_non_null(logger.warn);
	assert_non_null(logger.error);
	assert_string_equal(logger.log, "log");
	assert_string_equal(logger.warn, "warn");
	assert_string_equal(logger.error, "error");

	mScriptContextDetachLogger(&context);
	mScriptTestLoggerDeinit(&logger);
	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(screenshot) {
	SETUP_LUA;
	CREATE_CORE;
	color_t* buffer = malloc(240 * 160 * sizeof(color_t));
	core->setVideoBuffer(core, buffer, 240);
	core->reset(core);
	core->runFrame(core);

	TEST_PROGRAM("im = emu:screenshotToImage()");
	TEST_PROGRAM("assert(im)");
	TEST_PROGRAM("assert(im.width >= 160)");
	TEST_PROGRAM("assert(im.height >= 144)");

	TEARDOWN_CORE;
	free(buffer);
	mScriptContextDeinit(&context);
}

M_TEST_SUITE_DEFINE_SETUP_TEARDOWN(mScriptCore,
	cmocka_unit_test(globals),
	cmocka_unit_test(infoFuncs),
	cmocka_unit_test(detach),
	cmocka_unit_test(runFrame),
	cmocka_unit_test(memoryRead),
	cmocka_unit_test(memoryWrite),
	cmocka_unit_test(logging),
	cmocka_unit_test(screenshot),
)
