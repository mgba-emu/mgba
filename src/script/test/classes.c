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
};

#define MEMBER_A_DOCSTRING "Member a"

mSCRIPT_DEFINE_STRUCT(TestA)
	mSCRIPT_DEFINE_DOCSTRING(MEMBER_A_DOCSTRING)
	mSCRIPT_DEFINE_STRUCT_MEMBER(struct TestA, S32, i)
	mSCRIPT_DEFINE_STRUCT_MEMBER(struct TestA, S32, i2)
	mSCRIPT_DEFINE_STRUCT_MEMBER(struct TestA, S8, b8)
	mSCRIPT_DEFINE_STRUCT_MEMBER(struct TestA, S16, hUnaligned)

	mSCRIPT_DEFINE_DOCSTRING(MEMBER_A_DOCSTRING)
	mSCRIPT_DEFINE_STATIC_MEMBER(S32, i)
	mSCRIPT_DEFINE_STATIC_MEMBER(S32, i2)
	mSCRIPT_DEFINE_STATIC_MEMBER(S8, b8)
	mSCRIPT_DEFINE_STATIC_MEMBER(S16, hUnaligned)
mSCRIPT_DEFINE_END

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
	member = HashTableLookup(&cls->staticMembers, "i");
	assert_non_null(member);
	assert_string_equal(member->name, "i");
	assert_string_equal(member->docstring, MEMBER_A_DOCSTRING);
	assert_ptr_equal(member->type, mSCRIPT_TYPE_MS_S32);
	assert_int_equal(member->offset, 0);

	member = HashTableLookup(&cls->staticMembers, "i2");
	assert_non_null(member);
	assert_string_equal(member->name, "i2");
	assert_null(member->docstring);
	assert_ptr_equal(member->type, mSCRIPT_TYPE_MS_S32);
	assert_int_equal(member->offset, sizeof(int32_t));

	member = HashTableLookup(&cls->staticMembers, "b8");
	assert_non_null(member);
	assert_string_equal(member->name, "b8");
	assert_null(member->docstring);
	assert_ptr_equal(member->type, mSCRIPT_TYPE_MS_S8);
	assert_int_equal(member->offset, sizeof(int32_t) * 2);

	member = HashTableLookup(&cls->staticMembers, "hUnaligned");
	assert_non_null(member);
	assert_string_equal(member->name, "hUnaligned");
	assert_null(member->docstring);
	assert_ptr_equal(member->type, mSCRIPT_TYPE_MS_S16);
	assert_int_not_equal(member->offset, sizeof(int32_t) * 2 + 1);

	member = HashTableLookup(&cls->staticMembers, "unknown");
	assert_null(member);

	mScriptClassDeinit(cls);
	assert_false(cls->init);
}

M_TEST_SUITE_DEFINE(mScriptClasses,
	cmocka_unit_test(testALayout))
