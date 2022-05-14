/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/scripting.h>

#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba/script/context.h>
#include <mgba-util/table.h>
#include <mgba-util/vfs.h>

mLOG_DEFINE_CATEGORY(SCRIPT, "Scripting", "script");

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

struct mScriptMemoryAdapter {
	struct mCore* core;
	struct mCoreMemoryBlock block;
};

struct mScriptCoreAdapter {
	struct mCore* core;
	struct mScriptContext* context;
	struct mScriptValue memory;
};

static uint32_t mScriptMemoryAdapterRead8(struct mScriptMemoryAdapter* adapter, uint32_t address) {
	uint32_t segmentSize = adapter->block.end - adapter->block.start;
	uint32_t segmentAddress = address % segmentSize;
	int segment = address / segmentSize;
	address = adapter->block.start + segmentAddress;
	return adapter->core->rawRead8(adapter->core, address, segment);
}

static uint32_t mScriptMemoryAdapterRead16(struct mScriptMemoryAdapter* adapter, uint32_t address) {
	uint32_t segmentSize = adapter->block.end - adapter->block.start;
	uint32_t segmentAddress = address % segmentSize;
	int segment = address / segmentSize;
	address = adapter->block.start + segmentAddress;
	return adapter->core->rawRead16(adapter->core, address, segment);
}

static uint32_t mScriptMemoryAdapterRead32(struct mScriptMemoryAdapter* adapter, uint32_t address) {
	uint32_t segmentSize = adapter->block.end - adapter->block.start;
	uint32_t segmentAddress = address % segmentSize;
	int segment = address / segmentSize;
	address = adapter->block.start + segmentAddress;
	return adapter->core->rawRead32(adapter->core, address, segment);
}

static void mScriptMemoryAdapterWrite8(struct mScriptMemoryAdapter* adapter, uint32_t address, uint8_t value) {
	uint32_t segmentSize = adapter->block.end - adapter->block.start;
	uint32_t segmentAddress = address % segmentSize;
	int segment = address / segmentSize;
	address = adapter->block.start + segmentAddress;
	adapter->core->rawWrite8(adapter->core, address, segment, value);
}

static void mScriptMemoryAdapterWrite16(struct mScriptMemoryAdapter* adapter, uint32_t address, uint16_t value) {
	uint32_t segmentSize = adapter->block.end - adapter->block.start;
	uint32_t segmentAddress = address % segmentSize;
	int segment = address / segmentSize;
	address = adapter->block.segmentStart + segmentAddress;
	adapter->core->rawWrite16(adapter->core, address, segment, value);
}

static void mScriptMemoryAdapterWrite32(struct mScriptMemoryAdapter* adapter, uint32_t address, uint32_t value) {
	uint32_t segmentSize = adapter->block.end - adapter->block.start;
	uint32_t segmentAddress = address % segmentSize;
	int segment = address / segmentSize;
	address = adapter->block.segmentStart + segmentAddress;
	adapter->core->rawWrite32(adapter->core, address, segment, value);
}

mSCRIPT_DECLARE_STRUCT(mScriptMemoryAdapter);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptMemoryAdapter, U32, read8, mScriptMemoryAdapterRead8, 1, U32, address);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptMemoryAdapter, U32, read16, mScriptMemoryAdapterRead16, 1, U32, address);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptMemoryAdapter, U32, read32, mScriptMemoryAdapterRead32, 1, U32, address);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptMemoryAdapter, write8, mScriptMemoryAdapterWrite8, 2, U32, address, U8, value);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptMemoryAdapter, write16, mScriptMemoryAdapterWrite16, 2, U32, address, U16, value);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptMemoryAdapter, write32, mScriptMemoryAdapterWrite32, 2, U32, address, U32, value);

mSCRIPT_DEFINE_STRUCT(mScriptMemoryAdapter)
mSCRIPT_DEFINE_DOCSTRING("Read an 8-bit value from the given offset")
mSCRIPT_DEFINE_STRUCT_METHOD(mScriptMemoryAdapter, read8)
mSCRIPT_DEFINE_DOCSTRING("Read a 16-bit value from the given offset")
mSCRIPT_DEFINE_STRUCT_METHOD(mScriptMemoryAdapter, read16)
mSCRIPT_DEFINE_DOCSTRING("Read a 32-bit value from the given offset")
mSCRIPT_DEFINE_STRUCT_METHOD(mScriptMemoryAdapter, read32)
mSCRIPT_DEFINE_DOCSTRING("Write an 8-bit value from the given offset")
mSCRIPT_DEFINE_STRUCT_METHOD(mScriptMemoryAdapter, write8)
mSCRIPT_DEFINE_DOCSTRING("Write a 16-bit value from the given offset")
mSCRIPT_DEFINE_STRUCT_METHOD(mScriptMemoryAdapter, write16)
mSCRIPT_DEFINE_DOCSTRING("Write a 32-bit value from the given offset")
mSCRIPT_DEFINE_STRUCT_METHOD(mScriptMemoryAdapter, write32)
mSCRIPT_DEFINE_END;

