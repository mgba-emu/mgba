/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/script/context.h>

struct mScriptCallbackAdapter {
	struct mScriptContext* context;
};

static void _mScriptCallbackAdd(struct mScriptCallbackAdapter* adapter, struct mScriptString* name, struct mScriptValue* fn) {
	if (fn->type->base == mSCRIPT_TYPE_WRAPPER) {
		fn = mScriptValueUnwrap(fn);
	}
	mScriptContextAddCallback(adapter->context, name->buffer, fn);
}

mSCRIPT_DECLARE_STRUCT(mScriptCallbackAdapter);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptCallbackAdapter, add, _mScriptCallbackAdd, 2, STR, callback, WRAPPER, function);

mSCRIPT_DEFINE_STRUCT(mScriptCallbackAdapter)
mSCRIPT_DEFINE_DOCSTRING("Add a callback of the named type")
mSCRIPT_DEFINE_STRUCT_METHOD(mScriptCallbackAdapter, add)
mSCRIPT_DEFINE_END;

void mScriptContextAttachStdlib(struct mScriptContext* context) {
	struct mScriptValue* lib;

	lib = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mScriptCallbackAdapter));
	lib->value.opaque = calloc(1, sizeof(struct mScriptCallbackAdapter));
	*(struct mScriptCallbackAdapter*) lib->value.opaque = (struct mScriptCallbackAdapter) {
		.context = context
	};
	lib->flags = mSCRIPT_VALUE_FLAG_FREE_BUFFER;
	mScriptContextSetGlobal(context, "callbacks", lib);
}
