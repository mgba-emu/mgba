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
#include <mgba/script.h>

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

	mScriptContextDeinit(&context);
	TEARDOWN_CORE;
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

	mScriptContextDeinit(&context);
	TEARDOWN_CORE;
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

	mScriptContextDeinit(&context);
	TEARDOWN_CORE;
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

	mScriptContextDeinit(&context);
	TEARDOWN_CORE;
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

	mScriptContextDeinit(&context);
	TEARDOWN_CORE;
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

	mScriptContextDeinit(&context);
	TEARDOWN_CORE;
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

	free(buffer);
	mScriptContextDeinit(&context);
	TEARDOWN_CORE;
}

#ifdef USE_DEBUGGERS
void _setupBp(struct mCore* core) {
	switch (core->platform(core)) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		core->busWrite32(core, 0x020000C0, 0xE0000000); // nop
		core->busWrite32(core, 0x020000C4, 0xE0000000); // nop
		core->busWrite32(core, 0x020000C8, 0xEAFFFFFD); // b 0x020000C4
		break;
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		core->rawWrite8(core, 0x101, 0, 0xEE); // Jump to 0xF0
		core->rawWrite8(core, 0xF0, 0, 0x00); // nop
		core->rawWrite8(core, 0xF1, 0, 0x18); // Loop forecer
		core->rawWrite8(core, 0xF2, 0, 0xFD); // jr $-3
		break;
#endif
	}
}

#ifdef M_CORE_GBA
M_TEST_DEFINE(basicBreakpointGBA) {
	SETUP_LUA;
	struct mCore* core = mCoreCreate(mPLATFORM_GBA);
	struct mDebugger debugger;
	assert_non_null(core);
	assert_true(core->init(core));
	mCoreInitConfig(core, NULL);
	core->reset(core);
	_setupBp(core);
	mScriptContextAttachCore(&context, core);

	mDebuggerInit(&debugger);
	mDebuggerAttach(&debugger, core);

	TEST_PROGRAM(
		"hit = 0\n"
		"function bkpt()\n"
		"	hit = hit + 1\n"
		"end"
	);
	TEST_PROGRAM("cbid = emu:setBreakpoint(bkpt, 0x020000C4)");
	TEST_PROGRAM("assert(cbid == 1)");

	int i;
	for (i = 0; i < 20; ++i) {
		mDebuggerRun(&debugger);
	}

	assert_int_equal(debugger.state, DEBUGGER_RUNNING);
	TEST_PROGRAM("assert(hit >= 1)");

	mScriptContextDeinit(&context);
	TEARDOWN_CORE;
	mDebuggerDeinit(&debugger);
}
#endif

#ifdef M_CORE_GB
M_TEST_DEFINE(basicBreakpointGB) {
	SETUP_LUA;
	struct mCore* core = mCoreCreate(mPLATFORM_GB);
	struct mDebugger debugger;
	assert_non_null(core);
	assert_true(core->init(core));
	mCoreInitConfig(core, NULL);
	assert_true(core->loadROM(core, VFileFromConstMemory(_fakeGBROM, sizeof(_fakeGBROM))));
	core->reset(core);
	_setupBp(core);
	mScriptContextAttachCore(&context, core);

	mDebuggerInit(&debugger);
	mDebuggerAttach(&debugger, core);

	TEST_PROGRAM(
		"hit = 0\n"
		"function bkpt()\n"
		"	hit = hit + 1\n"
		"end"
	);
	TEST_PROGRAM("cbid = emu:setBreakpoint(bkpt, 0xF0)");
	TEST_PROGRAM("assert(cbid == 1)");

	int i;
	for (i = 0; i < 20; ++i) {
		mDebuggerRun(&debugger);
	}

	assert_int_equal(debugger.state, DEBUGGER_RUNNING);
	TEST_PROGRAM("assert(hit >= 1)");

	mScriptContextDeinit(&context);
	TEARDOWN_CORE;
	mDebuggerDeinit(&debugger);
}
#endif