mSCRIPT_DECLARE_STRUCT_CD_METHOD(mCore, U32, frameCounter, 0);
mSCRIPT_DECLARE_STRUCT_CD_METHOD(mCore, S32, frameCycles, 0);
mSCRIPT_DECLARE_STRUCT_CD_METHOD(mCore, S32, frequency, 0);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mCore, runFrame, 0);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mCore, step, 0);

mSCRIPT_DECLARE_STRUCT_D_METHOD(mCore, U32, busRead8, 1, U32, address);
mSCRIPT_DECLARE_STRUCT_D_METHOD(mCore, U32, busRead16, 1, U32, address);
mSCRIPT_DECLARE_STRUCT_D_METHOD(mCore, U32, busRead32, 1, U32, address);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mCore, busWrite8, 2, U32, address, U8, value);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mCore, busWrite16, 2, U32, address, U16, value);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mCore, busWrite32, 2, U32, address, U32, value);

mSCRIPT_DECLARE_STRUCT_METHOD_WITH_DEFAULTS(mCore, S32, saveStateSlot, mCoreSaveState, 2, S32, slot, S32, flags);
mSCRIPT_DECLARE_STRUCT_METHOD_WITH_DEFAULTS(mCore, S32, loadStateSlot, mCoreLoadState, 2, S32, slot, S32, flags);

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

	mSCRIPT_DEFINE_DOCSTRING("Save state to the slot number")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, saveStateSlot)
	mSCRIPT_DEFINE_DOCSTRING("Load state from the slot number")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, loadStateSlot)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT_BINDING_DEFAULTS(mCore, saveStateSlot)
	mSCRIPT_NO_DEFAULT,
	mSCRIPT_MAKE_S32(SAVESTATE_ALL)
mSCRIPT_DEFINE_DEFAULTS_END;

mSCRIPT_DEFINE_STRUCT_BINDING_DEFAULTS(mCore, loadStateSlot)
	mSCRIPT_NO_DEFAULT,
	mSCRIPT_MAKE_S32(SAVESTATE_ALL & ~SAVESTATE_SAVEDATA)
mSCRIPT_DEFINE_DEFAULTS_END;

static void _clearMemoryMap(struct mScriptContext* context, struct mScriptCoreAdapter* adapter, bool clear) {
	struct TableIterator iter;
	if (mScriptTableIteratorStart(&adapter->memory, &iter)) {
		while (true) {
			struct mScriptValue* weakref = mScriptTableIteratorGetValue(&adapter->memory, &iter);
			if (weakref) {
				if (clear) {
					mScriptContextClearWeakref(context, weakref->value.s32);
				}
				mScriptValueDeref(weakref);
			}
			if (!mScriptTableIteratorNext(&adapter->memory, &iter)) {
				break;
			}
		}
	}
	mScriptTableClear(&adapter->memory);
}

static void _rebuildMemoryMap(struct mScriptContext* context, struct mScriptCoreAdapter* adapter) {
	_clearMemoryMap(context, adapter, true);

	const struct mCoreMemoryBlock* blocks;
	size_t nBlocks = adapter->core->listMemoryBlocks(adapter->core, &blocks);
	size_t i;
	for (i = 0; i < nBlocks; ++i) {
		struct mScriptMemoryAdapter* memadapter = calloc(1, sizeof(*memadapter));
		memadapter->core = adapter->core;
		memcpy(&memadapter->block, &blocks[i], sizeof(memadapter->block));
		struct mScriptValue* value = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mScriptMemoryAdapter));
		value->flags = mSCRIPT_VALUE_FLAG_FREE_BUFFER;
		value->value.opaque = memadapter;
		struct mScriptValue* key = mScriptStringCreateFromUTF8(blocks[i].internalName);
		mScriptTableInsert(&adapter->memory, key, mScriptContextMakeWeakref(context, value));
		mScriptValueDeref(key);
	}
}

