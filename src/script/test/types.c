/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/script.h>

struct Test {
	int32_t a;
};

mSCRIPT_DEFINE_STRUCT(Test)
mSCRIPT_DEFINE_END;

static int voidOne(void) {
	return 1;
}

static void discard(int ignored) {
	UNUSED(ignored);
}

static int identityInt(int in) {
	return in;
}

static int64_t identityInt64(int64_t in) {
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

static int isHello(const char* str) {
	return strcmp(str, "hello") == 0;
}

static int isSequential(struct mScriptList* list) {
	int last;
	if (mScriptListSize(list) == 0) {
		return true;
	}
	size_t i;
	for (i = 0; i < mScriptListSize(list); ++i) {
		struct mScriptValue* value = mScriptListGetPointer(list, i);
		struct mScriptValue intValue;
		if (!mScriptCast(mSCRIPT_TYPE_MS_S32, value, &intValue)) {
			return false;
		}
		if (!i) {
			last = intValue.value.s32;
		} else {
			if (intValue.value.s32 != last + 1) {
				return false;
			}
			++last;
		}
	}
	return true;
}

static bool isNullCharp(const char* arg) {
	return !arg;
}

static bool isNullStruct(struct Test* arg) {
	return !arg;
}

mSCRIPT_BIND_FUNCTION(boundVoidOne, S32, voidOne, 0);
mSCRIPT_BIND_VOID_FUNCTION(boundDiscard, discard, 1, S32, ignored);
mSCRIPT_BIND_FUNCTION(boundIdentityInt, S32, identityInt, 1, S32, in);
mSCRIPT_BIND_FUNCTION(boundIdentityInt64, S64, identityInt64, 1, S64, in);
mSCRIPT_BIND_FUNCTION(boundIdentityFloat, F32, identityFloat, 1, F32, in);
mSCRIPT_BIND_FUNCTION(boundIdentityStruct, S(Test), identityStruct, 1, S(Test), t);
mSCRIPT_BIND_FUNCTION(boundAddInts, S32, addInts, 2, S32, a, S32, b);
mSCRIPT_BIND_FUNCTION(boundSubInts, S32, subInts, 2, S32, a, S32, b);
mSCRIPT_BIND_FUNCTION(boundIsHello, S32, isHello, 1, CHARP, str);
mSCRIPT_BIND_FUNCTION(boundIsSequential, S32, isSequential, 1, LIST, list);
mSCRIPT_BIND_FUNCTION(boundIsNullCharp, BOOL, isNullCharp, 1, CHARP, arg);
mSCRIPT_BIND_FUNCTION(boundIsNullStruct, BOOL, isNullStruct, 1, S(Test), arg);
mSCRIPT_BIND_FUNCTION_WITH_DEFAULTS(boundAddIntWithDefaults, S32, addInts, 2, S32, a, S32, b);

mSCRIPT_DEFINE_FUNCTION_BINDING_DEFAULTS(boundAddIntWithDefaults)
	mSCRIPT_NO_DEFAULT,
	mSCRIPT_S32(0)
mSCRIPT_DEFINE_DEFAULTS_END;

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

M_TEST_DEFINE(identityFunctionS64) {
	struct mScriptFrame frame;
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S64, 1);
	assert_true(mScriptInvoke(&boundIdentityInt64, &frame));
	int64_t val;
	assert_true(mScriptPopS64(&frame.returnValues, &val));
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

M_TEST_DEFINE(addS32Defaults) {
	struct mScriptFrame frame;
	int32_t val;

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
	mSCRIPT_PUSH(&frame.arguments, S32, 2);
	assert_true(mScriptInvoke(&boundAddIntWithDefaults, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &val));
	assert_int_equal(val, 3);
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
	assert_true(mScriptInvoke(&boundAddIntWithDefaults, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &val));
	assert_int_equal(val, 1);
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	assert_false(mScriptInvoke(&boundAddIntWithDefaults, &frame));
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

M_TEST_DEFINE(wrongPopType) {
	struct mScriptFrame frame;
	int32_t s32;
	int64_t s64;
	uint32_t u32;
	uint64_t u64;
	float f32;
	double f64;
	bool b;

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S32, 0);
	assert_false(mScriptPopU32(&frame.arguments, &u32));
	assert_false(mScriptPopF32(&frame.arguments, &f32));
	assert_false(mScriptPopBool(&frame.arguments, &b));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S64, 0);
	assert_false(mScriptPopU64(&frame.arguments, &u64));
	assert_false(mScriptPopF64(&frame.arguments, &f64));
	assert_false(mScriptPopBool(&frame.arguments, &b));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, U32, 0);
	assert_false(mScriptPopS32(&frame.arguments, &s32));
	assert_false(mScriptPopF32(&frame.arguments, &f32));
	assert_false(mScriptPopBool(&frame.arguments, &b));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, U64, 0);
	assert_false(mScriptPopS64(&frame.arguments, &s64));
	assert_false(mScriptPopF64(&frame.arguments, &f64));
	assert_false(mScriptPopBool(&frame.arguments, &b));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, F32, 0);
	assert_false(mScriptPopS32(&frame.arguments, &s32));
	assert_false(mScriptPopU32(&frame.arguments, &u32));
	assert_false(mScriptPopBool(&frame.arguments, &b));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, F64, 0);
	assert_false(mScriptPopS64(&frame.arguments, &s64));
	assert_false(mScriptPopU64(&frame.arguments, &u64));
	assert_false(mScriptPopBool(&frame.arguments, &b));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, BOOL, 0);
	assert_false(mScriptPopS32(&frame.arguments, &s32));
	assert_false(mScriptPopU32(&frame.arguments, &u32));
	assert_false(mScriptPopS64(&frame.arguments, &s64));
	assert_false(mScriptPopU64(&frame.arguments, &u64));
	assert_false(mScriptPopF32(&frame.arguments, &f32));
	assert_false(mScriptPopF64(&frame.arguments, &f64));
	mScriptFrameDeinit(&frame);
}

M_TEST_DEFINE(wrongPopSize) {
	struct mScriptFrame frame;
	int32_t s32;
	int64_t s64;
	uint32_t u32;
	uint64_t u64;
	float f32;
	double f64;

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S32, 0);
	assert_false(mScriptPopS64(&frame.arguments, &s64));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S64, 0);
	assert_false(mScriptPopS32(&frame.arguments, &s32));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, U32, 0);
	assert_false(mScriptPopU64(&frame.arguments, &u64));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, U64, 0);
	assert_false(mScriptPopU32(&frame.arguments, &u32));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, F32, 0);
	assert_false(mScriptPopF64(&frame.arguments, &f64));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, F64, 0);
	assert_false(mScriptPopF32(&frame.arguments, &f32));
	mScriptFrameDeinit(&frame);
}

bool mScriptPopCSTest(struct mScriptList* list, const struct Test** out) {
	mSCRIPT_POP(list, CS(Test), val);
	*out = val;
	return true;
}

bool mScriptPopSTest(struct mScriptList* list, struct Test** out) {
	mSCRIPT_POP(list, S(Test), val);
	*out = val;
	return true;
}

