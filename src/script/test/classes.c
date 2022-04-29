/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/script/context.h>
#include <mgba/script/types.h>

struct TestA {
	int32_t i;
	int32_t i2;
	int8_t b8;
	int16_t hUnaligned;
	int32_t (*ifn0)(struct TestA*);
	int32_t (*ifn1)(struct TestA*, int);
	void (*vfn0)(struct TestA*);
	void (*vfn1)(struct TestA*, int);
	int32_t (*icfn0)(const struct TestA*);
	int32_t (*icfn1)(const struct TestA*, int);
};

static int32_t testAi0(struct TestA* a) {
	return a->i;
}

static int32_t testAi1(struct TestA* a, int b) {
	return a->i + b;
}

static int32_t testAic0(const struct TestA* a) {
	return a->i;
}

static int32_t testAic1(const struct TestA* a, int b) {
	return a->i + b;
}

static void testAv0(struct TestA* a) {
	++a->i;
}

static void testAv1(struct TestA* a, int b) {
	a->i += b;
}

#define MEMBER_A_DOCSTRING "Member a"

mSCRIPT_DECLARE_STRUCT(TestA);
mSCRIPT_DECLARE_STRUCT_D_METHOD(TestA, S32, ifn0, 0);
mSCRIPT_DECLARE_STRUCT_D_METHOD(TestA, S32, ifn1, 1, S32);
mSCRIPT_DECLARE_STRUCT_CD_METHOD(TestA, S32, icfn0, 0);
mSCRIPT_DECLARE_STRUCT_CD_METHOD(TestA, S32, icfn1, 1, S32);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(TestA, vfn0, 0);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(TestA, vfn1, 1, S32);
mSCRIPT_DECLARE_STRUCT_METHOD(TestA, S32, i0, testAi0, 0);
mSCRIPT_DECLARE_STRUCT_METHOD(TestA, S32, i1, testAi1, 1, S32);
mSCRIPT_DECLARE_STRUCT_C_METHOD(TestA, S32, ic0, testAic0, 0);
mSCRIPT_DECLARE_STRUCT_C_METHOD(TestA, S32, ic1, testAic1, 1, S32);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(TestA, v0, testAv0, 0);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(TestA, v1, testAv1, 1, S32);

mSCRIPT_DEFINE_STRUCT(TestA)
	mSCRIPT_DEFINE_DOCSTRING(MEMBER_A_DOCSTRING)
	mSCRIPT_DEFINE_STRUCT_MEMBER(TestA, S32, i)
	mSCRIPT_DEFINE_STRUCT_MEMBER(TestA, S32, i2)
	mSCRIPT_DEFINE_STRUCT_MEMBER(TestA, S8, b8)
	mSCRIPT_DEFINE_STRUCT_MEMBER(TestA, S16, hUnaligned)
	mSCRIPT_DEFINE_STRUCT_METHOD(TestA, ifn0)
	mSCRIPT_DEFINE_STRUCT_METHOD(TestA, ifn1)
	mSCRIPT_DEFINE_STRUCT_METHOD(TestA, icfn0)
	mSCRIPT_DEFINE_STRUCT_METHOD(TestA, icfn1)
	mSCRIPT_DEFINE_STRUCT_METHOD(TestA, vfn0)
	mSCRIPT_DEFINE_STRUCT_METHOD(TestA, vfn1)
	mSCRIPT_DEFINE_STRUCT_METHOD(TestA, i0)
	mSCRIPT_DEFINE_STRUCT_METHOD(TestA, i1)
	mSCRIPT_DEFINE_STRUCT_METHOD(TestA, ic0)
	mSCRIPT_DEFINE_STRUCT_METHOD(TestA, ic1)
	mSCRIPT_DEFINE_STRUCT_METHOD(TestA, v0)
	mSCRIPT_DEFINE_STRUCT_METHOD(TestA, v1)

	mSCRIPT_DEFINE_DOCSTRING(MEMBER_A_DOCSTRING)
	mSCRIPT_DEFINE_STATIC_MEMBER(S32, s_i)
	mSCRIPT_DEFINE_STATIC_MEMBER(S32, s_i2)
	mSCRIPT_DEFINE_STATIC_MEMBER(S8, s_b8)
	mSCRIPT_DEFINE_STATIC_MEMBER(S16, s_hUnaligned)