static void _mScriptCoreAdapterDeinit(struct mScriptCoreAdapter* adapter) {
	_clearMemoryMap(adapter->context, adapter, false);
	adapter->memory.type->free(&adapter->memory);
}

static struct mScriptValue* _mScriptCoreAdapterGet(struct mScriptCoreAdapter* adapter, const char* name) {
	struct mScriptValue val;
	struct mScriptValue core = mSCRIPT_MAKE(S(mCore), adapter->core);
	if (!mScriptObjectGet(&core, name, &val)) {
		return NULL;
	}

	struct mScriptValue* ret = malloc(sizeof(*ret));
	memcpy(ret, &val, sizeof(*ret));
	ret->refs = 1;
	return ret;
}

mSCRIPT_DECLARE_STRUCT(mScriptCoreAdapter);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptCoreAdapter, WRAPPER, _get, _mScriptCoreAdapterGet, 1, CHARP, name);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptCoreAdapter, _deinit, _mScriptCoreAdapterDeinit, 0);

mSCRIPT_DEFINE_STRUCT(mScriptCoreAdapter)
mSCRIPT_DEFINE_STRUCT_MEMBER_NAMED(mScriptCoreAdapter, PS(mCore), _core, core)
mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptCoreAdapter, TABLE, memory)
mSCRIPT_DEFINE_STRUCT_DEINIT(mScriptCoreAdapter)
mSCRIPT_DEFINE_STRUCT_DEFAULT_GET(mScriptCoreAdapter)
mSCRIPT_DEFINE_STRUCT_CAST_TO_MEMBER(mScriptCoreAdapter, S(mCore), _core)
mSCRIPT_DEFINE_STRUCT_CAST_TO_MEMBER(mScriptCoreAdapter, CS(mCore), _core)
mSCRIPT_DEFINE_END;

void mScriptContextAttachCore(struct mScriptContext* context, struct mCore* core) {
	struct mScriptValue* coreValue = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mScriptCoreAdapter));
	struct mScriptCoreAdapter* adapter = calloc(1, sizeof(*adapter));
	adapter->core = core;
	adapter->context = context;

	adapter->memory.refs = mSCRIPT_VALUE_UNREF;
	adapter->memory.flags = 0;
	adapter->memory.type = mSCRIPT_TYPE_MS_TABLE;
	adapter->memory.type->alloc(&adapter->memory);

	_rebuildMemoryMap(context, adapter);

	coreValue->value.opaque = adapter;
	coreValue->flags = mSCRIPT_VALUE_FLAG_FREE_BUFFER;
	mScriptContextSetGlobal(context, "emu", coreValue);
}

void mScriptContextDetachCore(struct mScriptContext* context) {
	struct mScriptValue* value = HashTableLookup(&context->rootScope, "emu");
	if (!value) {
		return;
	}
	value = mScriptContextAccessWeakref(context, value);
	if (!value) {
		return;
	}
	_clearMemoryMap(context, value->value.opaque, true);
	mScriptContextRemoveGlobal(context, "emu");
}

void mScriptLog(struct mLogger* logger, struct mScriptString* msg) {
	mLogExplicit(logger, _mLOG_CAT_SCRIPT, mLOG_INFO, "%s", msg->buffer);
}

void mScriptWarn(struct mLogger* logger, struct mScriptString* msg) {
	mLogExplicit(logger, _mLOG_CAT_SCRIPT, mLOG_WARN, "%s", msg->buffer);
}

void mScriptError(struct mLogger* logger, struct mScriptString* msg) {
	mLogExplicit(logger, _mLOG_CAT_SCRIPT, mLOG_ERROR, "%s", msg->buffer);
}

mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mLogger, log, mScriptLog, 1, STR, msg);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mLogger, warn, mScriptWarn, 1, STR, msg);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mLogger, error, mScriptError, 1, STR, msg);

mSCRIPT_DEFINE_STRUCT(mLogger)
mSCRIPT_DEFINE_STRUCT_METHOD(mLogger, log)
mSCRIPT_DEFINE_STRUCT_METHOD(mLogger, warn)
mSCRIPT_DEFINE_STRUCT_METHOD(mLogger, error)
mSCRIPT_DEFINE_END;

void mScriptContextAttachLogger(struct mScriptContext* context, struct mLogger* logger) {
	struct mScriptValue* value = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mLogger));
	value->value.opaque = logger;
	mScriptContextSetGlobal(context, "console", value);
}

void mScriptContextDetachLogger(struct mScriptContext* context) {
	mScriptContextRemoveGlobal(context, "console");
}
