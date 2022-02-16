/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/script/context.h>
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

mSCRIPT_BIND_FUNCTION(boundVoidOne, S32, voidOne, 0);
mSCRIPT_BIND_VOID_FUNCTION(boundDiscard, discard, 1, S32);
mSCRIPT_BIND_FUNCTION(boundIdentityInt, S32, identityInt, 1, S32);
mSCRIPT_BIND_FUNCTION(boundIdentityInt64, S64, identityInt64, 1, S64);
mSCRIPT_BIND_FUNCTION(boundIdentityFloat, F32, identityFloat, 1, F32);
mSCRIPT_BIND_FUNCTION(boundIdentityStruct, S(Test), identityStruct, 1, S(Test));
mSCRIPT_BIND_FUNCTION(boundAddInts, S32, addInts, 2, S32, S32);
mSCRIPT_BIND_FUNCTION(boundSubInts, S32, subInts, 2, S32, S32);
mSCRIPT_BIND_FUNCTION(boundIsHello, S32, isHello, 1, CHARP);

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

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S32, 0);
	assert_false(mScriptPopU32(&frame.arguments, &u32));
	assert_false(mScriptPopF32(&frame.arguments, &f32));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S64, 0);
	assert_false(mScriptPopU64(&frame.arguments, &u64));
	assert_false(mScriptPopF64(&frame.arguments, &f64));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, U32, 0);
	assert_false(mScriptPopS32(&frame.arguments, &s32));
	assert_false(mScriptPopF32(&frame.arguments, &f32));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, U64, 0);
	assert_false(mScriptPopS64(&frame.arguments, &s64));
	assert_false(mScriptPopF64(&frame.arguments, &f64));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, F32, 0);
	assert_false(mScriptPopS32(&frame.arguments, &s32));
	assert_false(mScriptPopU32(&frame.arguments, &u32));
	mScriptFrameDeinit(&frame);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, F64, 0);
	assert_false(mScriptPopS64(&frame.arguments, &s64));
	assert_false(mScriptPopU64(&frame.arguments, &u64));
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

M_TEST_SUITE_DEFINE(mScript,
	cmocka_unit_test(voidArgs),
	cmocka_unit_test(voidFunc),
	cmocka_unit_test(identityFunctionS32),
	cmocka_unit_test(identityFunctionS64),
	cmocka_unit_test(identityFunctionF32),
	cmocka_unit_test(identityFunctionStruct),
	cmocka_unit_test(addS32),
	cmocka_unit_test(subS32),
	cmocka_unit_test(wrongArgCountLo),
	cmocka_unit_test(wrongArgCountHi),
	cmocka_unit_test(wrongArgType),
	cmocka_unit_test(wrongPopType),
	cmocka_unit_test(wrongPopSize),
	cmocka_unit_test(coerceToFloat),
	cmocka_unit_test(coerceFromFloat),
	cmocka_unit_test(coerceNarrow),
	cmocka_unit_test(coerceWiden),
	cmocka_unit_test(s32Equality),
	cmocka_unit_test(s64Equality),
	cmocka_unit_test(u32Equality),
	cmocka_unit_test(u64Equality),
	cmocka_unit_test(f32Equality),
	cmocka_unit_test(f64Equality),
	cmocka_unit_test(stringEquality),
	cmocka_unit_test(hashTableBasic),
	cmocka_unit_test(hashTableString),
	cmocka_unit_test(stringIsHello),
	cmocka_unit_test(stringIsNotHello))
