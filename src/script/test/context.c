/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/script.h>

M_TEST_DEFINE(weakrefBasic) {
	struct mScriptContext context;
	mScriptContextInit(&context);

	struct mScriptValue weakref = mSCRIPT_VAL(WEAKREF, 1);
	struct mScriptValue fakeVal = mSCRIPT_S32(0x7E57CA5E);
	struct mScriptValue* val;

	assert_int_equal(TableSize(&context.weakrefs), 0);
	assert_null(TableLookup(&context.weakrefs, 1));
	assert_int_equal(context.nextWeakref, 1);
	assert_null(mScriptContextAccessWeakref(&context, &weakref));

	assert_int_equal(mScriptContextSetWeakref(&context, &fakeVal), 1);
	assert_int_equal(context.nextWeakref, 2);
	assert_int_equal(TableSize(&context.weakrefs), 1);
	val = mScriptContextAccessWeakref(&context, &weakref);
	assert_non_null(val);
	assert_int_equal(val->value.u32, 0x7E57CA5E);

	mScriptContextClearWeakref(&context, 1);

	assert_int_equal(TableSize(&context.weakrefs), 0);
	assert_null(TableLookup(&context.weakrefs, 1));
	assert_int_equal(context.nextWeakref, 2);
	assert_null(mScriptContextAccessWeakref(&context, &weakref));

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(drainPool) {
	struct mScriptContext context;
	mScriptContextInit(&context);

	assert_int_equal(mScriptListSize(&context.refPool), 0);

	struct mScriptValue fakeVal = mSCRIPT_CHARP("foo");
	fakeVal.refs = 2;

	mScriptContextFillPool(&context, &fakeVal);
	assert_int_equal(mScriptListSize(&context.refPool), 1);
	assert_int_equal(fakeVal.refs, 2);

	mScriptContextDrainPool(&context);
	assert_int_equal(mScriptListSize(&context.refPool), 0);
	assert_int_equal(fakeVal.refs, 1);

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(disownWeakref) {
	struct mScriptContext context;
	mScriptContextInit(&context);

	struct mScriptValue weakref = mSCRIPT_VAL(WEAKREF, 1);
	struct mScriptValue fakeVal = mSCRIPT_S32(0x7E57CA5E);
	struct mScriptValue* val;

	assert_int_equal(mScriptListSize(&context.refPool), 0);
	assert_int_equal(TableSize(&context.weakrefs), 0);
	assert_null(TableLookup(&context.weakrefs, 1));
	assert_int_equal(context.nextWeakref, 1);
	assert_null(mScriptContextAccessWeakref(&context, &weakref));

	assert_int_equal(mScriptContextSetWeakref(&context, &fakeVal), 1);
	assert_int_equal(TableSize(&context.weakrefs), 1);
	assert_int_equal(context.nextWeakref, 2);
	val = mScriptContextAccessWeakref(&context, &weakref);
	assert_non_null(val);
	assert_int_equal(val->value.u32, 0x7E57CA5E);

	mScriptContextDisownWeakref(&context, 1);
	assert_int_equal(mScriptListSize(&context.refPool), 1);
	assert_int_equal(TableSize(&context.weakrefs), 1);
	val = mScriptContextAccessWeakref(&context, &weakref);
	assert_non_null(val);
	assert_int_equal(val->value.u32, 0x7E57CA5E);

	mScriptContextDrainPool(&context);
	assert_int_equal(mScriptListSize(&context.refPool), 0);
	assert_int_equal(TableSize(&context.weakrefs), 0);
	assert_null(TableLookup(&context.weakrefs, 1));
	assert_null(mScriptContextAccessWeakref(&context, &weakref));

	mScriptContextDeinit(&context);
}

M_TEST_SUITE_DEFINE(mScript,
	cmocka_unit_test(weakrefBasic),
	cmocka_unit_test(drainPool),
	cmocka_unit_test(disownWeakref),
)
