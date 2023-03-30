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
	mScriptContextAttachStdlib(&context)

M_TEST_SUITE_SETUP(mScriptStdlib) {
	if (mSCRIPT_ENGINE_LUA->init) {
		mSCRIPT_ENGINE_LUA->init(mSCRIPT_ENGINE_LUA);
	}
	return 0;
}

M_TEST_SUITE_TEARDOWN(mScriptStdlib) {
	if (mSCRIPT_ENGINE_LUA->deinit) {
		mSCRIPT_ENGINE_LUA->deinit(mSCRIPT_ENGINE_LUA);
	}
	return 0;
}

M_TEST_DEFINE(bitMask) {
	SETUP_LUA;

	TEST_PROGRAM("assert(util)");
	TEST_PROGRAM("assert(util.makeBitmask)");
	TEST_PROGRAM("assert(util.makeBitmask{0} == 1)");
	TEST_PROGRAM("assert(util.makeBitmask{1} == 2)");
	TEST_PROGRAM("assert(util.makeBitmask{0, 1} == 3)");
	TEST_PROGRAM("assert(util.makeBitmask{1, 1} == 2)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(bitUnmask) {
	SETUP_LUA;

	TEST_PROGRAM("assert(util)");
	TEST_PROGRAM("assert(util.expandBitmask)");
	TEST_PROGRAM("assert(#util.expandBitmask(0) == 0)");
	TEST_PROGRAM("assert(#util.expandBitmask(1) == 1)");
	TEST_PROGRAM("assert(util.expandBitmask(1)[1] == 0)");
	TEST_PROGRAM("assert(#util.expandBitmask(2) == 1)");
	TEST_PROGRAM("assert(util.expandBitmask(2)[1] == 1)");
	TEST_PROGRAM("assert(#util.expandBitmask(3) == 2)");
	TEST_PROGRAM("assert(util.expandBitmask(3)[1] == 0 or util.expandBitmask(3)[1] == 1)");
	TEST_PROGRAM("assert(util.expandBitmask(3)[2] == 0 or util.expandBitmask(3)[2] == 1)");
	TEST_PROGRAM("assert(#util.expandBitmask(6) == 2)");
	TEST_PROGRAM("assert(util.expandBitmask(6)[1] == 1 or util.expandBitmask(6)[1] == 2)");
	TEST_PROGRAM("assert(util.expandBitmask(6)[2] == 1 or util.expandBitmask(6)[2] == 2)");
	TEST_PROGRAM("assert(#util.expandBitmask(7) == 3)");
	TEST_PROGRAM("assert(#util.expandBitmask(11) == 3)");
	TEST_PROGRAM("assert(#util.expandBitmask(15) == 4)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(callbacks) {
	SETUP_LUA;

	TEST_PROGRAM(
		"val = 0\n"
		"function cb()\n"
		"	val = val + 1\n"
		"end\n"
		"id = callbacks:add('test', cb)\n"
		"assert(id)"
	);

	TEST_VALUE(S32, "val", 0);

	mScriptContextTriggerCallback(&context, "test", NULL);
	TEST_VALUE(S32, "val", 1);

	mScriptContextTriggerCallback(&context, "test", NULL);
	TEST_VALUE(S32, "val", 2);

	TEST_PROGRAM("callbacks:remove(id)");

	mScriptContextTriggerCallback(&context, "test", NULL);
	TEST_VALUE(S32, "val", 2);

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(oneshot) {
	SETUP_LUA;

	TEST_PROGRAM(
		"val = 0\n"
		"function cb()\n"
		"	val = val + 1\n"
		"end\n"
		"id = callbacks:oneshot('test', cb)\n"
		"assert(id)"
	);

	TEST_VALUE(S32, "val", 0);

	mScriptContextTriggerCallback(&context, "test", NULL);
	TEST_VALUE(S32, "val", 1);

	mScriptContextTriggerCallback(&context, "test", NULL);
	TEST_VALUE(S32, "val", 1);

	TEST_PROGRAM(
		"id = callbacks:oneshot('test', cb)\n"
		"assert(id)\n"
		"callbacks:remove(id)"
	);

	mScriptContextTriggerCallback(&context, "test", NULL);
	TEST_VALUE(S32, "val", 1);

	mScriptContextDeinit(&context);
}

M_TEST_SUITE_DEFINE_SETUP_TEARDOWN(mScriptStdlib,
	cmocka_unit_test(bitMask),
	cmocka_unit_test(bitUnmask),
	cmocka_unit_test(callbacks),
	cmocka_unit_test(oneshot),
)