M_TEST_DEFINE(multipleBreakpoint) {
	SETUP_LUA;
	struct mCore* core = mCoreCreate(TEST_PLATFORM);
	struct mDebugger debugger;
	assert_non_null(core);
	assert_true(core->init(core));
	mCoreInitConfig(core, NULL);
	core->reset(core);
	_setupBp(core);
	mScriptContextAttachCore(&context, core);

	mDebuggerInit(&debugger);
	mDebuggerAttach(&debugger, core);

	TEST_PROGRAM(
		"hit = 0\n"
		"function bkpt1()\n"
		"	hit = hit + 1\n"
		"end\n"
		"function bkpt2()\n"
		"	hit = hit + 100\n"
		"end"
	);
#ifdef M_CORE_GBA
	TEST_PROGRAM("cbid1 = emu:setBreakpoint(bkpt1, 0x020000C4)");
	TEST_PROGRAM("cbid2 = emu:setBreakpoint(bkpt2, 0x020000C8)");
#else
	TEST_PROGRAM("cbid1 = emu:setBreakpoint(bkpt1, 0xF0)");
	TEST_PROGRAM("cbid2 = emu:setBreakpoint(bkpt2, 0xF1)");
#endif
	TEST_PROGRAM("assert(cbid1 == 1)");
	TEST_PROGRAM("assert(cbid2 == 2)");

	int i;
	for (i = 0; i < 20; ++i) {
		mDebuggerRun(&debugger);
	}

	assert_int_equal(debugger.state, DEBUGGER_RUNNING);
	TEST_PROGRAM("assert(hit >= 101)");

	mScriptContextDeinit(&context);
	TEARDOWN_CORE;
	mDebuggerDeinit(&debugger);
}

M_TEST_DEFINE(basicWatchpoint) {
	SETUP_LUA;
	mScriptContextAttachStdlib(&context);
	CREATE_CORE;
	struct mDebugger debugger;
	core->reset(core);
	mScriptContextAttachCore(&context, core);

	mDebuggerInit(&debugger);
	mDebuggerAttach(&debugger, core);

	TEST_PROGRAM(
		"hit = 0\n"
		"function bkpt()\n"
		"	hit = hit + 1\n"
		"end"
	);
	struct mScriptValue base = mSCRIPT_MAKE_S32(RAM_BASE);
	lua->setGlobal(lua, "base", &base);
	TEST_PROGRAM("assert(0 < emu:setWatchpoint(bkpt, base, C.WATCHPOINT_TYPE.READ))");
	TEST_PROGRAM("assert(0 < emu:setWatchpoint(bkpt, base + 1, C.WATCHPOINT_TYPE.WRITE))");
	TEST_PROGRAM("assert(0 < emu:setWatchpoint(bkpt, base + 2, C.WATCHPOINT_TYPE.RW))");
	TEST_PROGRAM("assert(0 < emu:setWatchpoint(bkpt, base + 3, C.WATCHPOINT_TYPE.WRITE_CHANGE))");
	TEST_PROGRAM("assert(hit == 0)");

	uint8_t value;

	// Read
	TEST_PROGRAM("hit = 0");
	value = core->rawRead8(core, RAM_BASE, -1);
	TEST_PROGRAM("assert(hit == 0)");
	core->busRead8(core, RAM_BASE);
	TEST_PROGRAM("assert(hit == 1)");
	core->busWrite8(core, RAM_BASE, value);
	TEST_PROGRAM("assert(hit == 1)");
	core->busWrite8(core, RAM_BASE, ~value);
	TEST_PROGRAM("assert(hit == 1)");

	// Write
	TEST_PROGRAM("hit = 0");
	value = core->rawRead8(core, RAM_BASE + 1, -1);
	TEST_PROGRAM("assert(hit == 0)");
	core->busRead8(core, RAM_BASE + 1);
	TEST_PROGRAM("assert(hit == 0)");
	core->busWrite8(core, RAM_BASE + 1, value);
	TEST_PROGRAM("assert(hit == 1)");
	core->busWrite8(core, RAM_BASE + 1, ~value);
	TEST_PROGRAM("assert(hit == 2)");

	// RW
	TEST_PROGRAM("hit = 0");
	value = core->rawRead8(core, RAM_BASE + 2, -1);
	TEST_PROGRAM("assert(hit == 0)");
	core->busRead8(core, RAM_BASE + 2);
	TEST_PROGRAM("assert(hit == 1)");
	core->busWrite8(core, RAM_BASE + 2, value);
	TEST_PROGRAM("assert(hit == 2)");
	core->busWrite8(core, RAM_BASE + 2, ~value);
	TEST_PROGRAM("assert(hit == 3)");

	// Change
	TEST_PROGRAM("hit = 0");
	value = core->rawRead8(core, RAM_BASE + 3, -1);
	TEST_PROGRAM("assert(hit == 0)");
	core->busRead8(core, RAM_BASE + 3);
	TEST_PROGRAM("assert(hit == 0)");
	core->busWrite8(core, RAM_BASE + 3, value);
	TEST_PROGRAM("assert(hit == 0)");
	core->busWrite8(core, RAM_BASE + 3, ~value);
	TEST_PROGRAM("assert(hit == 1)");

	mScriptContextDeinit(&context);
	TEARDOWN_CORE;
	mDebuggerDeinit(&debugger);
}

