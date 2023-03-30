/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/script.h>

struct TestA {
	int32_t i;
	int32_t i2;
	int8_t b8;
	int16_t hUnaligned;
	char str[6];
	struct mScriptValue table;
	struct mScriptList list;
	int32_t (*ifn0)(struct TestA*);
	int32_t (*ifn1)(struct TestA*, int);
	void (*vfn0)(struct TestA*);
	void (*vfn1)(struct TestA*, int);
	int32_t (*icfn0)(const struct TestA*);
	int32_t (*icfn1)(const struct TestA*, int);
};

struct TestB {
	struct TestA d;
	int32_t i3;
};

struct TestC {
	int32_t i;
};

struct TestD {
	struct TestC a;
	struct TestC b;
};

struct TestE {
};

struct TestF {
	int* ref;
};

struct TestG {
	const char* name;
	int64_t s;
	uint64_t u;
	double f;
	const char* c;
};

struct TestH {
	int32_t i;
	int32_t j;
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

static void testAv2(struct TestA* a, int b, int c) {
	a->i += b + c;
}

static int32_t testGet(struct TestE* e, const char* name) {
	UNUSED(e);
	return name[0];
}

static void testDeinit(struct TestF* f) {
	++*f->ref;
}

static void testSetS(struct TestG* g, const char* name, int64_t val) {
	g->name = name;
	g->s = val;
}

static void testSetU(struct TestG* g, const char* name, uint64_t val) {
	g->name = name;
	g->u = val;
}

static void testSetF(struct TestG* g, const char* name, double val) {
	g->name = name;
	g->f = val;
}

static void testSetC(struct TestG* g, const char* name, const char* val) {
	g->name = name;
	g->c = val;
}

#define MEMBER_A_DOCSTRING "Member a"

mSCRIPT_DECLARE_STRUCT(TestA);
mSCRIPT_DECLARE_STRUCT_D_METHOD(TestA, S32, ifn0, 0);
mSCRIPT_DECLARE_STRUCT_D_METHOD(TestA, S32, ifn1, 1, S32, b);
mSCRIPT_DECLARE_STRUCT_CD_METHOD(TestA, S32, icfn0, 0);
mSCRIPT_DECLARE_STRUCT_CD_METHOD(TestA, S32, icfn1, 1, S32, b);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(TestA, vfn0, 0);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(TestA, vfn1, 1, S32, b);
mSCRIPT_DECLARE_STRUCT_METHOD(TestA, S32, i0, testAi0, 0);
mSCRIPT_DECLARE_STRUCT_METHOD(TestA, S32, i1, testAi1, 1, S32, b);
mSCRIPT_DECLARE_STRUCT_C_METHOD(TestA, S32, ic0, testAic0, 0);
mSCRIPT_DECLARE_STRUCT_C_METHOD(TestA, S32, ic1, testAic1, 1, S32, b);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(TestA, v0, testAv0, 0);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(TestA, v1, testAv1, 1, S32, b);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD_WITH_DEFAULTS(TestA, v2, testAv2, 2, S32, b, S32, c);

mSCRIPT_DEFINE_STRUCT(TestA)
	mSCRIPT_DEFINE_DOCSTRING(MEMBER_A_DOCSTRING)
	mSCRIPT_DEFINE_STRUCT_MEMBER(TestA, S32, i)
	mSCRIPT_DEFINE_STRUCT_MEMBER(TestA, S32, i2)
	mSCRIPT_DEFINE_STRUCT_MEMBER(TestA, S8, b8)
	mSCRIPT_DEFINE_STRUCT_MEMBER(TestA, S16, hUnaligned)
	mSCRIPT_DEFINE_STRUCT_MEMBER(TestA, CHARP, str)
	mSCRIPT_DEFINE_STRUCT_MEMBER(TestA, TABLE, table)
	mSCRIPT_DEFINE_STRUCT_MEMBER(TestA, LIST, list)
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
	mSCRIPT_DEFINE_STRUCT_METHOD(TestA, v2)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT_BINDING_DEFAULTS(TestA, v2)
	mSCRIPT_NO_DEFAULT,
	mSCRIPT_MAKE_S32(0)
mSCRIPT_DEFINE_DEFAULTS_END;

mSCRIPT_DEFINE_STRUCT(TestB)
	mSCRIPT_DEFINE_INHERIT(TestA)
	mSCRIPT_DEFINE_STRUCT_MEMBER(TestB, S32, i3)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT(TestC)
	mSCRIPT_DEFINE_STRUCT_MEMBER(TestC, S32, i)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT(TestD)
	mSCRIPT_DEFINE_STRUCT_MEMBER(TestD, S(TestC), a)
	mSCRIPT_DEFINE_STRUCT_MEMBER(TestD, S(TestC), b)
mSCRIPT_DEFINE_END;

mSCRIPT_DECLARE_STRUCT(TestE);
mSCRIPT_DECLARE_STRUCT_METHOD(TestE, S32, _get, testGet, 1, CHARP, name);

mSCRIPT_DEFINE_STRUCT(TestE)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_GET(TestE)
mSCRIPT_DEFINE_END;

mSCRIPT_DECLARE_STRUCT(TestF);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(TestF, _deinit, testDeinit, 0);

mSCRIPT_DEFINE_STRUCT(TestF)
	mSCRIPT_DEFINE_STRUCT_DEINIT(TestF)
mSCRIPT_DEFINE_END;

mSCRIPT_DECLARE_STRUCT(TestG);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(TestG, setS, testSetS, 2, CHARP, name, S64, value);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(TestG, setU, testSetU, 2, CHARP, name, U64, value);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(TestG, setF, testSetF, 2, CHARP, name, F64, value);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(TestG, setC, testSetC, 2, CHARP, name, CHARP, value);

mSCRIPT_DEFINE_STRUCT(TestG)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_SET(TestG, setS)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_SET(TestG, setU)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_SET(TestG, setF)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_SET(TestG, setC)
mSCRIPT_DEFINE_END;


mSCRIPT_DEFINE_STRUCT(TestH)
	mSCRIPT_DEFINE_STRUCT_MEMBER(TestH, S32, i)
	mSCRIPT_DEFINE_STRUCT_CONST_MEMBER(TestH, S32, j)
mSCRIPT_DEFINE_END;

M_TEST_DEFINE(testALayout) {
	struct mScriptTypeClass* cls = mSCRIPT_TYPE_MS_S(TestA)->details.cls;
	assert_false(cls->init);
	mScriptClassInit(cls);
	assert_true(cls->init);

	struct mScriptClassMember* member;

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

	member = HashTableLookup(&cls->instanceMembers, "str");
	assert_non_null(member);
	assert_string_equal(member->name, "str");
	assert_null(member->docstring);
	assert_ptr_equal(member->type, mSCRIPT_TYPE_MS_CHARP);
	assert_int_equal(member->offset, &((struct TestA*) 0)->str);

	member = HashTableLookup(&cls->instanceMembers, "table");
	assert_non_null(member);
	assert_string_equal(member->name, "table");
	assert_null(member->docstring);
	assert_ptr_equal(member->type, mSCRIPT_TYPE_MS_TABLE);
	assert_int_equal(member->offset, &((struct TestA*) 0)->table);

	member = HashTableLookup(&cls->instanceMembers, "list");
	assert_non_null(member);
	assert_string_equal(member->name, "list");
	assert_null(member->docstring);
	assert_ptr_equal(member->type, mSCRIPT_TYPE_MS_LIST);
	assert_int_equal(member->offset, &((struct TestA*) 0)->list);

	member = HashTableLookup(&cls->instanceMembers, "unknown");
	assert_null(member);

	mScriptClassDeinit(cls);
	assert_false(cls->init);
}

M_TEST_DEFINE(testASignatures) {
	struct mScriptTypeClass* cls = mSCRIPT_TYPE_MS_S(TestA)->details.cls;
	assert_false(cls->init);
	mScriptClassInit(cls);
	assert_true(cls->init);

	struct mScriptClassMember* member;

	member = HashTableLookup(&cls->instanceMembers, "ifn0");
	assert_non_null(member);
	assert_string_equal(member->name, "ifn0");
	assert_int_equal(member->type->base, mSCRIPT_TYPE_FUNCTION);
	assert_int_equal(member->type->details.function.parameters.count, 1);
	assert_ptr_equal(member->type->details.function.parameters.entries[0], mSCRIPT_TYPE_MS_S(TestA));
	assert_string_equal(member->type->details.function.parameters.names[0], "this");
	assert_int_equal(member->type->details.function.returnType.count, 1);
	assert_ptr_equal(member->type->details.function.returnType.entries[0], mSCRIPT_TYPE_MS_S32);

	member = HashTableLookup(&cls->instanceMembers, "ifn1");
	assert_non_null(member);
	assert_string_equal(member->name, "ifn1");
	assert_int_equal(member->type->base, mSCRIPT_TYPE_FUNCTION);
	assert_int_equal(member->type->details.function.parameters.count, 2);
	assert_ptr_equal(member->type->details.function.parameters.entries[0], mSCRIPT_TYPE_MS_S(TestA));
	assert_ptr_equal(member->type->details.function.parameters.entries[1], mSCRIPT_TYPE_MS_S32);
	assert_string_equal(member->type->details.function.parameters.names[0], "this");
	assert_string_equal(member->type->details.function.parameters.names[1], "b");
	assert_int_equal(member->type->details.function.returnType.count, 1);
	assert_ptr_equal(member->type->details.function.returnType.entries[0], mSCRIPT_TYPE_MS_S32);

	member = HashTableLookup(&cls->instanceMembers, "vfn0");
	assert_non_null(member);
	assert_string_equal(member->name, "vfn0");
	assert_int_equal(member->type->base, mSCRIPT_TYPE_FUNCTION);
	assert_int_equal(member->type->details.function.parameters.count, 1);
	assert_ptr_equal(member->type->details.function.parameters.entries[0], mSCRIPT_TYPE_MS_S(TestA));
	assert_string_equal(member->type->details.function.parameters.names[0], "this");
	assert_int_equal(member->type->details.function.returnType.count, 0);

	member = HashTableLookup(&cls->instanceMembers, "vfn1");
	assert_non_null(member);
	assert_string_equal(member->name, "vfn1");
	assert_int_equal(member->type->base, mSCRIPT_TYPE_FUNCTION);
	assert_int_equal(member->type->details.function.parameters.count, 2);
	assert_ptr_equal(member->type->details.function.parameters.entries[0], mSCRIPT_TYPE_MS_S(TestA));
	assert_ptr_equal(member->type->details.function.parameters.entries[1], mSCRIPT_TYPE_MS_S32);
	assert_string_equal(member->type->details.function.parameters.names[0], "this");
	assert_string_equal(member->type->details.function.parameters.names[1], "b");
	assert_int_equal(member->type->details.function.returnType.count, 0);

	member = HashTableLookup(&cls->instanceMembers, "icfn0");
	assert_non_null(member);
	assert_string_equal(member->name, "icfn0");
	assert_int_equal(member->type->base, mSCRIPT_TYPE_FUNCTION);
	assert_int_equal(member->type->details.function.parameters.count, 1);
	assert_ptr_equal(member->type->details.function.parameters.entries[0], mSCRIPT_TYPE_MS_CS(TestA));
	assert_string_equal(member->type->details.function.parameters.names[0], "this");
	assert_int_equal(member->type->details.function.returnType.count, 1);
	assert_ptr_equal(member->type->details.function.returnType.entries[0], mSCRIPT_TYPE_MS_S32);

	member = HashTableLookup(&cls->instanceMembers, "icfn1");
	assert_non_null(member);
	assert_string_equal(member->name, "icfn1");
	assert_int_equal(member->type->base, mSCRIPT_TYPE_FUNCTION);
	assert_int_equal(member->type->details.function.parameters.count, 2);
	assert_ptr_equal(member->type->details.function.parameters.entries[0], mSCRIPT_TYPE_MS_CS(TestA));
	assert_ptr_equal(member->type->details.function.parameters.entries[1], mSCRIPT_TYPE_MS_S32);
	assert_string_equal(member->type->details.function.parameters.names[0], "this");
	assert_string_equal(member->type->details.function.parameters.names[1], "b");
	assert_int_equal(member->type->details.function.returnType.count, 1);
	assert_ptr_equal(member->type->details.function.returnType.entries[0], mSCRIPT_TYPE_MS_S32);

	mScriptClassDeinit(cls);
	assert_false(cls->init);
}

M_TEST_DEFINE(testAGet) {
	struct mScriptTypeClass* cls = mSCRIPT_TYPE_MS_S(TestA)->details.cls;

	struct TestA s = {
		.i = 1,
		.i2 = 2,
		.b8 = 3,
		.hUnaligned = 4
	};

	mScriptListInit(&s.list, 1);
	*mScriptListAppend(&s.list) = mSCRIPT_MAKE_S32(5);

	s.table.type = mSCRIPT_TYPE_MS_TABLE;
	s.table.type->alloc(&s.table);

	strcpy(s.str, "test");

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

	compare = mSCRIPT_MAKE_CHARP("test");
	assert_true(mScriptObjectGet(&sval, "str", &val));
	assert_true(compare.type->equal(&compare, &val));

	compare = mSCRIPT_MAKE_S32(5);
	assert_true(mScriptObjectGet(&sval, "list", &val));
	assert_ptr_equal(val.type, mSCRIPT_TYPE_MS_LIST);
	assert_int_equal(mScriptListSize(val.value.list), 1);
	assert_true(compare.type->equal(&compare, mScriptListGetPointer(val.value.list, 0)));

	*mScriptListAppend(&s.list) = mSCRIPT_MAKE_S32(6);
	compare = mSCRIPT_MAKE_S32(6);
	assert_int_equal(mScriptListSize(val.value.list), 2);
	assert_true(compare.type->equal(&compare, mScriptListGetPointer(val.value.list, 1)));

	struct mScriptValue* ival = &val;
	assert_true(mScriptObjectGet(&sval, "table", &val));
	if (val.type->base == mSCRIPT_TYPE_WRAPPER) {
		ival = mScriptValueUnwrap(&val);
	}
	assert_ptr_equal(ival->type, mSCRIPT_TYPE_MS_TABLE);
	assert_int_equal(mScriptTableSize(ival), 0);
	compare = mSCRIPT_MAKE_S32(7);
	mScriptTableInsert(&s.table, &compare, &compare);
	assert_int_equal(mScriptTableSize(&s.table), 1);
	assert_int_equal(mScriptTableSize(ival), 1);

	assert_false(mScriptObjectGet(&sval, "unknown", &val));

	mScriptListDeinit(&s.list);
	mSCRIPT_TYPE_MS_TABLE->free(&s.table);

	assert_true(cls->init);
	mScriptClassDeinit(cls);
	assert_false(cls->init);
}

M_TEST_DEFINE(testASet) {
	struct mScriptTypeClass* cls = mSCRIPT_TYPE_MS_S(TestA)->details.cls;

	struct TestA s = {
		.i = 1,
		.i2 = 2,
		.b8 = 3,
		.hUnaligned = 4
	};

	struct mScriptValue sval = mSCRIPT_MAKE_S(TestA, &s);
	struct mScriptValue val;

	val = mSCRIPT_MAKE_S32(2);
	assert_true(mScriptObjectSet(&sval, "i", &val));
	assert_int_equal(s.i, 2);

	val = mSCRIPT_MAKE_S32(3);
	assert_true(mScriptObjectSet(&sval, "i2", &val));
	assert_int_equal(s.i2, 3);

	val = mSCRIPT_MAKE_S32(4);
	assert_true(mScriptObjectSet(&sval, "b8", &val));
	assert_int_equal(s.b8, 4);

	val = mSCRIPT_MAKE_S32(5);
	assert_true(mScriptObjectSet(&sval, "hUnaligned", &val));
	assert_int_equal(s.hUnaligned, 5);

	sval = mSCRIPT_MAKE_CS(TestA, &s);

	val = mSCRIPT_MAKE_S32(3);
	assert_false(mScriptObjectSet(&sval, "i", &val));
	assert_int_equal(s.i, 2);

	val = mSCRIPT_MAKE_S32(4);
	assert_false(mScriptObjectSet(&sval, "i2", &val));
	assert_int_equal(s.i2, 3);

	val = mSCRIPT_MAKE_S32(5);
	assert_false(mScriptObjectSet(&sval, "b8", &val));
	assert_int_equal(s.b8, 4);

	val = mSCRIPT_MAKE_S32(6);
	assert_false(mScriptObjectSet(&sval, "hUnaligned", &val));
	assert_int_equal(s.hUnaligned, 5);

	assert_false(mScriptObjectSet(&sval, "unknown", &val));

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
	assert_true(mScriptObjectGet(&sval, "v2", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(TestA), &s);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
	mSCRIPT_PUSH(&frame.arguments, S32, -2);
	assert_true(mScriptInvoke(&val, &frame));
	mScriptFrameDeinit(&frame);
	assert_true(mScriptObjectGet(&sval, "v2", &val));
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S(TestA), &s);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
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

M_TEST_DEFINE(testBLayout) {
	struct mScriptTypeClass* cls = mSCRIPT_TYPE_MS_S(TestB)->details.cls;
	assert_false(cls->init);
	mScriptClassInit(cls);
	assert_true(cls->init);

	struct mScriptClassMember* member;

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
	size_t hOffset = member->offset;

	member = HashTableLookup(&cls->instanceMembers, "i3");
	assert_non_null(member);
	assert_string_equal(member->name, "i3");
	assert_null(member->docstring);
	assert_ptr_equal(member->type, mSCRIPT_TYPE_MS_S32);
	assert_true(member->offset >= hOffset + sizeof(int16_t));

	member = HashTableLookup(&cls->instanceMembers, "_super");
	assert_non_null(member);
	assert_string_equal(member->name, "_super");
	assert_null(member->docstring);
	assert_ptr_equal(member->type, mSCRIPT_TYPE_MS_S(TestA));
	assert_int_equal(member->offset, 0);

	member = HashTableLookup(&cls->instanceMembers, "unknown");
	assert_null(member);

	mScriptClassDeinit(cls);
	assert_false(cls->init);
}

M_TEST_DEFINE(testBGet) {
	struct mScriptTypeClass* cls = mSCRIPT_TYPE_MS_S(TestB)->details.cls;

	struct TestB s = {
		.d = {
			.i = 1,
			.i2 = 2,
			.b8 = 3,
			.hUnaligned = 4
		},
		.i3 = 5
	};

	struct mScriptValue sval = mSCRIPT_MAKE_S(TestB, &s);
	struct mScriptValue super;
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

	compare = mSCRIPT_MAKE_S32(5);
	assert_true(mScriptObjectGet(&sval, "i3", &val));
	assert_true(compare.type->equal(&compare, &val));

	// Superclass explicit access
	assert_true(mScriptObjectGet(&sval, "_super", &super));
	assert_true(super.type == mSCRIPT_TYPE_MS_S(TestA));

	compare = mSCRIPT_MAKE_S32(1);
	assert_true(mScriptObjectGet(&super, "i", &val));
	assert_true(compare.type->equal(&compare, &val));

	compare = mSCRIPT_MAKE_S32(2);
	assert_true(mScriptObjectGet(&super, "i2", &val));
	assert_true(compare.type->equal(&compare, &val));

	compare = mSCRIPT_MAKE_S32(3);
	assert_true(mScriptObjectGet(&super, "b8", &val));
	assert_true(compare.type->equal(&compare, &val));

	compare = mSCRIPT_MAKE_S32(4);
	assert_true(mScriptObjectGet(&super, "hUnaligned", &val));
	assert_true(compare.type->equal(&compare, &val));

	assert_false(mScriptObjectGet(&super, "i3", &val));

	// Test const-correctness
	sval = mSCRIPT_MAKE_CS(TestB, &s);
	assert_true(mScriptObjectGet(&sval, "_super", &super));
	assert_true(super.type == mSCRIPT_TYPE_MS_CS(TestA));

	assert_true(cls->init);
	mScriptClassDeinit(cls);
	assert_false(cls->init);
}

M_TEST_DEFINE(testBSet) {
	struct mScriptTypeClass* cls = mSCRIPT_TYPE_MS_S(TestB)->details.cls;

	struct TestB s = {
		.d = {
			.i = 1,
			.i2 = 2,
			.b8 = 3,
			.hUnaligned = 4
		},
		.i3 = 5
	};

	struct mScriptValue sval = mSCRIPT_MAKE_S(TestB, &s);
	struct mScriptValue super;
	struct mScriptValue val;

	val = mSCRIPT_MAKE_S32(2);
	assert_true(mScriptObjectSet(&sval, "i", &val));
	assert_int_equal(s.d.i, 2);

	val = mSCRIPT_MAKE_S32(3);
	assert_true(mScriptObjectSet(&sval, "i2", &val));
	assert_int_equal(s.d.i2, 3);

	val = mSCRIPT_MAKE_S32(4);
	assert_true(mScriptObjectSet(&sval, "b8", &val));
	assert_int_equal(s.d.b8, 4);

	val = mSCRIPT_MAKE_S32(5);
	assert_true(mScriptObjectSet(&sval, "hUnaligned", &val));
	assert_int_equal(s.d.hUnaligned, 5);

	val = mSCRIPT_MAKE_S32(6);
	assert_true(mScriptObjectSet(&sval, "i3", &val));
	assert_int_equal(s.i3, 6);

	// Superclass explicit access
	assert_true(mScriptObjectGet(&sval, "_super", &super));
	assert_true(super.type == mSCRIPT_TYPE_MS_S(TestA));

	val = mSCRIPT_MAKE_S32(3);
	assert_true(mScriptObjectSet(&super, "i", &val));
	assert_int_equal(s.d.i, 3);

	val = mSCRIPT_MAKE_S32(4);
	assert_true(mScriptObjectSet(&super, "i2", &val));
	assert_int_equal(s.d.i2, 4);

	val = mSCRIPT_MAKE_S32(5);
	assert_true(mScriptObjectSet(&super, "b8", &val));
	assert_int_equal(s.d.b8, 5);

	val = mSCRIPT_MAKE_S32(6);
	assert_true(mScriptObjectSet(&super, "hUnaligned", &val));
	assert_int_equal(s.d.hUnaligned, 6);

	val = mSCRIPT_MAKE_S32(7);
	assert_false(mScriptObjectSet(&super, "i3", &val));
	assert_int_equal(s.i3, 6);

	// Const access
	sval = mSCRIPT_MAKE_CS(TestB, &s);

	val = mSCRIPT_MAKE_S32(4);
	assert_false(mScriptObjectSet(&sval, "i", &val));
	assert_int_equal(s.d.i, 3);

	val = mSCRIPT_MAKE_S32(5);
	assert_false(mScriptObjectSet(&sval, "i2", &val));
	assert_int_equal(s.d.i2, 4);

	val = mSCRIPT_MAKE_S32(6);
	assert_false(mScriptObjectSet(&sval, "b8", &val));
	assert_int_equal(s.d.b8, 5);

	val = mSCRIPT_MAKE_S32(7);
	assert_false(mScriptObjectSet(&sval, "hUnaligned", &val));
	assert_int_equal(s.d.hUnaligned, 6);

	val = mSCRIPT_MAKE_S32(8);
	assert_false(mScriptObjectSet(&sval, "i3", &val));
	assert_int_equal(s.i3, 6);

	assert_true(cls->init);
	mScriptClassDeinit(cls);
	assert_false(cls->init);
}

M_TEST_DEFINE(testDLayout) {
	struct mScriptTypeClass* cls = mSCRIPT_TYPE_MS_S(TestD)->details.cls;
	assert_false(cls->init);
	mScriptClassInit(cls);
	assert_true(cls->init);

	struct mScriptClassMember* member;

	member = HashTableLookup(&cls->instanceMembers, "a");
	assert_non_null(member);
	assert_string_equal(member->name, "a");
	assert_null(member->docstring);
	assert_ptr_equal(member->type, mSCRIPT_TYPE_MS_S(TestC));
	assert_int_equal(member->offset, 0);

	member = HashTableLookup(&cls->instanceMembers, "b");
	assert_non_null(member);
	assert_string_equal(member->name, "b");
	assert_null(member->docstring);
	assert_ptr_equal(member->type, mSCRIPT_TYPE_MS_S(TestC));
	assert_int_equal(member->offset, sizeof(struct TestC));

	mScriptClassDeinit(cls);
	assert_false(cls->init);
}

M_TEST_DEFINE(testDGet) {
	struct mScriptTypeClass* cls = mSCRIPT_TYPE_MS_S(TestD)->details.cls;

	struct TestD s = {
		.a = { 1 },
		.b = { 2 },
	};

	struct mScriptValue sval = mSCRIPT_MAKE_S(TestD, &s);
	struct mScriptValue val;
	struct mScriptValue member;
	struct mScriptValue compare;

	compare = mSCRIPT_MAKE_S32(1);
	assert_true(mScriptObjectGet(&sval, "a", &member));
	assert_true(mScriptObjectGet(&member, "i", &val));
	assert_true(compare.type->equal(&compare, &val));

	compare = mSCRIPT_MAKE_S32(2);
	assert_true(mScriptObjectGet(&sval, "b", &member));
	assert_true(mScriptObjectGet(&member, "i", &val));
	assert_true(compare.type->equal(&compare, &val));

	assert_true(cls->init);
	mScriptClassDeinit(cls);
	assert_false(cls->init);
}

M_TEST_DEFINE(testDSet) {
	struct mScriptTypeClass* cls = mSCRIPT_TYPE_MS_S(TestD)->details.cls;

	struct TestD s = {
		.a = { 1 },
		.b = { 2 },
	};

	struct mScriptValue sval = mSCRIPT_MAKE_S(TestD, &s);
	struct mScriptValue member;
	struct mScriptValue val;

	val = mSCRIPT_MAKE_S32(2);
	assert_true(mScriptObjectGet(&sval, "a", &member));
	assert_true(mScriptObjectSet(&member, "i", &val));
	assert_int_equal(s.a.i, 2);
	assert_int_equal(s.b.i, 2);

	val = mSCRIPT_MAKE_S32(3);
	assert_true(mScriptObjectGet(&sval, "b", &member));
	assert_true(mScriptObjectSet(&member, "i", &val));
	assert_int_equal(s.a.i, 2);
	assert_int_equal(s.b.i, 3);

	sval = mSCRIPT_MAKE_CS(TestD, &s);

	val = mSCRIPT_MAKE_S32(4);
	assert_true(mScriptObjectGet(&sval, "a", &member));
	assert_false(mScriptObjectSet(&member, "i", &val));
	assert_int_equal(s.a.i, 2);
	assert_int_equal(s.b.i, 3);

	val = mSCRIPT_MAKE_S32(5);
	assert_true(mScriptObjectGet(&sval, "b", &member));
	assert_false(mScriptObjectSet(&member, "i", &val));
	assert_int_equal(s.a.i, 2);
	assert_int_equal(s.b.i, 3);

	assert_true(cls->init);
	mScriptClassDeinit(cls);
	assert_false(cls->init);
}

M_TEST_DEFINE(testEGet) {
	struct mScriptTypeClass* cls = mSCRIPT_TYPE_MS_S(TestE)->details.cls;

	struct TestE s = {
	};

	struct mScriptValue sval = mSCRIPT_MAKE_S(TestE, &s);
	struct mScriptValue val;
	struct mScriptValue compare;

	compare = mSCRIPT_MAKE_S32('a');
	assert_true(mScriptObjectGet(&sval, "a", &val));
	assert_true(compare.type->equal(&compare, &val));

	compare = mSCRIPT_MAKE_S32('b');
	assert_true(mScriptObjectGet(&sval, "b", &val));
	assert_true(compare.type->equal(&compare, &val));

	assert_true(cls->init);
	mScriptClassDeinit(cls);
	assert_false(cls->init);
}

M_TEST_DEFINE(testFDeinit) {
	struct mScriptTypeClass* cls = mSCRIPT_TYPE_MS_S(TestF)->details.cls;

	int ref = 0;
	struct TestF* s = calloc(1, sizeof(struct TestF));
	s->ref = &ref;
	struct mScriptValue* val = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(TestF));
	val->value.opaque = s;
	val->flags = mSCRIPT_VALUE_FLAG_FREE_BUFFER;
	mScriptValueDeref(val);
	assert_int_equal(ref, 1);

	assert_true(cls->init);
	mScriptClassDeinit(cls);
	assert_false(cls->init);
}

M_TEST_DEFINE(testGSet) {
	struct mScriptTypeClass* cls = mSCRIPT_TYPE_MS_S(TestG)->details.cls;

	struct TestG s = {
	};

	assert_int_equal(s.s, 0);
	assert_int_equal(s.u, 0);
	assert_float_equal(s.f, 0, 0);
	assert_null(s.c);

	struct mScriptValue sval = mSCRIPT_MAKE_S(TestG, &s);
	struct mScriptValue val;
	struct mScriptValue* pval;

	val = mSCRIPT_MAKE_S64(1);
	assert_true(mScriptObjectSet(&sval, "a", &val));
	assert_int_equal(s.s, 1);
	assert_string_equal(s.name, "a");

	val = mSCRIPT_MAKE_U64(2);
	assert_true(mScriptObjectSet(&sval, "b", &val));
	assert_int_equal(s.u, 2);
	assert_string_equal(s.name, "b");

	val = mSCRIPT_MAKE_F64(1.5);
	assert_true(mScriptObjectSet(&sval, "c", &val));
	assert_float_equal(s.f, 1.5, 0);
	assert_string_equal(s.name, "c");

	val = mSCRIPT_MAKE_CHARP("hello");
	assert_true(mScriptObjectSet(&sval, "d", &val));
	assert_string_equal(s.c, "hello");
	assert_string_equal(s.name, "d");

	val = mSCRIPT_MAKE_S32(3);
	assert_true(mScriptObjectSet(&sval, "a", &val));
	assert_int_equal(s.s, 3);

	val = mSCRIPT_MAKE_S16(4);
	assert_true(mScriptObjectSet(&sval, "a", &val));
	assert_int_equal(s.s, 4);

	val = mSCRIPT_MAKE_S8(5);
	assert_true(mScriptObjectSet(&sval, "a", &val));
	assert_int_equal(s.s, 5);

	val = mSCRIPT_MAKE_BOOL(false);
	assert_true(mScriptObjectSet(&sval, "a", &val));
	assert_int_equal(s.u, 0);

	pval = mScriptStringCreateFromASCII("goodbye");
	assert_true(mScriptObjectSet(&sval, "a", pval));
	assert_string_equal(s.c, "goodbye");
	mScriptValueDeref(pval);

	assert_true(cls->init);
	mScriptClassDeinit(cls);
	assert_false(cls->init);
}

M_TEST_DEFINE(testHSet) {
	struct mScriptTypeClass* cls = mSCRIPT_TYPE_MS_S(TestH)->details.cls;

	struct TestH s = {
		.i = 1,
		.j = 2,
	};

	struct mScriptValue sval = mSCRIPT_MAKE_S(TestH, &s);
	struct mScriptValue val;
	struct mScriptValue compare;

	compare = mSCRIPT_MAKE_S32(1);
	assert_true(mScriptObjectGet(&sval, "i", &val));
	assert_true(compare.type->equal(&compare, &val));

	compare = mSCRIPT_MAKE_S32(2);
	assert_true(mScriptObjectGet(&sval, "j", &val));
	assert_true(compare.type->equal(&compare, &val));

	val = mSCRIPT_MAKE_S32(3);
	assert_true(mScriptObjectSet(&sval, "i", &val));
	assert_int_equal(s.i, 3);

	val = mSCRIPT_MAKE_S32(4);
	assert_false(mScriptObjectSet(&sval, "j", &val));
	assert_int_equal(s.j, 2);

	compare = mSCRIPT_MAKE_S32(3);
	assert_true(mScriptObjectGet(&sval, "i", &val));
	assert_true(compare.type->equal(&compare, &val));

	compare = mSCRIPT_MAKE_S32(2);
	assert_true(mScriptObjectGet(&sval, "j", &val));
	assert_true(compare.type->equal(&compare, &val));

	assert_true(cls->init);
	mScriptClassDeinit(cls);
	assert_false(cls->init);
}

M_TEST_SUITE_DEFINE(mScriptClasses,
	cmocka_unit_test(testALayout),
	cmocka_unit_test(testASignatures),
	cmocka_unit_test(testAGet),
	cmocka_unit_test(testASet),
	cmocka_unit_test(testAStatic),
	cmocka_unit_test(testADynamic),
	cmocka_unit_test(testBLayout),
	cmocka_unit_test(testBGet),
	cmocka_unit_test(testBSet),
	cmocka_unit_test(testDLayout),
	cmocka_unit_test(testDGet),
	cmocka_unit_test(testDSet),
	cmocka_unit_test(testEGet),
	cmocka_unit_test(testFDeinit),
	cmocka_unit_test(testGSet),
	cmocka_unit_test(testHSet),
)