M_TEST_DEFINE(wrongConst) {
	struct mScriptFrame frame;
	struct Test a;
	struct Test* b;
	const struct Test* cb;
	struct mScriptTypeTuple signature = {
		.count = 1,
		.variable = false
	};

	mScriptClassInit(mSCRIPT_TYPE_MS_S(Test)->details.cls);
	mScriptClassInit(mSCRIPT_TYPE_MS_CS(Test)->details.cls);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(Test), &a);
	signature.entries[0] = mSCRIPT_TYPE_MS_S(Test);
	assert_true(mScriptCoerceFrame(&signature, &frame.arguments));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, CS(Test), &a);
	signature.entries[0] = mSCRIPT_TYPE_MS_CS(Test);
	assert_true(mScriptCoerceFrame(&signature, &frame.arguments));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(Test), &a);
	signature.entries[0] = mSCRIPT_TYPE_MS_CS(Test);
	assert_true(mScriptCoerceFrame(&signature, &frame.arguments));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, CS(Test), &a);
	signature.entries[0] = mSCRIPT_TYPE_MS_S(Test);
	assert_false(mScriptCoerceFrame(&signature, &frame.arguments));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(Test), &a);
	assert_true(mScriptPopSTest(&frame.arguments, &b));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(Test), &a);
	assert_false(mScriptPopCSTest(&frame.arguments, &cb));
	signature.entries[0] = mSCRIPT_TYPE_MS_CS(Test);
	assert_true(mScriptCoerceFrame(&signature, &frame.arguments));
	assert_true(mScriptPopCSTest(&frame.arguments, &cb));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, CS(Test), &a);
	assert_false(mScriptPopSTest(&frame.arguments, &b));
	signature.entries[0] = mSCRIPT_TYPE_MS_S(Test);
	assert_false(mScriptCoerceFrame(&signature, &frame.arguments));
	assert_false(mScriptPopSTest(&frame.arguments, &b));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, CS(Test), &a);
	assert_true(mScriptPopCSTest(&frame.arguments, &cb));
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

M_TEST_DEFINE(coerceToBool) {
	struct mScriptValue a;
	struct mScriptValue b;

	a = mSCRIPT_MAKE_S32(0);
	assert_true(mScriptCast(mSCRIPT_TYPE_MS_BOOL, &a, &b));
	assert_true(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(false)));
	assert_false(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(true)));

	a = mSCRIPT_MAKE_S32(1);
	assert_true(mScriptCast(mSCRIPT_TYPE_MS_BOOL, &a, &b));
	assert_true(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(true)));
	assert_false(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(false)));

	a = mSCRIPT_MAKE_S32(-1);
	assert_true(mScriptCast(mSCRIPT_TYPE_MS_BOOL, &a, &b));
	assert_true(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(true)));
	assert_false(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(false)));

	a = mSCRIPT_MAKE_S32(INT_MAX);
	assert_true(mScriptCast(mSCRIPT_TYPE_MS_BOOL, &a, &b));
	assert_true(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(true)));
	assert_false(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(false)));

	a = mSCRIPT_MAKE_S32(INT_MIN);
	assert_true(mScriptCast(mSCRIPT_TYPE_MS_BOOL, &a, &b));
	assert_true(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(true)));
	assert_false(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(false)));

	a = mSCRIPT_MAKE_U32(0);
	assert_true(mScriptCast(mSCRIPT_TYPE_MS_BOOL, &a, &b));
	assert_true(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(false)));
	assert_false(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(true)));

	a = mSCRIPT_MAKE_U32(1);
	assert_true(mScriptCast(mSCRIPT_TYPE_MS_BOOL, &a, &b));
	assert_true(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(true)));
	assert_false(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(false)));

	a = mSCRIPT_MAKE_U32(UINT_MAX);
	assert_true(mScriptCast(mSCRIPT_TYPE_MS_BOOL, &a, &b));
	assert_true(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(true)));
	assert_false(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(false)));

	a = mSCRIPT_MAKE_F32(0);
	assert_true(mScriptCast(mSCRIPT_TYPE_MS_BOOL, &a, &b));
	assert_true(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(false)));
	assert_false(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(true)));

	a = mSCRIPT_MAKE_F32(1);
	assert_true(mScriptCast(mSCRIPT_TYPE_MS_BOOL, &a, &b));
	assert_true(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(true)));
	assert_false(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(false)));

	a = mSCRIPT_MAKE_F32(1e30f);
	assert_true(mScriptCast(mSCRIPT_TYPE_MS_BOOL, &a, &b));
	assert_true(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(true)));
	assert_false(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(false)));

	a = mSCRIPT_MAKE_F32(1e-30f);
	assert_true(mScriptCast(mSCRIPT_TYPE_MS_BOOL, &a, &b));
	assert_true(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(true)));
	assert_false(mSCRIPT_TYPE_MS_BOOL->equal(&b, &mSCRIPT_MAKE_BOOL(false)));
}

M_TEST_DEFINE(coerceFromBool) {
	struct mScriptValue a;
	struct mScriptValue b;

	a = mSCRIPT_MAKE_BOOL(false);
	assert_true(mScriptCast(mSCRIPT_TYPE_MS_S32, &a, &b));
	assert_true(mSCRIPT_TYPE_MS_S32->equal(&b, &mSCRIPT_MAKE_S32(0)));

	a = mSCRIPT_MAKE_BOOL(true);
	assert_true(mScriptCast(mSCRIPT_TYPE_MS_S32, &a, &b));
	assert_true(mSCRIPT_TYPE_MS_S32->equal(&b, &mSCRIPT_MAKE_S32(1)));

	a = mSCRIPT_MAKE_BOOL(true);
	assert_true(mScriptCast(mSCRIPT_TYPE_MS_S32, &a, &b));
	assert_false(mSCRIPT_TYPE_MS_S32->equal(&b, &mSCRIPT_MAKE_S32(-1)));

	a = mSCRIPT_MAKE_BOOL(false);
	assert_true(mScriptCast(mSCRIPT_TYPE_MS_U32, &a, &b));
	assert_true(mSCRIPT_TYPE_MS_U32->equal(&b, &mSCRIPT_MAKE_U32(0)));

	a = mSCRIPT_MAKE_BOOL(true);
	assert_true(mScriptCast(mSCRIPT_TYPE_MS_U32, &a, &b));
	assert_true(mSCRIPT_TYPE_MS_U32->equal(&b, &mSCRIPT_MAKE_U32(1)));

	a = mSCRIPT_MAKE_BOOL(true);
	assert_true(mScriptCast(mSCRIPT_TYPE_MS_U32, &a, &b));
	assert_false(mSCRIPT_TYPE_MS_U32->equal(&b, &mSCRIPT_MAKE_U32(2)));
}

M_TEST_DEFINE(coerceWiden) {
	struct mScriptFrame frame;
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S32, -1);
	assert_true(mScriptInvoke(&boundIdentityInt64, &frame));
	int64_t val;
	assert_true(mScriptPopS64(&frame.returnValues, &val));
	assert_true(val == -1LL);
	mScriptFrameDeinit(&frame);
}

M_TEST_DEFINE(coerceNarrow) {
	struct mScriptFrame frame;
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S64, -1);
	assert_true(mScriptInvoke(&boundIdentityInt, &frame));
	int32_t val;
	assert_true(mScriptPopS32(&frame.returnValues, &val));
	assert_true(val == -1);
	mScriptFrameDeinit(&frame);
}

#define COMPARE_BOOL(EXPECT, T0, V0, T1, V1) \
	a = mSCRIPT_MAKE_ ## T0 (V0); \
	b = mSCRIPT_MAKE_ ## T1 (V1); \
	assert_ ## EXPECT (a.type->equal(&a, &b));