mSCRIPT_DEFINE_END;

mSCRIPT_EXPORT_STRUCT(TestA);

M_TEST_DEFINE(testALayout) {
	struct mScriptTypeClass* cls = mSCRIPT_TYPE_MS_S(TestA)->details.cls;
	assert_false(cls->init);
	mScriptClassInit(cls);
	assert_true(cls->init);

	struct mScriptClassMember* member;

	// Instance members
	member = HashTableLookup(&cls->instanceMembers, "i");
	assert_non_null(member);
	assert_string_equal(member->name, "i");
	assert_string_equal(member->docstring, MEMBER_A_DOCSTRING);
	assert_ptr_equal(member->type, mSCRIPT_TYPE_MS_S32);
	assert_int_equal(member->offset, 0);

	member = HashTableLookup(&cls->instanceMembers, "i2");
	assert_non_null(member);
	assert_string_equal(member->name, "i2");
	assert_null(member->docstring);
	assert_ptr_equal(member->type, mSCRIPT_TYPE_MS_S32);
	assert_int_equal(member->offset, sizeof(int32_t));

	member = HashTableLookup(&cls->instanceMembers, "b8");
	assert_non_null(member);
	assert_string_equal(member->name, "b8");
	assert_null(member->docstring);
	assert_ptr_equal(member->type, mSCRIPT_TYPE_MS_S8);
	assert_int_equal(member->offset, sizeof(int32_t) * 2);

	member = HashTableLookup(&cls->instanceMembers, "hUnaligned");
	assert_non_null(member);
	assert_string_equal(member->name, "hUnaligned");
	assert_null(member->docstring);
	assert_ptr_equal(member->type, mSCRIPT_TYPE_MS_S16);
	assert_int_not_equal(member->offset, sizeof(int32_t) * 2 + 1);

	member = HashTableLookup(&cls->instanceMembers, "unknown");
	assert_null(member);

	// Static members
	member = HashTableLookup(&cls->staticMembers, "s_i");
	assert_non_null(member);
	assert_string_equal(member->name, "s_i");
	assert_string_equal(member->docstring, MEMBER_A_DOCSTRING);
	assert_ptr_equal(member->type, mSCRIPT_TYPE_MS_S32);
	assert_int_equal(member->offset, 0);

	member = HashTableLookup(&cls->staticMembers, "s_i2");
	assert_non_null(member);
	assert_string_equal(member->name, "s_i2");
	assert_null(member->docstring);
	assert_ptr_equal(member->type, mSCRIPT_TYPE_MS_S32);
	assert_int_equal(member->offset, sizeof(int32_t));

	member = HashTableLookup(&cls->staticMembers, "s_b8");
	assert_non_null(member);
	assert_string_equal(member->name, "s_b8");
	assert_null(member->docstring);
	assert_ptr_equal(member->type, mSCRIPT_TYPE_MS_S8);
	assert_int_equal(member->offset, sizeof(int32_t) * 2);

	member = HashTableLookup(&cls->staticMembers, "s_hUnaligned");
	assert_non_null(member);
	assert_string_equal(member->name, "s_hUnaligned");
	assert_null(member->docstring);
	assert_ptr_equal(member->type, mSCRIPT_TYPE_MS_S16);
	assert_int_not_equal(member->offset, sizeof(int32_t) * 2 + 1);

	member = HashTableLookup(&cls->staticMembers, "unknown");
	assert_null(member);

	mScriptClassDeinit(cls);
	assert_false(cls->init);
}

M_TEST_DEFINE(testATranslation) {
	struct mScriptTypeClass* cls = mSCRIPT_TYPE_MS_S(TestA)->details.cls;

	struct TestA s = {
		.i = 1,
		.i2 = 2,
		.b8 = 3,
		.hUnaligned = 4
	};

	struct mScriptValue sval = mSCRIPT_MAKE_S(TestA, &s);
	struct mScriptValue val;
	struct mScriptValue compare;

	compare = mSCRIPT_MAKE_S32(1);
	assert_true(mScriptObjectGet(&sval, "i", &val));
	assert_true(compare.type->equal(&compare, &val));

	compare = mSCRIPT_MAKE_S32(2);
	assert_true(mScriptObjectGet(&sval, "i2", &val));
	assert_true(compare.type->equal(&compare, &val));

	compare = mSCRIPT_MAKE_S32(3);
	assert_true(mScriptObjectGet(&sval, "b8", &val));
	assert_true(compare.type->equal(&compare, &val));

	compare = mSCRIPT_MAKE_S32(4);
	assert_true(mScriptObjectGet(&sval, "hUnaligned", &val));
	assert_true(compare.type->equal(&compare, &val));

	assert_true(cls->init);
	mScriptClassDeinit(cls);
	assert_false(cls->init);
}

