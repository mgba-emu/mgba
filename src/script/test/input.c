/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/internal/script/lua.h>
#include <mgba/script.h>

#include "script/test.h"

#define SETUP_LUA \
	struct mScriptContext context; \
	mScriptContextInit(&context); \
	struct mScriptEngineContext* lua = mScriptContextRegisterEngine(&context, mSCRIPT_ENGINE_LUA); \
	mScriptContextAttachStdlib(&context); \
	mScriptContextAttachInput(&context)

M_TEST_SUITE_SETUP(mScriptInput) {
	if (mSCRIPT_ENGINE_LUA->init) {
		mSCRIPT_ENGINE_LUA->init(mSCRIPT_ENGINE_LUA);
	}
	return 0;
}

M_TEST_SUITE_TEARDOWN(mScriptInput) {
	if (mSCRIPT_ENGINE_LUA->deinit) {
		mSCRIPT_ENGINE_LUA->deinit(mSCRIPT_ENGINE_LUA);
	}
	return 0;
}

M_TEST_DEFINE(members) {
	SETUP_LUA;

	TEST_PROGRAM("assert(input)");
	TEST_PROGRAM("assert(input.seq == 0)");
	TEST_PROGRAM("assert(input.isKeyActive)");

	mScriptContextDeinit(&context);
}
M_TEST_DEFINE(seq) {
	SETUP_LUA;

	TEST_PROGRAM("assert(input.seq == 0)");

	TEST_PROGRAM(
		"seq = nil\n"
		"function cb(ev)\n"
		"	seq = ev.seq\n"
		"end\n"
		"id = callbacks:add('key', cb)\n"
	);

	struct mScriptKeyEvent keyEvent = {
		.d = { .type = mSCRIPT_EV_TYPE_KEY },
		.state = mSCRIPT_INPUT_STATE_DOWN,
	};

	mScriptContextFireEvent(&context, &keyEvent.d);
	TEST_PROGRAM("assert(input.seq == 1)");
	TEST_PROGRAM("assert(seq == 0)");

	mScriptContextFireEvent(&context, &keyEvent.d);
	TEST_PROGRAM("assert(input.seq == 2)");
	TEST_PROGRAM("assert(seq == 1)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(fireKey) {
	SETUP_LUA;

	TEST_PROGRAM("assert(not input:isKeyActive('a'))");

	TEST_PROGRAM(
		"activeKey = false\n"
		"state = nil\n"
		"function cb(ev)\n"
		"	assert(ev.type == C.EV_TYPE.KEY)\n"
		"	activeKey = string.char(ev.key)\n"
		"	state = ev.state\n"
		"end\n"
		"id = callbacks:add('key', cb)\n"
		"assert(id)\n"
		"assert(not activeKey)\n"
	);

	struct mScriptKeyEvent keyEvent = {
		.d = { .type = mSCRIPT_EV_TYPE_KEY },
		.state = mSCRIPT_INPUT_STATE_DOWN,
		.key = 'a'
	};
	mScriptContextFireEvent(&context, &keyEvent.d);

	TEST_PROGRAM("assert(input:isKeyActive('a'))");
	TEST_PROGRAM("assert(activeKey == 'a')");
	TEST_PROGRAM("assert(state == C.INPUT_STATE.DOWN)");

	keyEvent.state = mSCRIPT_INPUT_STATE_UP;
	mScriptContextFireEvent(&context, &keyEvent.d);

	TEST_PROGRAM("assert(not input:isKeyActive('a'))");
	TEST_PROGRAM("assert(state == C.INPUT_STATE.UP)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(activeKeys) {
	SETUP_LUA;

	TEST_PROGRAM("assert(#input:activeKeys() == 0)");

	struct mScriptKeyEvent keyEvent = {
		.d = { .type = mSCRIPT_EV_TYPE_KEY },
		.state = mSCRIPT_INPUT_STATE_DOWN,
		.key = 'a'
	};
	mScriptContextFireEvent(&context, &keyEvent.d);
	TEST_PROGRAM("assert(#input:activeKeys() == 1)");
	TEST_PROGRAM("assert(input:activeKeys()[1] == string.byte('a'))");

	keyEvent.state = mSCRIPT_INPUT_STATE_UP;
	mScriptContextFireEvent(&context, &keyEvent.d);

	TEST_PROGRAM("assert(#input:activeKeys() == 0)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(clearKeys) {
	SETUP_LUA;

	TEST_PROGRAM("assert(not input:isKeyActive('a'))");
	TEST_PROGRAM("assert(not input:isKeyActive('b'))");
	TEST_PROGRAM("assert(#input:activeKeys() == 0)");

	struct mScriptKeyEvent keyEvent = {
		.d = { .type = mSCRIPT_EV_TYPE_KEY },
		.state = mSCRIPT_INPUT_STATE_DOWN,
		.key = 'a'
	};
	mScriptContextFireEvent(&context, &keyEvent.d);
	// This changes it to STATE_HELD, but increments the down counter
	mScriptContextFireEvent(&context, &keyEvent.d);
	keyEvent.state = mSCRIPT_INPUT_STATE_DOWN;
	keyEvent.key = 'b';
	mScriptContextFireEvent(&context, &keyEvent.d);

	TEST_PROGRAM("assert(input:isKeyActive('a'))");
	TEST_PROGRAM("assert(input:isKeyActive('b'))");
	TEST_PROGRAM("assert(#input:activeKeys() == 2)");

	TEST_PROGRAM(
		"up = {}\n"
		"function cb(ev)\n"
		"	assert(ev.type == C.EV_TYPE.KEY)\n"
		"	assert(ev.state == C.INPUT_STATE.UP)\n"
		"	table.insert(up, ev.key)\n"
		"end\n"
		"id = callbacks:add('key', cb)\n"
	);

	mScriptContextClearKeys(&context);

	TEST_PROGRAM("assert(not input:isKeyActive('a'))");
	TEST_PROGRAM("assert(not input:isKeyActive('b'))");
	TEST_PROGRAM("assert(#input:activeKeys() == 0)");
	TEST_PROGRAM("assert(#up == 2)");

	mScriptContextDeinit(&context);
}


M_TEST_DEFINE(gamepadExport) {
	SETUP_LUA;

	struct mScriptGamepad m_gamepad;
	mScriptGamepadInit(&m_gamepad);

	TEST_PROGRAM("assert(not input.activeGamepad)");
	assert_int_equal(mScriptContextGamepadAttach(&context, &m_gamepad), 0);
	TEST_PROGRAM("assert(input.activeGamepad)");

	TEST_PROGRAM("assert(#input.activeGamepad.axes == 0)");
	TEST_PROGRAM("assert(#input.activeGamepad.buttons == 0)");
	TEST_PROGRAM("assert(#input.activeGamepad.hats == 0)");

	mScriptGamepadSetAxisCount(&m_gamepad, 1);
	TEST_PROGRAM("assert(#input.activeGamepad.axes == 1)");
	TEST_PROGRAM("assert(input.activeGamepad.axes[1] == 0)");
	mScriptGamepadSetAxis(&m_gamepad, 0, 123);
	TEST_PROGRAM("assert(input.activeGamepad.axes[1] == 123)");

	mScriptGamepadSetButtonCount(&m_gamepad, 1);
	TEST_PROGRAM("assert(#input.activeGamepad.buttons == 1)");
	TEST_PROGRAM("assert(input.activeGamepad.buttons[1] == false)");
	mScriptGamepadSetButton(&m_gamepad, 0, true);
	TEST_PROGRAM("assert(input.activeGamepad.buttons[1] == true)");

	mScriptGamepadSetHatCount(&m_gamepad, 1);
	TEST_PROGRAM("assert(#input.activeGamepad.hats == 1)");
	TEST_PROGRAM("assert(input.activeGamepad.hats[1] == C.INPUT_DIR.NONE)");
	mScriptGamepadSetHat(&m_gamepad, 0, mSCRIPT_INPUT_DIR_NORTHWEST);
	TEST_PROGRAM("assert(input.activeGamepad.hats[1] == C.INPUT_DIR.NORTHWEST)");

	mScriptContextGamepadDetach(&context, 0);
	TEST_PROGRAM("assert(not input.activeGamepad)");

	mScriptGamepadDeinit(&m_gamepad);

	mScriptContextDeinit(&context);
}

M_TEST_SUITE_DEFINE_SETUP_TEARDOWN(mScriptInput,
	cmocka_unit_test(members),
	cmocka_unit_test(seq),
	cmocka_unit_test(fireKey),
	cmocka_unit_test(activeKeys),
	cmocka_unit_test(clearKeys),
	cmocka_unit_test(gamepadExport),
)