M_TEST_DEFINE(s32Equality) {
	struct mScriptValue a;
	struct mScriptValue b;

	// S32
	COMPARE_BOOL(true,  S32,  0, S32,  0);
	COMPARE_BOOL(false, S32,  0, S32,  1);
	COMPARE_BOOL(true,  S32,  1, S32,  1);
	COMPARE_BOOL(false, S32,  1, S32, -1);
	COMPARE_BOOL(true,  S32, -1, S32, -1);

	// S64
	COMPARE_BOOL(true,  S32,  0, S64,  0);
	COMPARE_BOOL(false, S32,  0, S64,  1);
	COMPARE_BOOL(true,  S32,  1, S64,  1);
	COMPARE_BOOL(false, S32,  1, S64, -1);
	COMPARE_BOOL(true,  S32, -1, S64, -1);
	COMPARE_BOOL(false, S32,  0, S64,  0x100000000LL);
	COMPARE_BOOL(false, S32, -1, S64,  0x1FFFFFFFFLL);
	COMPARE_BOOL(false, S32, -1, S64, -0x100000001LL);

	// U32
	COMPARE_BOOL(true,  S32,           0, U32, 0);
	COMPARE_BOOL(false, S32,           0, U32, 1);
	COMPARE_BOOL(true,  S32,           1, U32, 1);
	COMPARE_BOOL(true,  S32,  0x7FFFFFFF, U32, 0x7FFFFFFFU);
	COMPARE_BOOL(false, S32,  0xFFFFFFFF, U32, 0xFFFFFFFFU);
	COMPARE_BOOL(false, S32,  0x80000000, U32, 0x80000000U);

	// U64
	COMPARE_BOOL(true,  S32,           0, U64, 0);
	COMPARE_BOOL(false, S32,           0, U64, 1);
	COMPARE_BOOL(true,  S32,           1, U64, 1);
	COMPARE_BOOL(true,  S32,  0x7FFFFFFF, U64, 0x7FFFFFFFULL);
	COMPARE_BOOL(false, S32,  0xFFFFFFFF, U64, 0xFFFFFFFFULL);
	COMPARE_BOOL(false, S32,  0x80000000, U64, 0x80000000ULL);

	// F32
	COMPARE_BOOL(true,  S32,           0, F32,  0);
	COMPARE_BOOL(false, S32,           1, F32,  0);
	COMPARE_BOOL(false, S32,           0, F32,  1);
	COMPARE_BOOL(true,  S32,           1, F32,  1);
	COMPARE_BOOL(false, S32,           0, F32, -1);
	COMPARE_BOOL(false, S32,           1, F32, -1);
	COMPARE_BOOL(true,  S32,          -1, F32, -1);
	COMPARE_BOOL(false, S32,           1, F32,  1.1);
	COMPARE_BOOL(false, S32,           0, F32,  0.1);
	COMPARE_BOOL(true,  S32,  0x40000000, F32,  0x40000000);
	COMPARE_BOOL(true,  S32, -0x40000000, F32, -0x40000000);

	// F64
	COMPARE_BOOL(true,  S32,           0, F64,  0);
	COMPARE_BOOL(false, S32,           1, F64,  0);
	COMPARE_BOOL(false, S32,           0, F64,  1);
	COMPARE_BOOL(true,  S32,           1, F64,  1);
	COMPARE_BOOL(false, S32,           0, F64, -1);
	COMPARE_BOOL(false, S32,           1, F64, -1);
	COMPARE_BOOL(true,  S32,          -1, F64, -1);
	COMPARE_BOOL(false, S32,           1, F64,  1.1);
	COMPARE_BOOL(false, S32,           0, F64,  0.1);
	COMPARE_BOOL(true,  S32,  0x40000000, F64,  0x40000000);
	COMPARE_BOOL(true,  S32, -0x40000000, F64, -0x40000000);

	// BOOL
	COMPARE_BOOL(true,  S32,           0, BOOL, false);
	COMPARE_BOOL(false, S32,           0, BOOL, true);
	COMPARE_BOOL(false, S32,           1, BOOL, false);
	COMPARE_BOOL(true,  S32,           1, BOOL, true);
	COMPARE_BOOL(false, S32,          -1, BOOL, false);
	COMPARE_BOOL(true,  S32,          -1, BOOL, true);
	COMPARE_BOOL(false, S32,           2, BOOL, false);
	COMPARE_BOOL(true,  S32,           2, BOOL, true);
	COMPARE_BOOL(false, S32,  0x7FFFFFFF, BOOL, false);
	COMPARE_BOOL(true,  S32,  0x7FFFFFFF, BOOL, true);
	COMPARE_BOOL(false, S32, -0x80000000, BOOL, false);
	COMPARE_BOOL(true,  S32, -0x80000000, BOOL, true);
}

M_TEST_DEFINE(s64Equality) {
	struct mScriptValue a;
	struct mScriptValue b;

	// S32
	COMPARE_BOOL(true,  S64,              0, S32,  0);
	COMPARE_BOOL(false, S64,              0, S32,  1);
	COMPARE_BOOL(true,  S64,              1, S32,  1);
	COMPARE_BOOL(false, S64,              1, S32, -1);
	COMPARE_BOOL(true,  S64,             -1, S32, -1);
	COMPARE_BOOL(false, S64,  0x100000000LL, S32,  0);
	COMPARE_BOOL(false, S64,  0x1FFFFFFFFLL, S32, -1);
	COMPARE_BOOL(false, S64, -0x100000001LL, S32, -1);

	// S64
	COMPARE_BOOL(true,  S64,              0, S64,  0);
	COMPARE_BOOL(false, S64,              0, S64,  1);
	COMPARE_BOOL(true,  S64,              1, S64,  1);
	COMPARE_BOOL(false, S64,              1, S64, -1);
	COMPARE_BOOL(true,  S64,             -1, S64, -1);
	COMPARE_BOOL(false, S64,              0, S64,  0x100000000LL);
	COMPARE_BOOL(false, S64,             -1, S64,  0x1FFFFFFFFLL);
	COMPARE_BOOL(false, S64,             -1, S64, -0x100000001LL);
	COMPARE_BOOL(false, S64,  0x100000000LL, S64,  0);
	COMPARE_BOOL(false, S64,  0x1FFFFFFFFLL, S64, -1);
	COMPARE_BOOL(false, S64, -0x100000001LL, S64, -1);
	COMPARE_BOOL(true,  S64,  0x100000000LL, S64,  0x100000000LL);
	COMPARE_BOOL(true,  S64,  0x1FFFFFFFFLL, S64,  0x1FFFFFFFFLL);
	COMPARE_BOOL(true,  S64, -0x100000001LL, S64, -0x100000001LL);

	// U32
	COMPARE_BOOL(true,  S64,             0, U32, 0);
	COMPARE_BOOL(false, S64,             0, U32, 1);
	COMPARE_BOOL(true,  S64,             1, U32, 1);
	COMPARE_BOOL(true,  S64,  0x7FFFFFFFLL, U32, 0x7FFFFFFFU);
	COMPARE_BOOL(true,  S64,  0xFFFFFFFFLL, U32, 0xFFFFFFFFU);
	COMPARE_BOOL(true,  S64,  0x80000000LL, U32, 0x80000000U);
	COMPARE_BOOL(false, S64,            -1, U32, 0xFFFFFFFFU);
	COMPARE_BOOL(false, S64, -0x80000000LL, U32, 0x80000000U);

	// U64
	COMPARE_BOOL(true,  S64,              0, U64, 0);
	COMPARE_BOOL(false, S64,              0, U64, 1);
	COMPARE_BOOL(true,  S64,              1, U64, 1);
	COMPARE_BOOL(true,  S64,  0x07FFFFFFFLL, U64, 0x07FFFFFFFULL);
	COMPARE_BOOL(true,  S64,  0x0FFFFFFFFLL, U64, 0x0FFFFFFFFULL);
	COMPARE_BOOL(true,  S64,  0x080000000LL, U64, 0x080000000ULL);
	COMPARE_BOOL(false, S64,              0, U64, 0x100000000ULL);
	COMPARE_BOOL(false, S64,  0x100000000LL, U64, 0);
	COMPARE_BOOL(true,  S64,  0x100000000LL, U64, 0x100000000ULL);
	COMPARE_BOOL(false, S64,             -1, U64, 0x0FFFFFFFFULL);
	COMPARE_BOOL(false, S64,             -1, U64, 0xFFFFFFFFFFFFFFFFULL);
	COMPARE_BOOL(false, S64, -0x080000000LL, U64, 0x080000000ULL);

	// F32
	COMPARE_BOOL(true,  S64,                     0, F32,  0);
	COMPARE_BOOL(false, S64,                     1, F32,  0);
	COMPARE_BOOL(false, S64,                     0, F32,  1);
	COMPARE_BOOL(true,  S64,                     1, F32,  1);
	COMPARE_BOOL(false, S64,                     0, F32, -1);
	COMPARE_BOOL(false, S64,                     1, F32, -1);
	COMPARE_BOOL(true,  S64,                    -1, F32, -1);
	COMPARE_BOOL(false, S64,                     1, F32,  1.1);
	COMPARE_BOOL(false, S64,                     0, F32,  0.1);
	COMPARE_BOOL(true,  S64,  0x4000000000000000LL, F32,  0x4000000000000000LL);
	COMPARE_BOOL(true,  S64, -0x4000000000000000LL, F32, -0x4000000000000000LL);

	// F64
	COMPARE_BOOL(true,  S64,                     0, F64,  0);
	COMPARE_BOOL(false, S64,                     1, F64,  0);
	COMPARE_BOOL(false, S64,                     0, F64,  1);
	COMPARE_BOOL(true,  S64,                     1, F64,  1);
	COMPARE_BOOL(false, S64,                     0, F64, -1);
	COMPARE_BOOL(false, S64,                     1, F64, -1);
	COMPARE_BOOL(true,  S64,                    -1, F64, -1);
	COMPARE_BOOL(false, S64,                     1, F64,  1.1);
	COMPARE_BOOL(false, S64,                     0, F64,  0.1);
	COMPARE_BOOL(true,  S64,  0x4000000000000000LL, F64,  0x4000000000000000LL);
	COMPARE_BOOL(true,  S64, -0x4000000000000000LL, F64, -0x4000000000000000LL);

	// BOOL
	COMPARE_BOOL(true,  S64,                     0, BOOL, false);
	COMPARE_BOOL(false, S64,                     0, BOOL, true);
	COMPARE_BOOL(false, S64,                     1, BOOL, false);
	COMPARE_BOOL(true,  S64,                     1, BOOL, true);
	COMPARE_BOOL(false, S64,                    -1, BOOL, false);
	COMPARE_BOOL(true,  S64,                    -1, BOOL, true);
	COMPARE_BOOL(false, S64,                     2, BOOL, false);
	COMPARE_BOOL(true,  S64,                     2, BOOL, true);
	COMPARE_BOOL(false, S64,  0x7FFFFFFFFFFFFFFFLL, BOOL, false);
	COMPARE_BOOL(true,  S64,  0x7FFFFFFFFFFFFFFFLL, BOOL, true);
	COMPARE_BOOL(false, S64, -0x8000000000000000LL, BOOL, false);
	COMPARE_BOOL(true,  S64, -0x8000000000000000LL, BOOL, true);
}

