/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/internal/script/lua.h>

M_TEST_SUITE_SETUP(mScriptLua) {
	if (mSCRIPT_ENGINE_LUA->init) {
		mSCRIPT_ENGINE_LUA->init(mSCRIPT_ENGINE_LUA);
	}
	return 0;
}

M_TEST_SUITE_TEARDOWN(mScriptLua) {
	if (mSCRIPT_ENGINE_LUA->deinit) {
		mSCRIPT_ENGINE_LUA->deinit(mSCRIPT_ENGINE_LUA);
	}
	return 0;
}

M_TEST_DEFINE(create) {
	struct mScriptContext context;
	mScriptContextInit(&context);
	struct mScriptEngineContext* lua = mSCRIPT_ENGINE_LUA->create(mSCRIPT_ENGINE_LUA, &context);
	lua->destroy(lua);
	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(loadGood) {
	struct mScriptContext context;
	mScriptContextInit(&context);
	struct mScriptEngineContext* lua = mSCRIPT_ENGINE_LUA->create(mSCRIPT_ENGINE_LUA, &context);

	const char* program = "-- test\n";
	struct VFile* vf = VFileFromConstMemory(program, strlen(program));
	const char* error = NULL;
	assert_true(lua->load(lua, vf, &error));
	assert_null(error);

	lua->destroy(lua);
	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(loadSyntax) {
	struct mScriptContext context;
	mScriptContextInit(&context);
	struct mScriptEngineContext* lua = mSCRIPT_ENGINE_LUA->create(mSCRIPT_ENGINE_LUA, &context);

	const char* program = "Invalid syntax! )\n";
	struct VFile* vf = VFileFromConstMemory(program, strlen(program));
	const char* error = NULL;
	assert_false(lua->load(lua, vf, &error));
	assert_non_null(error);

	lua->destroy(lua);
	mScriptContextDeinit(&context);
}

M_TEST_SUITE_DEFINE_SETUP_TEARDOWN(mScriptLua,
	cmocka_unit_test(create),
	cmocka_unit_test(loadGood),
	cmocka_unit_test(loadSyntax))
