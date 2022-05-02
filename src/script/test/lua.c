/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/internal/script/lua.h>

static int identityInt(int in) {
	return in;
}

static int addInts(int a, int b) {
	return a + b;
}

mSCRIPT_BIND_FUNCTION(boundIdentityInt, S32, identityInt, 1, S32);
mSCRIPT_BIND_FUNCTION(boundAddInts, S32, addInts, 2, S32, S32);

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
	vf->close(vf);
}

M_TEST_DEFINE(loadBadSyntax) {
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
	vf->close(vf);
}

M_TEST_DEFINE(runNop) {
	struct mScriptContext context;
	mScriptContextInit(&context);
	struct mScriptEngineContext* lua = mSCRIPT_ENGINE_LUA->create(mSCRIPT_ENGINE_LUA, &context);

	const char* program = "return";
	struct VFile* vf = VFileFromConstMemory(program, strlen(program));
	const char* error = NULL;
	assert_true(lua->load(lua, vf, &error));
	assert_null(error);
	assert_true(lua->run(lua));

	// Make sure we can run it twice
	assert_true(lua->run(lua));

	lua->destroy(lua);
	mScriptContextDeinit(&context);
	vf->close(vf);
}

M_TEST_DEFINE(getGlobal) {
	struct mScriptContext context;
	mScriptContextInit(&context);
	struct mScriptEngineContext* lua = mSCRIPT_ENGINE_LUA->create(mSCRIPT_ENGINE_LUA, &context);

	struct mScriptValue a = mSCRIPT_MAKE_S32(1);
	struct mScriptValue* val;
	const char* program;
	struct VFile* vf;
	const char* error;

	program = "a = 1";
	vf = VFileFromConstMemory(program, strlen(program));
	error = NULL;
	assert_true(lua->load(lua, vf, &error));
	assert_null(error);
	assert_true(lua->run(lua));

	val = lua->getGlobal(lua, "a");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	mScriptValueDeref(val);
	vf->close(vf);

	program = "b = 1";
	vf = VFileFromConstMemory(program, strlen(program));
	error = NULL;
	assert_true(lua->load(lua, vf, &error));
	assert_null(error);
	assert_true(lua->run(lua));

	val = lua->getGlobal(lua, "a");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	mScriptValueDeref(val);

	val = lua->getGlobal(lua, "b");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	mScriptValueDeref(val);
	vf->close(vf);

	a = mSCRIPT_MAKE_S32(2);
	program = "a = 2";
	vf = VFileFromConstMemory(program, strlen(program));
	error = NULL;
	assert_true(lua->load(lua, vf, &error));
	assert_null(error);
	assert_true(lua->run(lua));

	val = lua->getGlobal(lua, "a");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	mScriptValueDeref(val);
	vf->close(vf);

	a = mSCRIPT_MAKE_S32(3);
	program = "b = a + b";
	vf = VFileFromConstMemory(program, strlen(program));
	error = NULL;
	assert_true(lua->load(lua, vf, &error));
	assert_null(error);
	assert_true(lua->run(lua));

	val = lua->getGlobal(lua, "b");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	mScriptValueDeref(val);

	lua->destroy(lua);
	mScriptContextDeinit(&context);
	vf->close(vf);
}

M_TEST_DEFINE(setGlobal) {
	struct mScriptContext context;
	mScriptContextInit(&context);
	struct mScriptEngineContext* lua = mSCRIPT_ENGINE_LUA->create(mSCRIPT_ENGINE_LUA, &context);

	struct mScriptValue a = mSCRIPT_MAKE_S32(1);
	struct mScriptValue* val;
	const char* program;
	struct VFile* vf;
	const char* error;

	program = "a = b";
	vf = VFileFromConstMemory(program, strlen(program));
	error = NULL;
	assert_true(lua->load(lua, vf, &error));
	assert_null(error);
	assert_true(lua->setGlobal(lua, "b", &a));

	val = lua->getGlobal(lua, "b");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	mScriptValueDeref(val);

	assert_true(lua->run(lua));

	val = lua->getGlobal(lua, "a");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	a = mSCRIPT_MAKE_S32(2);
	assert_false(a.type->equal(&a, val));
	mScriptValueDeref(val);

	val = lua->getGlobal(lua, "a");
	assert_non_null(val);
	assert_false(a.type->equal(&a, val));
	mScriptValueDeref(val);

	assert_true(lua->setGlobal(lua, "b", &a));

	val = lua->getGlobal(lua, "b");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	mScriptValueDeref(val);

	assert_true(lua->run(lua));

	val = lua->getGlobal(lua, "a");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	mScriptValueDeref(val);

	lua->destroy(lua);
	mScriptContextDeinit(&context);
	vf->close(vf);
}

