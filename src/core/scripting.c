/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/scripting.h>

#include <mgba/core/core.h>
#include <mgba/script/context.h>
#include <mgba-util/table.h>
#include <mgba-util/vfs.h>

struct mScriptBridge {
	struct Table engines;
	struct mDebugger* debugger;
};

struct mScriptInfo {
	const char* name;
	struct VFile* vf;
	bool success;
};

struct mScriptSymbol {
	const char* name;
	int32_t* out;
	bool success;
};

static void _seDeinit(void* value) {
	struct mScriptEngine* se = value;
	se->deinit(se);
}

static void _seTryLoad(const char* key, void* value, void* user) {
	UNUSED(key);
	struct mScriptEngine* se = value;
	struct mScriptInfo* si = user;
	if (!si->success && se->isScript(se, si->name, si->vf)) {
		si->success = se->loadScript(se, si->name, si->vf);
	}
}

static void _seLookupSymbol(const char* key, void* value, void* user) {
	UNUSED(key);
	struct mScriptEngine* se = value;
	struct mScriptSymbol* si = user;
	if (!si->success) {
		si->success = se->lookupSymbol(se, si->name, si->out);
	}
}

static void _seRun(const char* key, void* value, void* user) {
	UNUSED(key);
	UNUSED(user);
	struct mScriptEngine* se = value;
	se->run(se);
}

#ifdef USE_DEBUGGERS
struct mScriptDebuggerEntry {
	enum mDebuggerEntryReason reason;
	struct mDebuggerEntryInfo* info;
};

static void _seDebuggerEnter(const char* key, void* value, void* user) {
	UNUSED(key);
	struct mScriptEngine* se = value;
	struct mScriptDebuggerEntry* entry = user;
	se->debuggerEntered(se, entry->reason, entry->info);
}
#endif

struct mScriptBridge* mScriptBridgeCreate(void) {
	struct mScriptBridge* sb = malloc(sizeof(*sb));
	HashTableInit(&sb->engines, 0, _seDeinit);
	sb->debugger = NULL;
	return sb;
}

void mScriptBridgeDestroy(struct mScriptBridge* sb) {
	HashTableDeinit(&sb->engines);
	free(sb);
}

void mScriptBridgeInstallEngine(struct mScriptBridge* sb, struct mScriptEngine* se) {
	if (!se->init(se, sb)) {
		return;
	}
	const char* name = se->name(se);
	HashTableInsert(&sb->engines, name, se);
}

#ifdef USE_DEBUGGERS
void mScriptBridgeSetDebugger(struct mScriptBridge* sb, struct mDebugger* debugger) {
	if (sb->debugger == debugger) {
		return;
	}
	if (sb->debugger) {
		sb->debugger->bridge = NULL;
	}
	sb->debugger = debugger;
	if (debugger) {
		debugger->bridge = sb;
	}
}

struct mDebugger* mScriptBridgeGetDebugger(struct mScriptBridge* sb) {
	return sb->debugger;
}

void mScriptBridgeDebuggerEntered(struct mScriptBridge* sb, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info) {
	struct mScriptDebuggerEntry entry = {
		.reason = reason,
		.info = info
	};
	HashTableEnumerate(&sb->engines, _seDebuggerEnter, &entry);
}
#endif

void mScriptBridgeRun(struct mScriptBridge* sb) {
	HashTableEnumerate(&sb->engines, _seRun, NULL);
}

bool mScriptBridgeLoadScript(struct mScriptBridge* sb, const char* name) {
	struct VFile* vf = VFileOpen(name, O_RDONLY);
	if (!vf) {
		return false;
	}
	struct mScriptInfo info = {
		.name = name,
		.vf = vf,
		.success = false
	};
	HashTableEnumerate(&sb->engines, _seTryLoad, &info);
	vf->close(vf);
	return info.success;
}

bool mScriptBridgeLookupSymbol(struct mScriptBridge* sb, const char* name, int32_t* out) {
	struct mScriptSymbol info = {
		.name = name,
		.out = out,
		.success = false
	};
	HashTableEnumerate(&sb->engines, _seLookupSymbol, &info);
	return info.success;
}

mSCRIPT_DECLARE_STRUCT_CD_METHOD(mCore, U32, frameCounter, 0);
mSCRIPT_DECLARE_STRUCT_CD_METHOD(mCore, S32, frameCycles, 0);
mSCRIPT_DECLARE_STRUCT_CD_METHOD(mCore, S32, frequency, 0);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mCore, runFrame, 0);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mCore, step, 0);

mSCRIPT_DECLARE_STRUCT_D_METHOD(mCore, U8, busRead8, 1, U32, address);
mSCRIPT_DECLARE_STRUCT_D_METHOD(mCore, U16, busRead16, 1, U32, address);
mSCRIPT_DECLARE_STRUCT_D_METHOD(mCore, U32, busRead32, 1, U32, address);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mCore, busWrite8, 2, U32, address, U8, value);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mCore, busWrite16, 2, U32, address, U16, value);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mCore, busWrite32, 2, U32, address, U32, value);

mSCRIPT_DEFINE_STRUCT(mCore)
mSCRIPT_DEFINE_DOCSTRING("Get the number of the current frame")
mSCRIPT_DEFINE_STRUCT_METHOD_NAMED(mCore, currentFrame, frameCounter)
mSCRIPT_DEFINE_DOCSTRING("Get the number of cycles per frame")
mSCRIPT_DEFINE_STRUCT_METHOD(mCore, frameCycles)
mSCRIPT_DEFINE_DOCSTRING("Get the number of cycles per second")
mSCRIPT_DEFINE_STRUCT_METHOD(mCore, frequency)
mSCRIPT_DEFINE_DOCSTRING("Run until the next frame")
mSCRIPT_DEFINE_STRUCT_METHOD(mCore, runFrame)
mSCRIPT_DEFINE_DOCSTRING("Run a single instruction")
mSCRIPT_DEFINE_STRUCT_METHOD(mCore, step)

mSCRIPT_DEFINE_DOCSTRING("Read an 8-bit value from the given bus address")
mSCRIPT_DEFINE_STRUCT_METHOD_NAMED(mCore, read8, busRead8)
mSCRIPT_DEFINE_DOCSTRING("Read a 16-bit value from the given bus address")
mSCRIPT_DEFINE_STRUCT_METHOD_NAMED(mCore, read16, busRead16)
mSCRIPT_DEFINE_DOCSTRING("Read a 32-bit value from the given bus address")
mSCRIPT_DEFINE_STRUCT_METHOD_NAMED(mCore, read32, busRead32)
mSCRIPT_DEFINE_DOCSTRING("Write an 8-bit value from the given bus address")
mSCRIPT_DEFINE_STRUCT_METHOD_NAMED(mCore, write8, busWrite8)
mSCRIPT_DEFINE_DOCSTRING("Write a 16-bit value from the given bus address")
mSCRIPT_DEFINE_STRUCT_METHOD_NAMED(mCore, write16, busWrite16)
mSCRIPT_DEFINE_DOCSTRING("Write a 32-bit value from the given bus address")
mSCRIPT_DEFINE_STRUCT_METHOD_NAMED(mCore, write32, busWrite32)
mSCRIPT_DEFINE_END;

void mScriptContextAttachCore(struct mScriptContext* context, struct mCore* core) {
	struct mScriptValue* coreValue = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mCore));
	coreValue->value.opaque = core;
	mScriptContextSetGlobal(context, "emu", coreValue);
	mScriptValueDeref(coreValue);
}

void mScriptContextDetachCore(struct mScriptContext* context) {
	mScriptContextRemoveGlobal(context, "emu");
}