M_TEST_DEFINE(u32Equality) {
	struct mScriptValue a;
	struct mScriptValue b;

	// U32
	COMPARE_BOOL(true,  U32,           0, U32, 0);
	COMPARE_BOOL(false, U32,           0, U32, 1);
	COMPARE_BOOL(true,  U32,           1, U32, 1);
	COMPARE_BOOL(false, U32, 0x80000000U, U32, 1);
	COMPARE_BOOL(true,  U32, 0x80000000U, U32, 0x80000000U);
	COMPARE_BOOL(false, U32, 0x7FFFFFFFU, U32, 1);
	COMPARE_BOOL(true,  U32, 0x7FFFFFFFU, U32, 0x7FFFFFFFU);

	// U64
	COMPARE_BOOL(true,  U32,           0, U64, 0);
	COMPARE_BOOL(false, U32,           0, U64, 1);
	COMPARE_BOOL(true,  U32,           1, U64, 1);
	COMPARE_BOOL(false, U32, 0x80000000U, U64, 1);
	COMPARE_BOOL(true,  U32, 0x80000000U, U64, 0x080000000ULL);
	COMPARE_BOOL(false, U32, 0x7FFFFFFFU, U64, 1);
	COMPARE_BOOL(true,  U32, 0x7FFFFFFFU, U64, 0x07FFFFFFFULL);
	COMPARE_BOOL(false, U32, 0x80000000U, U64, 0x180000000ULL);
	COMPARE_BOOL(false, U32,           0, U64, 0x100000000ULL);

	// S32
	COMPARE_BOOL(true,  U32,           0, S32, 0);
	COMPARE_BOOL(false, U32,           0, S32, 1);
	COMPARE_BOOL(true,  U32,           1, S32, 1);
	COMPARE_BOOL(true,  U32, 0x7FFFFFFFU, S32, 0x7FFFFFFF);
	COMPARE_BOOL(false, U32, 0xFFFFFFFFU, S32, 0xFFFFFFFF);
	COMPARE_BOOL(false, U32, 0x80000000U, S32, 0x80000000);

	// S64
	COMPARE_BOOL(true,  U32,           0, S64, 0);
	COMPARE_BOOL(false, U32,           0, S64, 1);
	COMPARE_BOOL(true,  U32,           1, S64, 1);
	COMPARE_BOOL(true,  U32, 0x7FFFFFFFU, S64, 0x07FFFFFFFLL);
	COMPARE_BOOL(true,  U32, 0xFFFFFFFFU, S64, 0x0FFFFFFFFLL);
	COMPARE_BOOL(true,  U32, 0x80000000U, S64, 0x080000000LL);
	COMPARE_BOOL(false, U32, 0x80000000U, S64, 0x180000000LL);
	COMPARE_BOOL(false, U32,           0, S64, 0x100000000LL);

	// F32
	COMPARE_BOOL(true,  U32,           0, F32,  0);
	COMPARE_BOOL(false, U32,           1, F32,  0);
	COMPARE_BOOL(false, U32,           0, F32,  1);
	COMPARE_BOOL(true,  U32,           1, F32,  1);
	COMPARE_BOOL(false, U32,           0, F32, -1);
	COMPARE_BOOL(false, U32,           1, F32, -1);
	COMPARE_BOOL(false, U32, 0xFFFFFFFFU, F32, -1);
	COMPARE_BOOL(true,  U32, 0x80000000U, F32,  0x80000000);
	COMPARE_BOOL(false, U32,           0, F32,  0x80000000);
	COMPARE_BOOL(false, U32, 0x80000000U, F32,  0);
	COMPARE_BOOL(false, U32,           1, F32,  1.1);
	COMPARE_BOOL(false, U32,           0, F32,  0.1);

	// F64
	COMPARE_BOOL(true,  U32,           0, F64,  0);
	COMPARE_BOOL(false, U32,           1, F64,  0);
	COMPARE_BOOL(false, U32,           0, F64,  1);
	COMPARE_BOOL(true,  U32,           1, F64,  1);
	COMPARE_BOOL(false, U32,           0, F64, -1);
	COMPARE_BOOL(false, U32,           1, F64, -1);
	COMPARE_BOOL(false, U32, 0xFFFFFFFFU, F64, -1);
	COMPARE_BOOL(true,  U32, 0x80000000U, F64,  0x80000000);
	COMPARE_BOOL(false, U32,           0, F64,  0x80000000);
	COMPARE_BOOL(false, U32, 0x80000000U, F64,  0);
	COMPARE_BOOL(false, U32,           1, F64,  1.1);
	COMPARE_BOOL(false, U32,           0, F64,  0.1);

	// BOOL
	COMPARE_BOOL(true,  U32,           0, BOOL, false);
	COMPARE_BOOL(false, U32,           0, BOOL, true);
	COMPARE_BOOL(false, U32,           1, BOOL, false);
	COMPARE_BOOL(true,  U32,           1, BOOL, true);
	COMPARE_BOOL(false, U32,           2, BOOL, false);
	COMPARE_BOOL(true,  U32,           2, BOOL, true);
	COMPARE_BOOL(false, U32, 0xFFFFFFFFU, BOOL, false);
	COMPARE_BOOL(true,  U32, 0xFFFFFFFFU, BOOL, true);
	COMPARE_BOOL(false, U32, 0x80000000U, BOOL, false);
	COMPARE_BOOL(true,  U32, 0x80000000U, BOOL, true);
}