M_TEST_DEFINE(callLuaFunc) {
	struct mScriptContext context;
	mScriptContextInit(&context);
	struct mScriptEngineContext* lua = mSCRIPT_ENGINE_LUA->create(mSCRIPT_ENGINE_LUA, &context);

	struct mScriptValue* fn;
	const char* program;
	struct VFile* vf;
	const char* error;

	program = "function a(b) return b + 1 end; function c(d, e) return d + e end";
	vf = VFileFromConstMemory(program, strlen(program));
	error = NULL;
	assert_true(lua->load(lua, vf, &error));
	assert_null(error);
	assert_true(lua->run(lua));

	fn = lua->getGlobal(lua, "a");
	assert_non_null(fn);
	assert_int_equal(fn->type->base, mSCRIPT_TYPE_FUNCTION);

	struct mScriptFrame frame;
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
	assert_true(mScriptInvoke(fn, &frame));
	int64_t val;
	assert_true(mScriptPopS64(&frame.returnValues, &val));
	assert_int_equal(val, 2);

	mScriptFrameDeinit(&frame);
	mScriptValueDeref(fn);

	fn = lua->getGlobal(lua, "c");
	assert_non_null(fn);
	assert_int_equal(fn->type->base, mSCRIPT_TYPE_FUNCTION);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
	mSCRIPT_PUSH(&frame.arguments, S32, 2);
	assert_true(mScriptInvoke(fn, &frame));
	assert_true(mScriptPopS64(&frame.returnValues, &val));
	assert_int_equal(val, 3);

	mScriptFrameDeinit(&frame);
	mScriptValueDeref(fn);

	lua->destroy(lua);
	mScriptContextDeinit(&context);
	vf->close(vf);
}

M_TEST_DEFINE(callCFunc) {
	struct mScriptContext context;
	mScriptContextInit(&context);
	struct mScriptEngineContext* lua = mSCRIPT_ENGINE_LUA->create(mSCRIPT_ENGINE_LUA, &context);

	struct mScriptValue a = mSCRIPT_MAKE_S32(1);
	struct mScriptValue* val;
	const char* program;
	struct VFile* vf;
	const char* error;

	program = "a = b(1); c = d(1, 2)";
	vf = VFileFromConstMemory(program, strlen(program));
	error = NULL;
	assert_true(lua->load(lua, vf, &error));
	assert_null(error);

	assert_true(lua->setGlobal(lua, "b", &boundIdentityInt));
	assert_true(lua->setGlobal(lua, "d", &boundAddInts));
	assert_true(lua->run(lua));

	val = lua->getGlobal(lua, "a");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	mScriptValueDeref(val);

	a = mSCRIPT_MAKE_S32(3);
	val = lua->getGlobal(lua, "c");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	mScriptValueDeref(val);

	lua->destroy(lua);
	mScriptContextDeinit(&context);
	vf->close(vf);
}

M_TEST_SUITE_DEFINE_SETUP_TEARDOWN(mScriptLua,
	cmocka_unit_test(create),
	cmocka_unit_test(loadGood),
	cmocka_unit_test(loadBadSyntax),
	cmocka_unit_test(runNop),
	cmocka_unit_test(getGlobal),
	cmocka_unit_test(setGlobal),
	cmocka_unit_test(callLuaFunc),
	cmocka_unit_test(callCFunc))