M_TEST_DEFINE(removeBreakpoint) {
	SETUP_LUA;
	mScriptContextAttachStdlib(&context);
	CREATE_CORE;
	struct mDebugger debugger;
	core->reset(core);
	mScriptContextAttachCore(&context, core);

	mDebuggerInit(&debugger);
	mDebuggerAttach(&debugger, core);

	TEST_PROGRAM(
		"hit = 0\n"
		"function bkpt()\n"
		"	hit = hit + 1\n"
		"end"
	);
	struct mScriptValue base = mSCRIPT_MAKE_S32(RAM_BASE);
	lua->setGlobal(lua, "base", &base);
	TEST_PROGRAM("cbid = emu:setWatchpoint(bkpt, base, C.WATCHPOINT_TYPE.READ)");

	TEST_PROGRAM("assert(hit == 0)");
	core->busRead8(core, RAM_BASE);
	TEST_PROGRAM("assert(hit == 1)");
	core->busRead8(core, RAM_BASE);
	TEST_PROGRAM("assert(hit == 2)");
	TEST_PROGRAM("assert(emu:clearBreakpoint(cbid))");
	core->busRead8(core, RAM_BASE);
	TEST_PROGRAM("assert(hit == 2)");

	mScriptContextDeinit(&context);
	TEARDOWN_CORE;
	mDebuggerDeinit(&debugger);
}


M_TEST_DEFINE(overlappingBreakpoint) {
	SETUP_LUA;
	struct mCore* core = mCoreCreate(TEST_PLATFORM);
	struct mDebugger debugger;
	assert_non_null(core);
	assert_true(core->init(core));
	mCoreInitConfig(core, NULL);
	core->reset(core);
	_setupBp(core);
	mScriptContextAttachCore(&context, core);

	mDebuggerInit(&debugger);
	mDebuggerAttach(&debugger, core);

	TEST_PROGRAM(
		"hit = 0\n"
		"function bkpt1()\n"
		"	hit = hit + 1\n"
		"end\n"
		"function bkpt2()\n"
		"	hit = hit + 100\n"
		"end"
	);
#ifdef M_CORE_GBA
	TEST_PROGRAM("cbid1 = emu:setBreakpoint(bkpt1, 0x020000C4)");
	TEST_PROGRAM("cbid2 = emu:setBreakpoint(bkpt2, 0x020000C4)");
#else
	TEST_PROGRAM("cbid1 = emu:setBreakpoint(bkpt1, 0xF0)");
	TEST_PROGRAM("cbid2 = emu:setBreakpoint(bkpt2, 0xF0)");
#endif
	TEST_PROGRAM("assert(cbid1 == 1)");
	TEST_PROGRAM("assert(cbid2 == 2)");

	int i;
	for (i = 0; i < 20; ++i) {
		mDebuggerRun(&debugger);
	}

	assert_int_equal(debugger.state, DEBUGGER_RUNNING);
	TEST_PROGRAM("assert(hit >= 101)");
	TEST_PROGRAM("oldHit = hit");

	TEST_PROGRAM("assert(emu:clearBreakpoint(cbid2))");

	for (i = 0; i < 10; ++i) {
		mDebuggerRun(&debugger);
	}
	TEST_PROGRAM("assert(hit - oldHit > 0)");
	TEST_PROGRAM("assert(hit - oldHit < 100)");

	mScriptContextDeinit(&context);
	TEARDOWN_CORE;
	mDebuggerDeinit(&debugger);
}