M_TEST_DEFINE(u64Equality) {
	struct mScriptValue a;
	struct mScriptValue b;

	// U32
	COMPARE_BOOL(true,  U64,              0, U32, 0);
	COMPARE_BOOL(false, U64,              0, U32, 1);
	COMPARE_BOOL(true,  U64,              1, U32, 1);
	COMPARE_BOOL(false, U64, 0x080000000ULL, U32, 1);
	COMPARE_BOOL(true,  U64, 0x080000000ULL, U32, 0x80000000U);
	COMPARE_BOOL(false, U64, 0x07FFFFFFFULL, U32, 1);
	COMPARE_BOOL(true,  U64, 0x07FFFFFFFULL, U32, 0x7FFFFFFFU);
	COMPARE_BOOL(false, U64, 0x180000000ULL, U32, 0x80000000U);
	COMPARE_BOOL(false, U64, 0x100000000ULL, U32, 0);

	// U64
	COMPARE_BOOL(true,  U64,              0, U64, 0);
	COMPARE_BOOL(false, U64,              0, U64, 1);
	COMPARE_BOOL(true,  U64,              1, U64, 1);
	COMPARE_BOOL(false, U64, 0x080000000ULL, U64, 1);
	COMPARE_BOOL(true,  U64, 0x080000000ULL, U64, 0x080000000ULL);
	COMPARE_BOOL(false, U64, 0x07FFFFFFFULL, U64, 1);
	COMPARE_BOOL(true,  U64, 0x07FFFFFFFULL, U64, 0x07FFFFFFFULL);
	COMPARE_BOOL(true,  U64, 0x180000000ULL, U64, 0x180000000ULL);
	COMPARE_BOOL(true,  U64, 0x100000000ULL, U64, 0x100000000ULL);

	// S32
	COMPARE_BOOL(true,  U64,              0, S32, 0);
	COMPARE_BOOL(false, U64,              0, S32, 1);
	COMPARE_BOOL(true,  U64,              1, S32, 1);
	COMPARE_BOOL(true,  U64, 0x07FFFFFFFULL, S32, 0x7FFFFFFF);
	COMPARE_BOOL(false, U64, 0x0FFFFFFFFULL, S32, 0xFFFFFFFF);
	COMPARE_BOOL(false, U64, 0x080000000ULL, S32, 0x80000000);
	COMPARE_BOOL(false, U64, 0x100000000ULL, S32, 0);

	// S64
	COMPARE_BOOL(true,  U64,                     0, S64, 0);
	COMPARE_BOOL(false, U64,                     0, S64, 1);
	COMPARE_BOOL(true,  U64,                     1, S64, 1);
	COMPARE_BOOL(true,  U64,        0x07FFFFFFFULL, S64, 0x07FFFFFFFLL);
	COMPARE_BOOL(true,  U64,        0x0FFFFFFFFULL, S64, 0x0FFFFFFFFLL);
	COMPARE_BOOL(true,  U64,        0x080000000ULL, S64, 0x080000000LL);
	COMPARE_BOOL(false, U64,                     0, S64, 0x100000000LL);
	COMPARE_BOOL(false, U64,        0x100000000ULL, S64, 0);
	COMPARE_BOOL(true,  U64,        0x100000000ULL, S64, 0x100000000LL);
	COMPARE_BOOL(false, U64,        0x0FFFFFFFFULL, S64, -1);
	COMPARE_BOOL(false, U64, 0xFFFFFFFFFFFFFFFFULL, S64, -1);
	COMPARE_BOOL(true,  U64,        0x080000000ULL, S64, 0x080000000LL);

	// F32
	COMPARE_BOOL(true,  U64,                     0, F32,  0);
	COMPARE_BOOL(false, U64,                     1, F32,  0);
	COMPARE_BOOL(false, U64,                     0, F32,  1);
	COMPARE_BOOL(true,  U64,                     1, F32,  1);
	COMPARE_BOOL(false, U64,                     0, F32, -1);
	COMPARE_BOOL(false, U64,                     1, F32, -1);
	COMPARE_BOOL(false, U64, 0xFFFFFFFFFFFFFFFFULL, F32, -1);
	COMPARE_BOOL(true,  U64, 0x8000000000000000ULL, F32,  0x8000000000000000ULL);
	COMPARE_BOOL(false, U64,                     0, F32,  0x8000000000000000ULL);
	COMPARE_BOOL(false, U64, 0x8000000000000000ULL, F32,  0);
	COMPARE_BOOL(false, U64,                     1, F32,  1.1);
	COMPARE_BOOL(false, U64,                     0, F32,  0.1);

	// F64
	COMPARE_BOOL(true,  U64,                     0, F64,  0);
	COMPARE_BOOL(false, U64,                     1, F64,  0);
	COMPARE_BOOL(false, U64,                     0, F64,  1);
	COMPARE_BOOL(true,  U64,                     1, F64,  1);
	COMPARE_BOOL(false, U64,                     0, F64, -1);
	COMPARE_BOOL(false, U64,                     1, F64, -1);
	COMPARE_BOOL(false, U64, 0xFFFFFFFFFFFFFFFFULL, F64, -1);
	COMPARE_BOOL(true,  U64, 0x8000000000000000ULL, F64,  0x8000000000000000ULL);
	COMPARE_BOOL(false, U64,                     0, F64,  0x8000000000000000ULL);
	COMPARE_BOOL(false, U64, 0x8000000000000000ULL, F64,  0);
	COMPARE_BOOL(false, U64,                     1, F64,  1.1);
	COMPARE_BOOL(false, U64,                     0, F64,  0.1);

	// BOOL
	COMPARE_BOOL(true,  U64,                     0, BOOL, false);
	COMPARE_BOOL(false, U64,                     0, BOOL, true);
	COMPARE_BOOL(false, U64,                     1, BOOL, false);
	COMPARE_BOOL(true,  U64,                     1, BOOL, true);
	COMPARE_BOOL(false, U64,                     2, BOOL, false);
	COMPARE_BOOL(true,  U64,                     2, BOOL, true);
	COMPARE_BOOL(false, U64, 0xFFFFFFFFFFFFFFFFULL, BOOL, false);
	COMPARE_BOOL(true,  U64, 0xFFFFFFFFFFFFFFFFULL, BOOL, true);
	COMPARE_BOOL(false, U64, 0x8000000000000000ULL, BOOL, false);
	COMPARE_BOOL(true,  U64, 0x8000000000000000ULL, BOOL, true);
}

