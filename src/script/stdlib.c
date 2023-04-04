/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/script/base.h>

#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba/core/version.h>
#include <mgba/script/context.h>
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

static uint32_t _mScriptCallbackAdd(struct mScriptCallbackManager* adapter, struct mScriptString* name, struct mScriptValue* fn) {
	if (fn->type->base == mSCRIPT_TYPE_WRAPPER) {
		fn = mScriptValueUnwrap(fn);
	}
	uint32_t id = mScriptContextAddCallback(adapter->context, name->buffer, fn);
	return id;
}

static uint32_t _mScriptCallbackOneshot(struct mScriptCallbackManager* adapter, struct mScriptString* name, struct mScriptValue* fn) {
	if (fn->type->base == mSCRIPT_TYPE_WRAPPER) {
		fn = mScriptValueUnwrap(fn);
	}
	uint32_t id = mScriptContextAddOneshot(adapter->context, name->buffer, fn);
	return id;
}

static void _mScriptCallbackRemove(struct mScriptCallbackManager* adapter, uint32_t id) {
	mScriptContextRemoveCallback(adapter->context, id);
}

mSCRIPT_DECLARE_STRUCT(mScriptCallbackManager);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptCallbackManager, U32, add, _mScriptCallbackAdd, 2, STR, callback, WRAPPER, function);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptCallbackManager, U32, oneshot, _mScriptCallbackOneshot, 2, STR, callback, WRAPPER, function);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptCallbackManager, remove, _mScriptCallbackRemove, 1, U32, cbid);

static uint64_t mScriptMakeBitmask(struct mScriptList* list) {
	size_t i;
	uint64_t mask = 0;
	for (i = 0; i < mScriptListSize(list); ++i) {
		struct mScriptValue bit;
		struct mScriptValue* value = mScriptListGetPointer(list, i);
		if (value->type->base == mSCRIPT_TYPE_WRAPPER) {
			value = mScriptValueUnwrap(value);
		}
		if (!mScriptCast(mSCRIPT_TYPE_MS_U64, value, &bit)) {
			continue;
		}
		mask |= 1ULL << bit.value.u64;
	}
	return mask;
}

static struct mScriptValue* mScriptExpandBitmask(uint64_t mask) {
	struct mScriptValue* list = mScriptValueAlloc(mSCRIPT_TYPE_MS_LIST);
	size_t i;
	for (i = 0; mask; ++i, mask >>= 1) {
		if (!(mask & 1)) {
			continue;
		}
		*mScriptListAppend(list->value.list) = mSCRIPT_MAKE_U32(i);
	}
	return list;
}

mSCRIPT_BIND_FUNCTION(mScriptMakeBitmask_Binding, U64, mScriptMakeBitmask, 1, LIST, bits);
mSCRIPT_BIND_FUNCTION(mScriptExpandBitmask_Binding, WLIST, mScriptExpandBitmask, 1, U64, mask);

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
	mSCRIPT_DEFINE_DOCSTRING("Add a callback of the named type. The returned id can be used to remove it later")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptCallbackManager, add)
	mSCRIPT_DEFINE_DOCSTRING("Add a one-shot callback of the named type that will be automatically removed after called. The returned id can be used to remove it early")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptCallbackManager, oneshot)
	mSCRIPT_DEFINE_DOCSTRING("Remove a callback with the previously retuned id")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptCallbackManager, remove)
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
	mScriptContextSetDocstring(context, "callbacks", "Singleton instance of struct::mScriptCallbackManager");

	mScriptContextExportConstants(context, "SAVESTATE", (struct mScriptKVPair[]) {
		mSCRIPT_CONSTANT_PAIR(SAVESTATE, SCREENSHOT),
		mSCRIPT_CONSTANT_PAIR(SAVESTATE, SAVEDATA),
		mSCRIPT_CONSTANT_PAIR(SAVESTATE, CHEATS),
		mSCRIPT_CONSTANT_PAIR(SAVESTATE, RTC),
		mSCRIPT_CONSTANT_PAIR(SAVESTATE, METADATA),
		mSCRIPT_CONSTANT_PAIR(SAVESTATE, ALL),
		mSCRIPT_KV_SENTINEL
	});
	mScriptContextExportConstants(context, "PLATFORM", (struct mScriptKVPair[]) {
		mSCRIPT_CONSTANT_PAIR(mPLATFORM, NONE),
		mSCRIPT_CONSTANT_PAIR(mPLATFORM, GBA),
		mSCRIPT_CONSTANT_PAIR(mPLATFORM, GB),
		mSCRIPT_KV_SENTINEL
	});
	mScriptContextExportConstants(context, "CHECKSUM", (struct mScriptKVPair[]) {
		mSCRIPT_CONSTANT_PAIR(mCHECKSUM, CRC32),
		mSCRIPT_KV_SENTINEL
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
		mSCRIPT_KV_SENTINEL
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
		mSCRIPT_KV_SENTINEL
	});
#endif
	mScriptContextSetGlobal(context, "C", context->constants);
	mScriptContextSetDocstring(context, "C", "A table containing the [exported constants](#constants)");

	mScriptContextExportNamespace(context, "util", (struct mScriptKVPair[]) {
		mSCRIPT_KV_PAIR(makeBitmask, &mScriptMakeBitmask_Binding),
		mSCRIPT_KV_PAIR(expandBitmask, &mScriptExpandBitmask_Binding),
		mSCRIPT_KV_SENTINEL
	});
	mScriptContextSetDocstring(context, "util", "Basic utility library");
	mScriptContextSetDocstring(context, "util.makeBitmask", "Compile a list of bit indices into a bitmask");
	mScriptContextSetDocstring(context, "util.expandBitmask", "Expand a bitmask into a list of bit indices");

	struct mScriptValue* systemVersion = mScriptStringCreateFromUTF8(projectVersion);
	struct mScriptValue* systemProgram = mScriptStringCreateFromUTF8(projectName);
	struct mScriptValue* systemBranch = mScriptStringCreateFromUTF8(gitBranch);
	struct mScriptValue* systemCommit = mScriptStringCreateFromUTF8(gitCommit);
	struct mScriptValue* systemRevision = mScriptValueAlloc(mSCRIPT_TYPE_MS_S32);
	systemRevision->value.s32 = gitRevision;

	mScriptContextExportNamespace(context, "system", (struct mScriptKVPair[]) {
		mSCRIPT_KV_PAIR(version, systemVersion),
		mSCRIPT_KV_PAIR(program, systemProgram),
		mSCRIPT_KV_PAIR(branch, systemBranch),
		mSCRIPT_KV_PAIR(commit, systemCommit),
		mSCRIPT_KV_PAIR(revision, systemRevision),
		mSCRIPT_KV_SENTINEL
	});

	mScriptContextSetDocstring(context, "system", "Information about the system the script is running under");
	mScriptContextSetDocstring(context, "system.version", "The current version of this build of the program");
	mScriptContextSetDocstring(context, "system.program", "The name of the program. Generally this will be \"mGBA\", but forks may change it to differentiate");
	mScriptContextSetDocstring(context, "system.branch", "The current git branch of this build of the program, if known");
	mScriptContextSetDocstring(context, "system.commit", "The current git commit hash of this build of the program, if known");
	mScriptContextSetDocstring(context, "system.revision", "The current git revision number of this build of the program, or -1 if unknown");
}