M_TEST_DEFINE(overlappingWatchpoint) {
	SETUP_LUA;
	mScriptContextAttachStdlib(&context);
	CREATE_CORE;
	struct mDebugger debugger;
	core->reset(core);
	mScriptContextAttachCore(&context, core);

	mDebuggerInit(&debugger);
	mDebuggerAttach(&debugger, core);

	TEST_PROGRAM(
		"hit = 0\n"
		"function bkpt()\n"
		"	hit = hit + 1\n"
		"end"
	);
	struct mScriptValue base = mSCRIPT_MAKE_S32(RAM_BASE);
	lua->setGlobal(lua, "base", &base);
	TEST_PROGRAM("assert(0 < emu:setWatchpoint(bkpt, base, C.WATCHPOINT_TYPE.READ))");
	TEST_PROGRAM("assert(0 < emu:setWatchpoint(bkpt, base, C.WATCHPOINT_TYPE.WRITE))");
	TEST_PROGRAM("assert(0 < emu:setWatchpoint(bkpt, base, C.WATCHPOINT_TYPE.RW))");
	TEST_PROGRAM("assert(0 < emu:setWatchpoint(bkpt, base, C.WATCHPOINT_TYPE.WRITE_CHANGE))");
	TEST_PROGRAM("assert(hit == 0)");

	uint8_t value;

	// Read
	TEST_PROGRAM("hit = 0");
	value = core->rawRead8(core, RAM_BASE, -1);
	TEST_PROGRAM("assert(hit == 0)");
	core->busRead8(core, RAM_BASE);
	TEST_PROGRAM("assert(hit == 2)"); // Read, RW
	core->busWrite8(core, RAM_BASE, value);
	TEST_PROGRAM("assert(hit == 4)"); // Write, RW
	core->busWrite8(core, RAM_BASE, ~value);
	TEST_PROGRAM("assert(hit == 7)"); // Write, RW, change

	mScriptContextDeinit(&context);
	TEARDOWN_CORE;
	mDebuggerDeinit(&debugger);
}

M_TEST_DEFINE(rangeWatchpoint) {
	SETUP_LUA;
	mScriptContextAttachStdlib(&context);
	CREATE_CORE;
	struct mDebugger debugger;
	core->reset(core);
	mScriptContextAttachCore(&context, core);

	mDebuggerInit(&debugger);
	mDebuggerAttach(&debugger, core);

	TEST_PROGRAM(
		"hit = 0\n"
		"function bkpt()\n"
		"	hit = hit + 1\n"
		"end"
	);
	struct mScriptValue base = mSCRIPT_MAKE_S32(RAM_BASE);
	lua->setGlobal(lua, "base", &base);
	TEST_PROGRAM("assert(0 < emu:setRangeWatchpoint(bkpt, base, base + 2, C.WATCHPOINT_TYPE.READ))");
	TEST_PROGRAM("assert(0 < emu:setRangeWatchpoint(bkpt, base + 1, base + 3, C.WATCHPOINT_TYPE.READ))");

	// Read
	TEST_PROGRAM("assert(hit == 0)");
	core->busRead8(core, RAM_BASE);
	TEST_PROGRAM("assert(hit == 1)");
	core->busRead8(core, RAM_BASE + 1);
	TEST_PROGRAM("assert(hit == 3)");
	core->busRead8(core, RAM_BASE + 2);
	TEST_PROGRAM("assert(hit == 4)");

	mScriptContextDeinit(&context);
	TEARDOWN_CORE;
	mDebuggerDeinit(&debugger);
}
#endif

M_TEST_SUITE_DEFINE_SETUP_TEARDOWN(mScriptCore,
	cmocka_unit_test(globals),
	cmocka_unit_test(infoFuncs),
	cmocka_unit_test(detach),
	cmocka_unit_test(runFrame),
	cmocka_unit_test(memoryRead),
	cmocka_unit_test(memoryWrite),
	cmocka_unit_test(logging),
	cmocka_unit_test(screenshot),
#ifdef USE_DEBUGGERS
#ifdef M_CORE_GBA
	cmocka_unit_test(basicBreakpointGBA),
#endif
#ifdef M_CORE_GB
	cmocka_unit_test(basicBreakpointGB),
#endif
	cmocka_unit_test(multipleBreakpoint),
	cmocka_unit_test(basicWatchpoint),
	cmocka_unit_test(removeBreakpoint),
	cmocka_unit_test(overlappingBreakpoint),
	cmocka_unit_test(overlappingWatchpoint),
	cmocka_unit_test(rangeWatchpoint),
#endif
)
