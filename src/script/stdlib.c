/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/script/context.h>

#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba/script/macros.h>
#ifdef M_CORE_GBA
#include <mgba/internal/gba/input.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/input.h>
#endif

struct mScriptCallbackManager {
	struct mScriptContext* context;
};

static void _mScriptCallbackAdd(struct mScriptCallbackManager* adapter, struct mScriptString* name, struct mScriptValue* fn) {
	if (fn->type->base == mSCRIPT_TYPE_WRAPPER) {
		fn = mScriptValueUnwrap(fn);
	}
	mScriptContextAddCallback(adapter->context, name->buffer, fn);
	mScriptValueDeref(fn);
}

mSCRIPT_DECLARE_STRUCT(mScriptCallbackManager);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptCallbackManager, add, _mScriptCallbackAdd, 2, STR, callback, WRAPPER, function);

mSCRIPT_DEFINE_STRUCT(mScriptCallbackManager)
	mSCRIPT_DEFINE_CLASS_DOCSTRING(
		"A global singleton object `callbacks` used for managing callbacks. The following callbacks are defined:\n\n"
		"- **alarm**: An in-game alarm went off\n"
		"- **crashed**: The emulation crashed\n"
		"- **frame**: The emulation finished a frame\n"
		"- **keysRead**: The emulation is about to read the key input\n"
		"- **reset**: The emulation has been reset\n"
		"- **savedataUpdated**: The emulation has just finished modifying save data\n"
		"- **sleep**: The emulation has used the sleep feature to enter a low-power mode\n"
		"- **shutdown**: The emulation has been powered off\n"
		"- **start**: The emulation has started\n"
		"- **stop**: The emulation has voluntarily shut down\n"
	)
	mSCRIPT_DEFINE_DOCSTRING("Add a callback of the named type")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptCallbackManager, add)
mSCRIPT_DEFINE_END;

void mScriptContextAttachStdlib(struct mScriptContext* context) {
	struct mScriptValue* lib;

	lib = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mScriptCallbackManager));
	lib->value.opaque = calloc(1, sizeof(struct mScriptCallbackManager));
	*(struct mScriptCallbackManager*) lib->value.opaque = (struct mScriptCallbackManager) {
		.context = context
	};
	lib->flags = mSCRIPT_VALUE_FLAG_FREE_BUFFER;
	mScriptContextSetGlobal(context, "callbacks", lib);

	mScriptContextExportConstants(context, "SAVESTATE", (struct mScriptKVPair[]) {
		mSCRIPT_CONSTANT_PAIR(SAVESTATE, SCREENSHOT),
		mSCRIPT_CONSTANT_PAIR(SAVESTATE, SAVEDATA),
		mSCRIPT_CONSTANT_PAIR(SAVESTATE, CHEATS),
		mSCRIPT_CONSTANT_PAIR(SAVESTATE, RTC),
		mSCRIPT_CONSTANT_PAIR(SAVESTATE, METADATA),
		mSCRIPT_CONSTANT_PAIR(SAVESTATE, ALL),
		mSCRIPT_CONSTANT_SENTINEL
	});
	mScriptContextExportConstants(context, "PLATFORM", (struct mScriptKVPair[]) {
		mSCRIPT_CONSTANT_PAIR(mPLATFORM, NONE),
		mSCRIPT_CONSTANT_PAIR(mPLATFORM, GBA),
		mSCRIPT_CONSTANT_PAIR(mPLATFORM, GB),
		mSCRIPT_CONSTANT_SENTINEL
	});
	mScriptContextExportConstants(context, "CHECKSUM", (struct mScriptKVPair[]) {
		mSCRIPT_CONSTANT_PAIR(mCHECKSUM, CRC32),
		mSCRIPT_CONSTANT_SENTINEL
	});
#ifdef M_CORE_GBA
	mScriptContextExportConstants(context, "GBA_KEY", (struct mScriptKVPair[]) {
		mSCRIPT_CONSTANT_PAIR(GBA_KEY, A),
		mSCRIPT_CONSTANT_PAIR(GBA_KEY, B),
		mSCRIPT_CONSTANT_PAIR(GBA_KEY, SELECT),
		mSCRIPT_CONSTANT_PAIR(GBA_KEY, START),
		mSCRIPT_CONSTANT_PAIR(GBA_KEY, RIGHT),
		mSCRIPT_CONSTANT_PAIR(GBA_KEY, LEFT),
		mSCRIPT_CONSTANT_PAIR(GBA_KEY, UP),
		mSCRIPT_CONSTANT_PAIR(GBA_KEY, DOWN),
		mSCRIPT_CONSTANT_PAIR(GBA_KEY, R),
		mSCRIPT_CONSTANT_PAIR(GBA_KEY, L),
		mSCRIPT_CONSTANT_SENTINEL
	});
#endif
#ifdef M_CORE_GB
	mScriptContextExportConstants(context, "GB_KEY", (struct mScriptKVPair[]) {
		mSCRIPT_CONSTANT_PAIR(GB_KEY, A),
		mSCRIPT_CONSTANT_PAIR(GB_KEY, B),
		mSCRIPT_CONSTANT_PAIR(GB_KEY, SELECT),
		mSCRIPT_CONSTANT_PAIR(GB_KEY, START),
		mSCRIPT_CONSTANT_PAIR(GB_KEY, RIGHT),
		mSCRIPT_CONSTANT_PAIR(GB_KEY, LEFT),
		mSCRIPT_CONSTANT_PAIR(GB_KEY, UP),
		mSCRIPT_CONSTANT_PAIR(GB_KEY, DOWN),
		mSCRIPT_CONSTANT_SENTINEL
	});
#endif
	mScriptContextSetGlobal(context, "C", context->constants);
}
