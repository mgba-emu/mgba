/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/internal/script/lua.h>
#include <mgba/script/storage.h>
#include <mgba/script/types.h>

#include "script/test.h"

#define SETUP_LUA \
	struct mScriptContext context; \
	mScriptContextInit(&context); \
	struct mScriptEngineContext* lua = mScriptContextRegisterEngine(&context, mSCRIPT_ENGINE_LUA); \
	mScriptContextAttachStdlib(&context); \
	mScriptContextAttachStorage(&context)

M_TEST_SUITE_SETUP(mScriptStorage) {
	if (mSCRIPT_ENGINE_LUA->init) {
		mSCRIPT_ENGINE_LUA->init(mSCRIPT_ENGINE_LUA);
	}
	return 0;
}

M_TEST_SUITE_TEARDOWN(mScriptStorage) {
	if (mSCRIPT_ENGINE_LUA->deinit) {
		mSCRIPT_ENGINE_LUA->deinit(mSCRIPT_ENGINE_LUA);
	}
	return 0;
}

M_TEST_DEFINE(basicInt) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");
	TEST_PROGRAM("assert(not bucket.a)");
	TEST_PROGRAM("bucket.a = 1");
	TEST_PROGRAM("assert(bucket.a == 1)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(basicFloat) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");
	TEST_PROGRAM("assert(not bucket.a)");
	TEST_PROGRAM("bucket.a = 0.5");
	TEST_PROGRAM("assert(bucket.a == 0.5)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(basicBool) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");
	TEST_PROGRAM("assert(not bucket.a)");
	TEST_PROGRAM("bucket.a = true");
	TEST_PROGRAM("assert(bucket.a == true)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(basicNil) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");
	TEST_PROGRAM("assert(not bucket.a)");
	TEST_PROGRAM("bucket.a = nil");
	TEST_PROGRAM("assert(bucket.a == nil)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(basicString) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");
	TEST_PROGRAM("assert(not bucket.a)");
	TEST_PROGRAM("bucket.a = 'hello'");
	TEST_PROGRAM("assert(bucket.a == 'hello')");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(basicList) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");
	TEST_PROGRAM("assert(not bucket.a)");
	TEST_PROGRAM("bucket.a = {1}");
	TEST_PROGRAM("assert(#bucket.a == 1)");
	TEST_PROGRAM("assert(bucket.a[1] == 1)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(basicTable) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");
	TEST_PROGRAM("assert(not bucket.a)");
	TEST_PROGRAM("bucket.a = {['a']=1}");
	TEST_PROGRAM("assert(#bucket.a == 1)");
	TEST_PROGRAM("assert(bucket.a.a == 1)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(nullByteString) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");
	TEST_PROGRAM("assert(not bucket.a)");
	TEST_PROGRAM("bucket.a = 'a\\x00b'");
	TEST_PROGRAM("assert(bucket.a == 'a\\x00b')");
	TEST_PROGRAM("assert(#bucket.a == 3)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(structured) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");
	TEST_PROGRAM("assert(not bucket.a)");
	TEST_PROGRAM(
		"bucket.a = {\n"
		"	['a'] = 1,\n"
		"	['b'] = {1},\n"
		"	['c'] = {\n"
		"		['d'] = 1\n"
		"	}\n"
		"}"
	);
	TEST_PROGRAM("assert(bucket.a)");
	TEST_PROGRAM("assert(bucket.a.a == 1)");
	TEST_PROGRAM("assert(#bucket.a.b == 1)");
	TEST_PROGRAM("assert(bucket.a.b[1] == 1)");
	TEST_PROGRAM("assert(#bucket.a.c == 1)");
	TEST_PROGRAM("assert(bucket.a.c.d == 1)");

	mScriptContextDeinit(&context);
}

M_TEST_SUITE_DEFINE_SETUP_TEARDOWN(mScriptStorage,
	cmocka_unit_test(basicInt),
	cmocka_unit_test(basicFloat),
	cmocka_unit_test(basicBool),
	cmocka_unit_test(basicNil),
	cmocka_unit_test(basicString),
	cmocka_unit_test(basicList),
	cmocka_unit_test(basicTable),
	cmocka_unit_test(nullByteString),
	cmocka_unit_test(structured),
)
