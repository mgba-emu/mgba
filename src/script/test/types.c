/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/script/types.h>

struct Test {
	int32_t a;
};

mSCRIPT_EXPORT_STRUCT(Test);

static int voidOne(void) {
	return 1;
}

static void discard(int ignored) {
	UNUSED(ignored);
}

static int identityInt(int in) {
	return in;
}

static float identityFloat(float in) {
	return in;
}

static struct Test* identityStruct(struct Test* t) {
	return t;
}

static int addInts(int a, int b) {
	return a + b;
}

static int subInts(int a, int b) {
	return a - b;
}

mSCRIPT_BIND_FUNCTION(boundVoidOne, S32, voidOne, 0);
mSCRIPT_BIND_VOID_FUNCTION(boundDiscard, discard, 1, S32);
mSCRIPT_BIND_FUNCTION(boundIdentityInt, S32, identityInt, 1, S32);
mSCRIPT_BIND_FUNCTION(boundIdentityFloat, F32, identityFloat, 1, F32);
mSCRIPT_BIND_FUNCTION(boundIdentityStruct, S(Test), identityStruct, 1, S(Test));
mSCRIPT_BIND_FUNCTION(boundAddInts, S32, addInts, 2, S32, S32);
mSCRIPT_BIND_FUNCTION(boundSubInts, S32, subInts, 2, S32, S32);

M_TEST_DEFINE(voidArgs) {
	struct mScriptFrame frame;
	mScriptFrameInit(&frame);
	assert_true(mScriptInvoke(&boundVoidOne, &frame));
	int32_t val;
	assert_true(mScriptPopS32(&frame.returnValues, &val));
	assert_int_equal(val, 1);
	mScriptFrameDeinit(&frame);
}

M_TEST_DEFINE(voidFunc) {
	struct mScriptFrame frame;
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
	assert_true(mScriptInvoke(&boundDiscard, &frame));
	mScriptFrameDeinit(&frame);
}

M_TEST_DEFINE(identityFunctionS32) {
	struct mScriptFrame frame;
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
	assert_true(mScriptInvoke(&boundIdentityInt, &frame));
	int32_t val;
	assert_true(mScriptPopS32(&frame.returnValues, &val));
	assert_int_equal(val, 1);
	mScriptFrameDeinit(&frame);
}

M_TEST_DEFINE(identityFunctionF32) {
	struct mScriptFrame frame;
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, F32, 3.125f);
	assert_true(mScriptInvoke(&boundIdentityFloat, &frame));
	float val;
	assert_true(mScriptPopF32(&frame.returnValues, &val));
	assert_float_equal(val, 3.125f, 0.f);
	mScriptFrameDeinit(&frame);
}

M_TEST_DEFINE(identityFunctionStruct) {
	struct mScriptFrame frame;
	struct Test v = {};
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(Test), &v);
	assert_true(mScriptInvoke(&boundIdentityStruct, &frame));
	struct Test* val;
	assert_true(mScriptPopPointer(&frame.returnValues, (void**) &val));
	assert_ptr_equal(val, &v);
	mScriptFrameDeinit(&frame);
}

M_TEST_DEFINE(addS32) {
	struct mScriptFrame frame;
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
	mSCRIPT_PUSH(&frame.arguments, S32, 2);
	assert_true(mScriptInvoke(&boundAddInts, &frame));
	int32_t val;
	assert_true(mScriptPopS32(&frame.returnValues, &val));
	assert_int_equal(val, 3);
	mScriptFrameDeinit(&frame);
}

M_TEST_DEFINE(subS32) {
	struct mScriptFrame frame;
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S32, 2);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
	assert_true(mScriptInvoke(&boundSubInts, &frame));
	int32_t val;
	assert_true(mScriptPopS32(&frame.returnValues, &val));
	assert_int_equal(val, 1);
	mScriptFrameDeinit(&frame);
}

M_TEST_DEFINE(wrongArgCountLo) {
	struct mScriptFrame frame;
	mScriptFrameInit(&frame);
	assert_false(mScriptInvoke(&boundIdentityInt, &frame));
	mScriptFrameDeinit(&frame);
}

M_TEST_DEFINE(wrongArgCountHi) {
	struct mScriptFrame frame;
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
	assert_false(mScriptInvoke(&boundIdentityInt, &frame));
	mScriptFrameDeinit(&frame);
}

M_TEST_DEFINE(wrongArgType) {
	struct mScriptFrame frame;
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
	assert_false(mScriptInvoke(&boundIdentityStruct, &frame));
	mScriptFrameDeinit(&frame);
}

M_TEST_DEFINE(coerceToFloat) {
	struct mScriptFrame frame;
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
	assert_true(mScriptInvoke(&boundIdentityFloat, &frame));
	float val;
	assert_true(mScriptPopF32(&frame.returnValues, &val));
	assert_float_equal(val, 1.f, 0.f);
	mScriptFrameDeinit(&frame);
}

M_TEST_DEFINE(coerceFromFloat) {
	struct mScriptFrame frame;
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, F32, 1.25f);
	assert_true(mScriptInvoke(&boundIdentityInt, &frame));
	int val;
	assert_true(mScriptPopS32(&frame.returnValues, &val));
	assert_int_equal(val, 1);
	mScriptFrameDeinit(&frame);
}

M_TEST_DEFINE(hashTableBasic) {
	struct mScriptValue* table = mScriptValueAlloc(mSCRIPT_TYPE_MS_TABLE);
	assert_non_null(table);

	struct mScriptValue* intValue = mScriptValueAlloc(mSCRIPT_TYPE_MS_S32);
	assert_ptr_equal(intValue->type, mSCRIPT_TYPE_MS_S32);
	assert_int_equal(intValue->value.s32, 0);
	assert_int_equal(intValue->refs, 1);

	struct mScriptValue intKey = {
		.type = mSCRIPT_TYPE_MS_S32,
		.value = {
			.s32 = 1234
		},
		.refs = mSCRIPT_VALUE_UNREF
	};

	struct mScriptValue badKey = {
		.type = mSCRIPT_TYPE_MS_S32,
		.value = {
			.s32 = 1235
		},
		.refs = mSCRIPT_VALUE_UNREF
	};

	assert_true(mScriptTableInsert(table, &intKey, intValue));
	assert_int_equal(intValue->refs, 2);

	struct mScriptValue* lookupValue = mScriptTableLookup(table, &intKey);
	assert_non_null(lookupValue);
	assert_ptr_equal(lookupValue, intValue);

	lookupValue = mScriptTableLookup(table, &badKey);
	assert_null(lookupValue);

	assert_true(mScriptTableRemove(table, &intKey));
	assert_int_equal(intValue->refs, 1);

	mScriptValueDeref(intValue);
	mScriptValueDeref(table);
}

M_TEST_SUITE_DEFINE(mScript,
	cmocka_unit_test(voidArgs),
	cmocka_unit_test(voidFunc),
	cmocka_unit_test(identityFunctionS32),
	cmocka_unit_test(identityFunctionF32),
	cmocka_unit_test(identityFunctionStruct),
	cmocka_unit_test(addS32),
	cmocka_unit_test(subS32),
	cmocka_unit_test(wrongArgCountLo),
	cmocka_unit_test(wrongArgCountHi),
	cmocka_unit_test(wrongArgType),
	cmocka_unit_test(coerceToFloat),
	cmocka_unit_test(coerceFromFloat),
	cmocka_unit_test(hashTableBasic))