M_TEST_DEFINE(f32Equality) {
	struct mScriptValue a;
	struct mScriptValue b;

	// F32
	COMPARE_BOOL(true,  F32,   0, F32,  0);
	COMPARE_BOOL(false, F32,   0, F32,  1);
	COMPARE_BOOL(true,  F32,   1, F32,  1);
	COMPARE_BOOL(true,  F32,  -1, F32, -1);
	COMPARE_BOOL(false, F32, 1.1, F32,  1);
	COMPARE_BOOL(false, F32,   1, F32,  1.1);
	COMPARE_BOOL(true,  F32, 1.1, F32,  1.1);

	// F64
	COMPARE_BOOL(true,  F32,    0, F64,  0);
	COMPARE_BOOL(false, F32,    0, F64,  1);
	COMPARE_BOOL(true,  F32,    1, F64,  1);
	COMPARE_BOOL(true,  F32,   -1, F64, -1);
	COMPARE_BOOL(false, F32,  1.1, F64,  1);
	COMPARE_BOOL(false, F32,    1, F64,  1.1);
	COMPARE_BOOL(true,  F32, 1.25, F64,  1.25);

	// S32
	COMPARE_BOOL(true,  F32,           0, S32,  0);
	COMPARE_BOOL(false, F32,           0, S32,  1);
	COMPARE_BOOL(false, F32,           1, S32,  0);
	COMPARE_BOOL(true,  F32,           1, S32,  1);
	COMPARE_BOOL(false, F32,         1.1, S32,  1);
	COMPARE_BOOL(true,  F32,          -1, S32, -1);
	COMPARE_BOOL(false, F32,        -1.1, S32, -1);
	COMPARE_BOOL(true,  F32,  0x40000000, S32,  0x40000000);
	COMPARE_BOOL(true,  F32, -0x40000000, S32, -0x40000000);

	// S64
	COMPARE_BOOL(true,  F32,              0, S64,  0);
	COMPARE_BOOL(false, F32,              0, S64,  1);
	COMPARE_BOOL(false, F32,              1, S64,  0);
	COMPARE_BOOL(true,  F32,              1, S64,  1);
	COMPARE_BOOL(false, F32,            1.1, S64,  1);
	COMPARE_BOOL(true,  F32,             -1, S64, -1);
	COMPARE_BOOL(false, F32,           -1.1, S64, -1);
	COMPARE_BOOL(true,  F32,  0x040000000LL, S64,  0x040000000LL);
	COMPARE_BOOL(true,  F32,  0x100000000LL, S64,  0x100000000LL);
	COMPARE_BOOL(false, F32,  0x100000000LL, S64,  0);
	COMPARE_BOOL(false, F32,              0, S64,  0x100000000LL);
	COMPARE_BOOL(true,  F32, -0x040000000LL, S64, -0x040000000LL);

	// U32
	COMPARE_BOOL(true,  F32,          0, U32, 0);
	COMPARE_BOOL(false, F32,          0, U32, 1);
	COMPARE_BOOL(false, F32,          1, U32, 0);
	COMPARE_BOOL(true,  F32,          1, U32, 1);
	COMPARE_BOOL(false, F32,        1.1, U32, 1);
	COMPARE_BOOL(true,  F32, 0x40000000, U32, 0x40000000);

	// U64
	COMPARE_BOOL(true,  F32,              0, U64, 0);
	COMPARE_BOOL(false, F32,              0, U64, 1);
	COMPARE_BOOL(false, F32,              1, U64, 0);
	COMPARE_BOOL(true,  F32,              1, U64, 1);
	COMPARE_BOOL(false, F32,            1.1, U64, 1);
	COMPARE_BOOL(true,  F32, 0x040000000ULL, U64, 0x040000000ULL);
	COMPARE_BOOL(true,  F32, 0x100000000ULL, U64, 0x100000000ULL);
	COMPARE_BOOL(false, F32, 0x100000000ULL, U64, 0);
	COMPARE_BOOL(false, F32,              0, U64, 0x100000000ULL);

	// BOOL
	COMPARE_BOOL(true,  F32,              0, BOOL, false);
	COMPARE_BOOL(false, F32,              0, BOOL, true);
	COMPARE_BOOL(false, F32,              1, BOOL, false);
	COMPARE_BOOL(true,  F32,              1, BOOL, true);
	COMPARE_BOOL(false, F32,            1.1, BOOL, false);
	COMPARE_BOOL(true,  F32,            1.1, BOOL, true);
	COMPARE_BOOL(false, F32, 0x040000000ULL, BOOL, false);
	COMPARE_BOOL(true,  F32, 0x040000000ULL, BOOL, true);
	COMPARE_BOOL(false, F32, 0x100000000ULL, BOOL, false);
	COMPARE_BOOL(true,  F32, 0x100000000ULL, BOOL, true);
}

M_TEST_DEFINE(f64Equality) {
	struct mScriptValue a;
	struct mScriptValue b;

	// F32
	COMPARE_BOOL(true,  F64,    0, F32,  0);
	COMPARE_BOOL(false, F64,    0, F32,  1);
	COMPARE_BOOL(true,  F64,    1, F32,  1);
	COMPARE_BOOL(true,  F64,   -1, F32, -1);
	COMPARE_BOOL(false, F64,  1.1, F32,  1);
	COMPARE_BOOL(false, F64,    1, F32,  1.1);
	COMPARE_BOOL(true,  F64, 1.25, F32,  1.25);

	// F64
	COMPARE_BOOL(true,  F64,   0, F64,  0);
	COMPARE_BOOL(false, F64,   0, F64,  1);
	COMPARE_BOOL(true,  F64,   1, F64,  1);
	COMPARE_BOOL(true,  F64,  -1, F64, -1);
	COMPARE_BOOL(false, F64, 1.1, F64,  1);
	COMPARE_BOOL(false, F64,   1, F64,  1.1);
	COMPARE_BOOL(true,  F64, 1.1, F64,  1.1);

	// S32
	COMPARE_BOOL(true,  F64,           0, S32,  0);
	COMPARE_BOOL(false, F64,           0, S32,  1);
	COMPARE_BOOL(false, F64,           1, S32,  0);
	COMPARE_BOOL(true,  F64,           1, S32,  1);
	COMPARE_BOOL(false, F64,         1.1, S32,  1);
	COMPARE_BOOL(true,  F64,          -1, S32, -1);
	COMPARE_BOOL(false, F64,        -1.1, S32, -1);
	COMPARE_BOOL(true,  F64,  0x40000000, S32,  0x40000000);
	COMPARE_BOOL(true,  F64, -0x40000000, S32, -0x40000000);

	// S64
	COMPARE_BOOL(true,  F64,              0, S64,  0);
	COMPARE_BOOL(false, F64,              0, S64,  1);
	COMPARE_BOOL(false, F64,              1, S64,  0);
	COMPARE_BOOL(true,  F64,              1, S64,  1);
	COMPARE_BOOL(false, F64,            1.1, S64,  1);
	COMPARE_BOOL(true,  F64,             -1, S64, -1);
	COMPARE_BOOL(false, F64,           -1.1, S64, -1);
	COMPARE_BOOL(true,  F64,  0x040000000LL, S64,  0x040000000LL);
	COMPARE_BOOL(true,  F64,  0x100000000LL, S64,  0x100000000LL);
	COMPARE_BOOL(false, F64,  0x100000000LL, S64,  0);
	COMPARE_BOOL(false, F64,              0, S64,  0x100000000LL);
	COMPARE_BOOL(true,  F64, -0x040000000LL, S64, -0x040000000LL);

	// U32
	COMPARE_BOOL(true,  F64,          0, U32, 0);
	COMPARE_BOOL(false, F64,          0, U32, 1);
	COMPARE_BOOL(false, F64,          1, U32, 0);
	COMPARE_BOOL(true,  F64,          1, U32, 1);
	COMPARE_BOOL(false, F64,        1.1, U32, 1);
	COMPARE_BOOL(true,  F64, 0x40000000, U32, 0x40000000);

	// U64
	COMPARE_BOOL(true,  F64,              0, U64, 0);
	COMPARE_BOOL(false, F64,              0, U64, 1);
	COMPARE_BOOL(false, F64,              1, U64, 0);
	COMPARE_BOOL(true,  F64,              1, U64, 1);
	COMPARE_BOOL(false, F64,            1.1, U64, 1);
	COMPARE_BOOL(true,  F64, 0x040000000ULL, U64, 0x040000000ULL);
	COMPARE_BOOL(true,  F64, 0x100000000ULL, U64, 0x100000000ULL);
	COMPARE_BOOL(false, F64, 0x100000000ULL, U64, 0);
	COMPARE_BOOL(false, F64,              0, U64, 0x100000000ULL);

	// BOOL
	COMPARE_BOOL(true,  F64,              0, BOOL, false);
	COMPARE_BOOL(false, F64,              0, BOOL, true);
	COMPARE_BOOL(false, F64,              1, BOOL, false);
	COMPARE_BOOL(true,  F64,              1, BOOL, true);
	COMPARE_BOOL(false, F64,            1.1, BOOL, false);
	COMPARE_BOOL(true,  F64,            1.1, BOOL, true);
	COMPARE_BOOL(false, F64, 0x040000000ULL, BOOL, false);
	COMPARE_BOOL(true,  F64, 0x040000000ULL, BOOL, true);
	COMPARE_BOOL(false, F64, 0x100000000ULL, BOOL, false);
	COMPARE_BOOL(true,  F64, 0x100000000ULL, BOOL, true);
}

