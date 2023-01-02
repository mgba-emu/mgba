/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/internal/script/lua.h>
#include <mgba/script/context.h>
#include <mgba/script/input.h>
#include <mgba/script/types.h>

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

M_TEST_SUITE_DEFINE_SETUP_TEARDOWN(mScriptInput,
	cmocka_unit_test(members),
	cmocka_unit_test(seq),
	cmocka_unit_test(fireKey),
)