M_TEST_DEFINE(testAStatic) {
	struct mScriptTypeClass* cls = mSCRIPT_TYPE_MS_S(TestA)->details.cls;
	assert_false(cls->init);
	mScriptClassInit(cls);
	assert_true(cls->init);

	struct TestA s = {
		.i = 1,
	};

	struct mScriptValue sval = mSCRIPT_MAKE_S(TestA, &s);
	struct mScriptValue val;
	struct mScriptFrame frame;
	int32_t rval;

	assert_true(mScriptObjectGet(&sval, "i0", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(TestA), &s);
	assert_true(mScriptInvoke(&val, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &rval));
	assert_int_equal(rval, 1);
	mScriptFrameDeinit(&frame);

	assert_true(mScriptObjectGet(&sval, "i1", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(TestA), &s);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
	assert_true(mScriptInvoke(&val, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &rval));
	assert_int_equal(rval, 2);
	mScriptFrameDeinit(&frame);

	assert_true(mScriptObjectGet(&sval, "ic0", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, CS(TestA), &s);
	assert_true(mScriptInvoke(&val, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &rval));
	assert_int_equal(rval, 1);
	mScriptFrameDeinit(&frame);

	assert_true(mScriptObjectGet(&sval, "ic0", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(TestA), &s);
	assert_true(mScriptInvoke(&val, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &rval));
	assert_int_equal(rval, 1);
	mScriptFrameDeinit(&frame);

	assert_true(mScriptObjectGet(&sval, "ic1", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, CS(TestA), &s);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
	assert_true(mScriptInvoke(&val, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &rval));
	assert_int_equal(rval, 2);
	mScriptFrameDeinit(&frame);

	assert_true(mScriptObjectGet(&sval, "ic1", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(TestA), &s);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
	assert_true(mScriptInvoke(&val, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &rval));
	assert_int_equal(rval, 2);
	mScriptFrameDeinit(&frame);

	assert_true(mScriptObjectGet(&sval, "v0", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(TestA), &s);
	assert_true(mScriptInvoke(&val, &frame));
	mScriptFrameDeinit(&frame);
	assert_true(mScriptObjectGet(&sval, "i0", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(TestA), &s);
	assert_true(mScriptInvoke(&val, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &rval));
	assert_int_equal(rval, 2);
	mScriptFrameDeinit(&frame);
	assert_true(mScriptObjectGet(&sval, "ic0", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, CS(TestA), &s);
	assert_true(mScriptInvoke(&val, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &rval));
	assert_int_equal(rval, 2);
	mScriptFrameDeinit(&frame);

	assert_true(mScriptObjectGet(&sval, "v1", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(TestA), &s);
	mSCRIPT_PUSH(&frame.arguments, S32, 2);
	assert_true(mScriptInvoke(&val, &frame));
	mScriptFrameDeinit(&frame);
	assert_true(mScriptObjectGet(&sval, "i0", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(TestA), &s);
	assert_true(mScriptInvoke(&val, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &rval));
	assert_int_equal(rval, 4);
	mScriptFrameDeinit(&frame);
	assert_true(mScriptObjectGet(&sval, "ic0", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, CS(TestA), &s);
	assert_true(mScriptInvoke(&val, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &rval));
	assert_int_equal(rval, 4);
	mScriptFrameDeinit(&frame);

	assert_true(cls->init);
	mScriptClassDeinit(cls);
	assert_false(cls->init);
}

M_TEST_DEFINE(testADynamic) {
	struct mScriptTypeClass* cls = mSCRIPT_TYPE_MS_S(TestA)->details.cls;
	assert_false(cls->init);
	mScriptClassInit(cls);
	assert_true(cls->init);

	struct mScriptClassMember* member;

	// Instance methods
	member = HashTableLookup(&cls->instanceMembers, "ifn0");
	assert_non_null(member);
	assert_string_equal(member->name, "ifn0");
	assert_int_equal(member->type->base, mSCRIPT_TYPE_FUNCTION);
	assert_int_equal(member->type->details.function.parameters.count, 1);
	assert_ptr_equal(member->type->details.function.parameters.entries[0], mSCRIPT_TYPE_MS_S(TestA));

	struct TestA s = {
		.i = 1,
		.ifn0 = testAi0,
		.ifn1 = testAi1,
		.icfn0 = testAic0,
		.icfn1 = testAic1,
		.vfn0 = testAv0,
		.vfn1 = testAv1,
	};

	struct mScriptValue sval = mSCRIPT_MAKE_S(TestA, &s);
	struct mScriptValue val;
	struct mScriptFrame frame;
	int32_t rval;

	assert_true(mScriptObjectGet(&sval, "ifn0", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(TestA), &s);
	assert_true(mScriptInvoke(&val, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &rval));
	assert_int_equal(rval, 1);
	mScriptFrameDeinit(&frame);

	assert_true(mScriptObjectGet(&sval, "ifn1", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(TestA), &s);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
	assert_true(mScriptInvoke(&val, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &rval));
	assert_int_equal(rval, 2);
	mScriptFrameDeinit(&frame);

	assert_true(mScriptObjectGet(&sval, "icfn0", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, CS(TestA), &s);
	assert_true(mScriptInvoke(&val, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &rval));
	assert_int_equal(rval, 1);
	mScriptFrameDeinit(&frame);

	assert_true(mScriptObjectGet(&sval, "icfn0", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(TestA), &s);
	assert_true(mScriptInvoke(&val, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &rval));
	assert_int_equal(rval, 1);
	mScriptFrameDeinit(&frame);

	assert_true(mScriptObjectGet(&sval, "icfn1", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, CS(TestA), &s);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
	assert_true(mScriptInvoke(&val, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &rval));
	assert_int_equal(rval, 2);
	mScriptFrameDeinit(&frame);

	assert_true(mScriptObjectGet(&sval, "icfn1", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(TestA), &s);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
	assert_true(mScriptInvoke(&val, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &rval));
	assert_int_equal(rval, 2);
	mScriptFrameDeinit(&frame);

	assert_true(mScriptObjectGet(&sval, "vfn0", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(TestA), &s);
	assert_true(mScriptInvoke(&val, &frame));
	mScriptFrameDeinit(&frame);
	assert_true(mScriptObjectGet(&sval, "ifn0", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(TestA), &s);
	assert_true(mScriptInvoke(&val, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &rval));
	assert_int_equal(rval, 2);
	mScriptFrameDeinit(&frame);
	assert_true(mScriptObjectGet(&sval, "icfn0", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, CS(TestA), &s);
	assert_true(mScriptInvoke(&val, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &rval));
	assert_int_equal(rval, 2);
	mScriptFrameDeinit(&frame);

	assert_true(mScriptObjectGet(&sval, "vfn1", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(TestA), &s);
	mSCRIPT_PUSH(&frame.arguments, S32, 2);
	assert_true(mScriptInvoke(&val, &frame));
	mScriptFrameDeinit(&frame);
	assert_true(mScriptObjectGet(&sval, "ifn0", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(TestA), &s);
	assert_true(mScriptInvoke(&val, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &rval));
	assert_int_equal(rval, 4);
	mScriptFrameDeinit(&frame);
	assert_true(mScriptObjectGet(&sval, "icfn0", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, CS(TestA), &s);
	assert_true(mScriptInvoke(&val, &frame));
	assert_true(mScriptPopS32(&frame.returnValues, &rval));
	assert_int_equal(rval, 4);
	mScriptFrameDeinit(&frame);

	assert_true(cls->init);
	mScriptClassDeinit(cls);
	assert_false(cls->init);
}

M_TEST_SUITE_DEFINE(mScriptClasses,
	cmocka_unit_test(testALayout),
	cmocka_unit_test(testATranslation),
	cmocka_unit_test(testAStatic),
	cmocka_unit_test(testADynamic))