M_TEST_DEFINE(boolEquality) {
	struct mScriptValue a;
	struct mScriptValue b;

	// S32
	COMPARE_BOOL(true,  BOOL, false, S32,  0);
	COMPARE_BOOL(false, BOOL, false, S32,  1);
	COMPARE_BOOL(false, BOOL, false, S32, -1);
	COMPARE_BOOL(false, BOOL, false, S32,  2);
	COMPARE_BOOL(false, BOOL, false, S32,  0x7FFFFFFF);
	COMPARE_BOOL(false, BOOL, false, S32, -0x80000000);
	COMPARE_BOOL(false, BOOL,  true, S32,  0);
	COMPARE_BOOL(true,  BOOL,  true, S32,  1);
	COMPARE_BOOL(true,  BOOL,  true, S32, -1);
	COMPARE_BOOL(true,  BOOL,  true, S32,  2);
	COMPARE_BOOL(true,  BOOL,  true, S32,  0x7FFFFFFF);
	COMPARE_BOOL(true,  BOOL,  true, S32, -0x80000000);

	// S64
	COMPARE_BOOL(true,  BOOL, false, S64,  0);
	COMPARE_BOOL(false, BOOL, false, S64,  1);
	COMPARE_BOOL(false, BOOL, false, S64, -1);
	COMPARE_BOOL(false, BOOL, false, S64,  2);
	COMPARE_BOOL(false, BOOL, false, S64,  INT64_MIN);
	COMPARE_BOOL(false, BOOL, false, S64,  INT64_MAX);
	COMPARE_BOOL(false, BOOL,  true, S64,  0);
	COMPARE_BOOL(true,  BOOL,  true, S64,  1);
	COMPARE_BOOL(true,  BOOL,  true, S64, -1);
	COMPARE_BOOL(true,  BOOL,  true, S64,  2);
	COMPARE_BOOL(true,  BOOL,  true, S64,  INT64_MIN);
	COMPARE_BOOL(true,  BOOL,  true, S64,  INT64_MAX);

	// U32
	COMPARE_BOOL(true,  BOOL, false, U32,  0);
	COMPARE_BOOL(false, BOOL, false, U32,  1);
	COMPARE_BOOL(false, BOOL, false, U32,  2);
	COMPARE_BOOL(false, BOOL, false, U32,  UINT32_MAX);
	COMPARE_BOOL(false, BOOL,  true, U32,  0);
	COMPARE_BOOL(true,  BOOL,  true, U32,  1);
	COMPARE_BOOL(true,  BOOL,  true, U32,  2);
	COMPARE_BOOL(true,  BOOL,  true, U32,  UINT32_MAX);

	// U64
	COMPARE_BOOL(true,  BOOL, false, U64,  0);
	COMPARE_BOOL(false, BOOL, false, U64,  1);
	COMPARE_BOOL(false, BOOL, false, U64,  2);
	COMPARE_BOOL(false, BOOL, false, U64,  INT64_MAX);
	COMPARE_BOOL(false, BOOL,  true, U64,  0);
	COMPARE_BOOL(true,  BOOL,  true, U64,  1);
	COMPARE_BOOL(true,  BOOL,  true, U64,  2);
	COMPARE_BOOL(true,  BOOL,  true, U64,  INT64_MAX);

	// F32
	COMPARE_BOOL(true,  BOOL, false, F32,  0);
	COMPARE_BOOL(false, BOOL,  true, F32,  0);
	COMPARE_BOOL(false, BOOL, false, F32,  1);
	COMPARE_BOOL(true,  BOOL,  true, F32,  1);
	COMPARE_BOOL(false, BOOL, false, F32,  1.1f);
	COMPARE_BOOL(true,  BOOL,  true, F32,  1.1f);
	COMPARE_BOOL(false, BOOL, false, F32,  1e30f);
	COMPARE_BOOL(true,  BOOL,  true, F32,  1e30f);
	COMPARE_BOOL(false, BOOL, false, F32, -1);
	COMPARE_BOOL(true,  BOOL,  true, F32, -1);
	COMPARE_BOOL(false, BOOL, false, F32, -1.1f);
	COMPARE_BOOL(true,  BOOL,  true, F32, -1.1f);
	COMPARE_BOOL(false, BOOL, false, F32, -0.1e-30f);
	COMPARE_BOOL(true,  BOOL,  true, F32, -0.1e-30f);
	COMPARE_BOOL(false, BOOL, false, F32, -1e30f);
	COMPARE_BOOL(true,  BOOL,  true, F32, -1e30f);

	// F64
	COMPARE_BOOL(true,  BOOL, false, F64,  0);
	COMPARE_BOOL(false, BOOL,  true, F64,  0);
	COMPARE_BOOL(false, BOOL, false, F64,  1);
	COMPARE_BOOL(true,  BOOL,  true, F64,  1);
	COMPARE_BOOL(false, BOOL, false, F64,  1.1);
	COMPARE_BOOL(true,  BOOL,  true, F64,  1.1);
	COMPARE_BOOL(false, BOOL, false, F64,  1e30);
	COMPARE_BOOL(true,  BOOL,  true, F64,  1e30);
	COMPARE_BOOL(false, BOOL, false, F64, -1);
	COMPARE_BOOL(true,  BOOL,  true, F64, -1);
	COMPARE_BOOL(false, BOOL, false, F64, -1.1);
	COMPARE_BOOL(true,  BOOL,  true, F64, -1.1);
	COMPARE_BOOL(false, BOOL, false, F64, -0.1e-300);
	COMPARE_BOOL(true,  BOOL,  true, F64, -0.1e-300);
	COMPARE_BOOL(false, BOOL, false, F64, -1e300);
	COMPARE_BOOL(true,  BOOL,  true, F64, -1e300);
}

M_TEST_DEFINE(stringEquality) {
	struct mScriptValue* stringA = mScriptStringCreateFromUTF8("hello");
	struct mScriptValue* stringB = mScriptStringCreateFromUTF8("world");
	struct mScriptValue* stringC = mScriptStringCreateFromUTF8("hello");
	struct mScriptValue charpA = mSCRIPT_MAKE_CHARP("hello");
	struct mScriptValue charpB = mSCRIPT_MAKE_CHARP("world");

	assert_true(stringA->type->equal(stringA, stringC));
	assert_false(stringA->type->equal(stringA, stringB));

	assert_true(stringA->type->equal(stringA, &charpA));
	assert_false(stringA->type->equal(stringA, &charpB));

	assert_true(charpA.type->equal(&charpA, stringA));
	assert_false(charpA.type->equal(&charpA, stringB));

	charpB = mSCRIPT_MAKE_CHARP("hello");
	assert_true(charpA.type->equal(&charpA, &charpB));

	charpB = mSCRIPT_MAKE_CHARP("world");
	assert_false(charpA.type->equal(&charpA, &charpB));

	mScriptValueDeref(stringA);
	mScriptValueDeref(stringB);
	mScriptValueDeref(stringC);
}

M_TEST_DEFINE(hashTableBasic) {
	struct mScriptValue* table = mScriptValueAlloc(mSCRIPT_TYPE_MS_TABLE);
	assert_non_null(table);

	struct mScriptValue* intValue = mScriptValueAlloc(mSCRIPT_TYPE_MS_S32);
	assert_ptr_equal(intValue->type, mSCRIPT_TYPE_MS_S32);
	assert_int_equal(intValue->value.s32, 0);
	assert_int_equal(intValue->refs, 1);

	struct mScriptValue intKey = mSCRIPT_MAKE_S32(1234);
	struct mScriptValue badKey = mSCRIPT_MAKE_S32(1235);

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

M_TEST_DEFINE(hashTableString) {
	struct mScriptValue* table = mScriptValueAlloc(mSCRIPT_TYPE_MS_TABLE);
	assert_non_null(table);

	struct mScriptValue* intValue = mScriptValueAlloc(mSCRIPT_TYPE_MS_S32);
	assert_ptr_equal(intValue->type, mSCRIPT_TYPE_MS_S32);
	assert_int_equal(intValue->value.s32, 0);
	assert_int_equal(intValue->refs, 1);

	struct mScriptValue key = mSCRIPT_MAKE_CHARP("key");
	struct mScriptValue badKey = mSCRIPT_MAKE_CHARP("bad");

	assert_true(mScriptTableInsert(table, &key, intValue));
	assert_int_equal(intValue->refs, 2);

	struct mScriptValue* lookupValue = mScriptTableLookup(table, &key);
	assert_non_null(lookupValue);
	assert_ptr_equal(lookupValue, intValue);

	lookupValue = mScriptTableLookup(table, &badKey);
	assert_null(lookupValue);

	assert_true(mScriptTableRemove(table, &key));
	assert_int_equal(intValue->refs, 1);

	mScriptValueDeref(intValue);
	mScriptValueDeref(table);
}

M_TEST_DEFINE(stringIsHello) {
	struct mScriptFrame frame;
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, CHARP, "hello");
	assert_true(mScriptInvoke(&boundIsHello, &frame));
	int val;
	assert_true(mScriptPopS32(&frame.returnValues, &val));
	assert_int_equal(val, 1);
	mScriptFrameDeinit(&frame);
}

M_TEST_DEFINE(stringIsNotHello) {
	struct mScriptFrame frame;
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, CHARP, "world");
	assert_true(mScriptInvoke(&boundIsHello, &frame));
	int val;
	assert_true(mScriptPopS32(&frame.returnValues, &val));
	assert_int_equal(val, 0);
	mScriptFrameDeinit(&frame);
}

M_TEST_DEFINE(invokeList) {
	struct mScriptFrame frame;
	struct mScriptList list;
	int val;

	mScriptListInit(&list, 0);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, LIST, &list);
	assert_true(mScriptInvoke(&boundIsSequential, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &val));
	assert_int_equal(val, 1);
	mScriptFrameDeinit(&frame);

	*mScriptListAppend(&list) = mSCRIPT_MAKE_S32(1);
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, LIST, &list);
	assert_true(mScriptInvoke(&boundIsSequential, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &val));
	assert_int_equal(val, 1);
	mScriptFrameDeinit(&frame);

	*mScriptListAppend(&list) = mSCRIPT_MAKE_S32(2);
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, LIST, &list);
	assert_true(mScriptInvoke(&boundIsSequential, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &val));
	assert_int_equal(val, 1);
	mScriptFrameDeinit(&frame);

	*mScriptListAppend(&list) = mSCRIPT_MAKE_S32(4);
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, LIST, &list);
	assert_true(mScriptInvoke(&boundIsSequential, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &val));
	assert_int_equal(val, 0);
	mScriptFrameDeinit(&frame);

	mScriptListDeinit(&list);
}

M_TEST_DEFINE(nullString) {
	struct mScriptFrame frame;
	bool res;
	mScriptFrameInit(&frame);

	mSCRIPT_PUSH(&frame.arguments, CHARP, "hi");
	assert_true(mScriptInvoke(&boundIsNullCharp, &frame));
	assert_true(mScriptPopBool(&frame.returnValues, &res));
	assert_false(res);

	mSCRIPT_PUSH(&frame.arguments, CHARP, NULL);
	assert_true(mScriptInvoke(&boundIsNullCharp, &frame));
	assert_true(mScriptPopBool(&frame.returnValues, &res));
	assert_true(res);

	mScriptFrameDeinit(&frame);
}

M_TEST_DEFINE(nullStruct) {
	struct mScriptFrame frame;
	struct Test v = {};
	bool res;
	mScriptFrameInit(&frame);

	mSCRIPT_PUSH(&frame.arguments, S(Test), &v);
	assert_true(mScriptInvoke(&boundIsNullStruct, &frame));
	assert_true(mScriptPopBool(&frame.returnValues, &res));
	assert_false(res);

	mSCRIPT_PUSH(&frame.arguments, S(Test), NULL);
	assert_true(mScriptInvoke(&boundIsNullStruct, &frame));
	assert_true(mScriptPopBool(&frame.returnValues, &res));
	assert_true(res);

	mScriptFrameDeinit(&frame);
}

M_TEST_SUITE_DEFINE(mScript,
	cmocka_unit_test(voidArgs),
	cmocka_unit_test(voidFunc),
	cmocka_unit_test(identityFunctionS32),
	cmocka_unit_test(identityFunctionS64),
	cmocka_unit_test(identityFunctionF32),
	cmocka_unit_test(identityFunctionStruct),
	cmocka_unit_test(addS32),
	cmocka_unit_test(addS32Defaults),
	cmocka_unit_test(subS32),
	cmocka_unit_test(wrongArgCountLo),
	cmocka_unit_test(wrongArgCountHi),
	cmocka_unit_test(wrongArgType),
	cmocka_unit_test(wrongPopType),
	cmocka_unit_test(wrongPopSize),
	cmocka_unit_test(wrongConst),
	cmocka_unit_test(coerceToFloat),
	cmocka_unit_test(coerceFromFloat),
	cmocka_unit_test(coerceToBool),
	cmocka_unit_test(coerceFromBool),
	cmocka_unit_test(coerceNarrow),
	cmocka_unit_test(coerceWiden),
	cmocka_unit_test(s32Equality),
	cmocka_unit_test(s64Equality),
	cmocka_unit_test(u32Equality),
	cmocka_unit_test(u64Equality),
	cmocka_unit_test(f32Equality),
	cmocka_unit_test(f64Equality),
	cmocka_unit_test(boolEquality),
	cmocka_unit_test(stringEquality),
	cmocka_unit_test(hashTableBasic),
	cmocka_unit_test(hashTableString),
	cmocka_unit_test(stringIsHello),
	cmocka_unit_test(stringIsNotHello),
	cmocka_unit_test(invokeList),
	cmocka_unit_test(nullString),
	cmocka_unit_test(nullStruct),
)
