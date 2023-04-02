/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/scripting.h>

#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba/script/base.h>
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

struct mScriptMemoryDomain {
	struct mCore* core;
	struct mCoreMemoryBlock block;
};

struct mScriptCoreAdapter {
	struct mCore* core;
	struct mScriptContext* context;
	struct mScriptValue memory;
};

struct mScriptConsole {
	struct mLogger* logger;
	mScriptContextBufferFactory textBufferFactory;
	void* textBufferContext;
};

#define CALCULATE_SEGMENT_INFO \
	uint32_t segmentSize = adapter->block.end - adapter->block.start; \
	uint32_t segmentStart = adapter->block.segmentStart - adapter->block.start; \
	if (adapter->block.segmentStart) { \
		segmentSize -= segmentStart; \
	}

#define CALCULATE_SEGMENT_ADDRESS \
	uint32_t segmentAddress = address % segmentSize; \
	int segment = address / segmentSize; \
	segmentAddress += adapter->block.start; \
	if (adapter->block.segmentStart && segment) { \
		segmentAddress += segmentStart; \
	}

static uint32_t mScriptMemoryDomainRead8(struct mScriptMemoryDomain* adapter, uint32_t address) {
	CALCULATE_SEGMENT_INFO;
	CALCULATE_SEGMENT_ADDRESS;
	return adapter->core->rawRead8(adapter->core, segmentAddress, segment);
}

static uint32_t mScriptMemoryDomainRead16(struct mScriptMemoryDomain* adapter, uint32_t address) {
	CALCULATE_SEGMENT_INFO;
	CALCULATE_SEGMENT_ADDRESS;
	return adapter->core->rawRead16(adapter->core, segmentAddress, segment);
}

static uint32_t mScriptMemoryDomainRead32(struct mScriptMemoryDomain* adapter, uint32_t address) {
	CALCULATE_SEGMENT_INFO;
	CALCULATE_SEGMENT_ADDRESS;
	return adapter->core->rawRead32(adapter->core, segmentAddress, segment);
}

static struct mScriptValue* mScriptMemoryDomainReadRange(struct mScriptMemoryDomain* adapter, uint32_t address, uint32_t length) {
	CALCULATE_SEGMENT_INFO;
	struct mScriptValue* value = mScriptStringCreateEmpty(length);
	char* buffer = value->value.string->buffer;
	uint32_t i;
	for (i = 0; i < length; ++i, ++address) {
		CALCULATE_SEGMENT_ADDRESS;
		buffer[i] = adapter->core->rawRead8(adapter->core, segmentAddress, segment);
	}
	return value;
}

static void mScriptMemoryDomainWrite8(struct mScriptMemoryDomain* adapter, uint32_t address, uint8_t value) {
	CALCULATE_SEGMENT_INFO;
	CALCULATE_SEGMENT_ADDRESS;
	adapter->core->rawWrite8(adapter->core, segmentAddress, segment, value);
}

static void mScriptMemoryDomainWrite16(struct mScriptMemoryDomain* adapter, uint32_t address, uint16_t value) {
	CALCULATE_SEGMENT_INFO;
	CALCULATE_SEGMENT_ADDRESS;
	adapter->core->rawWrite16(adapter->core, segmentAddress, segment, value);
}

static void mScriptMemoryDomainWrite32(struct mScriptMemoryDomain* adapter, uint32_t address, uint32_t value) {
	CALCULATE_SEGMENT_INFO;
	CALCULATE_SEGMENT_ADDRESS;
	adapter->core->rawWrite32(adapter->core, segmentAddress, segment, value);
}

static uint32_t mScriptMemoryDomainBase(struct mScriptMemoryDomain* adapter) {
	return adapter->block.start;
}

static uint32_t mScriptMemoryDomainEnd(struct mScriptMemoryDomain* adapter) {
	return adapter->block.end;
}

static uint32_t mScriptMemoryDomainSize(struct mScriptMemoryDomain* adapter) {
	return adapter->block.size;
}

static struct mScriptValue* mScriptMemoryDomainName(struct mScriptMemoryDomain* adapter) {
	return mScriptStringCreateFromUTF8(adapter->block.shortName);
}

mSCRIPT_DECLARE_STRUCT(mScriptMemoryDomain);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptMemoryDomain, U32, read8, mScriptMemoryDomainRead8, 1, U32, address);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptMemoryDomain, U32, read16, mScriptMemoryDomainRead16, 1, U32, address);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptMemoryDomain, U32, read32, mScriptMemoryDomainRead32, 1, U32, address);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptMemoryDomain, WSTR, readRange, mScriptMemoryDomainReadRange, 2, U32, address, U32, length);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptMemoryDomain, write8, mScriptMemoryDomainWrite8, 2, U32, address, U8, value);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptMemoryDomain, write16, mScriptMemoryDomainWrite16, 2, U32, address, U16, value);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptMemoryDomain, write32, mScriptMemoryDomainWrite32, 2, U32, address, U32, value);

mSCRIPT_DECLARE_STRUCT_METHOD(mScriptMemoryDomain, U32, base, mScriptMemoryDomainBase, 0);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptMemoryDomain, U32, bound, mScriptMemoryDomainEnd, 0);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptMemoryDomain, U32, size, mScriptMemoryDomainSize, 0);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptMemoryDomain, WSTR, name, mScriptMemoryDomainName, 0);

mSCRIPT_DEFINE_STRUCT(mScriptMemoryDomain)
	mSCRIPT_DEFINE_CLASS_DOCSTRING(
		"An object used for access directly to a memory domain, e.g. the cartridge, "
		"instead of through a whole address space, as with the functions directly on struct::mCore."
	)
	mSCRIPT_DEFINE_DOCSTRING("Read an 8-bit value from the given offset")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptMemoryDomain, read8)
	mSCRIPT_DEFINE_DOCSTRING("Read a 16-bit value from the given offset")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptMemoryDomain, read16)
	mSCRIPT_DEFINE_DOCSTRING("Read a 32-bit value from the given offset")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptMemoryDomain, read32)
	mSCRIPT_DEFINE_DOCSTRING("Read byte range from the given offset")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptMemoryDomain, readRange)
	mSCRIPT_DEFINE_DOCSTRING("Write an 8-bit value from the given offset")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptMemoryDomain, write8)
	mSCRIPT_DEFINE_DOCSTRING("Write a 16-bit value from the given offset")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptMemoryDomain, write16)
	mSCRIPT_DEFINE_DOCSTRING("Write a 32-bit value from the given offset")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptMemoryDomain, write32)

	mSCRIPT_DEFINE_DOCSTRING("Get the address of the base of this memory domain")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptMemoryDomain, base)
	mSCRIPT_DEFINE_DOCSTRING("Get the address of the end bound of this memory domain. Note that this address is not in the domain itself, and is the address of the first byte past it")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptMemoryDomain, bound)
	mSCRIPT_DEFINE_DOCSTRING("Get the size of this memory domain in bytes")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptMemoryDomain, size)
	mSCRIPT_DEFINE_DOCSTRING("Get a short, human-readable name for this memory domain")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptMemoryDomain, name)
mSCRIPT_DEFINE_END;

static struct mScriptValue* _mScriptCoreGetGameTitle(const struct mCore* core) {
	char title[32] = {0};
	core->getGameTitle(core, title);
	return mScriptStringCreateFromASCII(title);
}

static struct mScriptValue* _mScriptCoreGetGameCode(const struct mCore* core) {
	char code[16] = {0};
	core->getGameCode(core, code);
	return mScriptStringCreateFromASCII(code);
}

static struct mScriptValue* _mScriptCoreChecksum(const struct mCore* core, int t) {
	enum mCoreChecksumType type = (enum mCoreChecksumType) t;
	size_t size = 0;
	switch (type) {
	case mCHECKSUM_CRC32:
		size = 4;
		break;
	}
	if (!size) {
		return &mScriptValueNull;
	}
	void* data = calloc(1, size);
	core->checksum(core, data, type);
	if (type == mCHECKSUM_CRC32) {
		// This checksum is endian-dependent...let's just make it big endian for Lua
		uint32_t* crc = data;
		STORE_32BE(*crc, 0, crc);
	}
	struct mScriptValue* ret = mScriptStringCreateFromBytes(data, size);
	free(data);
	return ret;
}

static void _mScriptCoreAddKey(struct mCore* core, int32_t key) {
	core->addKeys(core, 1 << key);
}

static void _mScriptCoreClearKey(struct mCore* core, int32_t key) {
	core->clearKeys(core, 1 << key);
}

static int32_t _mScriptCoreGetKey(struct mCore* core, int32_t key) {
	return (core->getKeys(core) >> key) & 1;
}

static struct mScriptValue* _mScriptCoreReadRange(struct mCore* core, uint32_t address, uint32_t length) {
	struct mScriptValue* value = mScriptStringCreateEmpty(length);
	char* buffer = value->value.string->buffer;
	uint32_t i;
	for (i = 0; i < length; ++i, ++address) {
		buffer[i] = core->busRead8(core, address);
	}
	return value;
}

static struct mScriptValue* _mScriptCoreReadRegister(const struct mCore* core, const char* regName) {
	int32_t out;
	if (!core->readRegister(core, regName, &out)) {
		return &mScriptValueNull;
	}
	struct mScriptValue* value = mScriptValueAlloc(mSCRIPT_TYPE_MS_S32);
	value->value.s32 = out;
	return value;
}

static void _mScriptCoreWriteRegister(struct mCore* core, const char* regName, int32_t in) {
	core->writeRegister(core, regName, &in);
}

static struct mScriptValue* _mScriptCoreSaveState(struct mCore* core, int32_t flags) {
	struct VFile* vf = VFileMemChunk(NULL, 0);
	if (!mCoreSaveStateNamed(core, vf, flags)) {
		vf->close(vf);
		return &mScriptValueNull;
	}
	void* buffer = vf->map(vf, vf->size(vf), MAP_READ);
	struct mScriptValue* value = mScriptStringCreateFromBytes(buffer, vf->size(vf));
	vf->close(vf);
	return value;
}

static int _mScriptCoreSaveStateFile(struct mCore* core, const char* path, int flags) {
	struct VFile* vf = VFileOpen(path, O_WRONLY | O_TRUNC | O_CREAT);
	if (!vf) {
		return false;
	}
	bool ok = mCoreSaveStateNamed(core, vf, flags);
	vf->close(vf);
	return ok;
}

static int32_t _mScriptCoreLoadState(struct mCore* core, struct mScriptString* buffer, int32_t flags) {
	struct VFile* vf = VFileFromConstMemory(buffer->buffer, buffer->size);
	int ret = mCoreLoadStateNamed(core, vf, flags);
	vf->close(vf);
	return ret;
}

static int _mScriptCoreLoadStateFile(struct mCore* core, const char* path, int flags) {
	struct VFile* vf = VFileOpen(path, O_RDONLY);
	if (!vf) {
		return false;
	}
	bool ok = mCoreLoadStateNamed(core, vf, flags);
	vf->close(vf);
	return ok;
}

static void _mScriptCoreTakeScreenshot(struct mCore* core, const char* filename) {
	if (filename) {
		struct VFile* vf = VFileOpen(filename, O_WRONLY | O_CREAT | O_TRUNC);
		if (!vf) {
			return;
		}
		mCoreTakeScreenshotVF(core, vf);
		vf->close(vf);
	} else {
		mCoreTakeScreenshot(core);
	}
}

static struct mScriptValue* _mScriptCoreTakeScreenshotToImage(struct mCore* core) {
	size_t stride;
	const void* pixels = 0;
	unsigned width, height;
	core->currentVideoSize(core, &width, &height);
	core->getPixels(core, &pixels, &stride);
	if (!pixels) {
		return NULL;
	}
#ifndef COLOR_16_BIT
	struct mImage* image = mImageCreateFromConstBuffer(width, height, stride, mCOLOR_XBGR8, pixels);
#elif COLOR_5_6_5
	struct mImage* image = mImageCreateFromConstBuffer(width, height, stride, mCOLOR_RGB565, pixels);
#else
	struct mImage* image = mImageCreateFromConstBuffer(width, height, stride, mCOLOR_BGR5, pixels);
#endif

	struct mScriptValue* result = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mImage));
	result->value.opaque = image;
	result->flags = mSCRIPT_VALUE_FLAG_DEINIT;
	return result;
}

// Loading functions
mSCRIPT_DECLARE_STRUCT_METHOD(mCore, BOOL, loadFile, mCoreLoadFile, 1, CHARP, path);
mSCRIPT_DECLARE_STRUCT_METHOD(mCore, BOOL, autoloadSave, mCoreAutoloadSave, 0);
mSCRIPT_DECLARE_STRUCT_METHOD(mCore, BOOL, loadSaveFile, mCoreLoadSaveFile, 2, CHARP, path, BOOL, temporary);

// Info functions
mSCRIPT_DECLARE_STRUCT_CD_METHOD(mCore, S32, platform, 0);
mSCRIPT_DECLARE_STRUCT_CD_METHOD(mCore, U32, frameCounter, 0);
mSCRIPT_DECLARE_STRUCT_CD_METHOD(mCore, S32, frameCycles, 0);
mSCRIPT_DECLARE_STRUCT_CD_METHOD(mCore, S32, frequency, 0);
mSCRIPT_DECLARE_STRUCT_C_METHOD(mCore, WSTR, getGameTitle, _mScriptCoreGetGameTitle, 0);
mSCRIPT_DECLARE_STRUCT_C_METHOD(mCore, WSTR, getGameCode, _mScriptCoreGetGameCode, 0);
mSCRIPT_DECLARE_STRUCT_CD_METHOD(mCore, S64, romSize, 0);
mSCRIPT_DECLARE_STRUCT_C_METHOD_WITH_DEFAULTS(mCore, WSTR, checksum, _mScriptCoreChecksum, 1, S32, type);

// Run functions
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mCore, reset, 0);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mCore, runFrame, 0);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mCore, step, 0);

// Key functions
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mCore, setKeys, 1, U32, keys);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mCore, addKeys, 1, U32, keys);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mCore, clearKeys, 1, U32, keys);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mCore, addKey, _mScriptCoreAddKey, 1, S32, key);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mCore, clearKey, _mScriptCoreClearKey, 1, S32, key);
mSCRIPT_DECLARE_STRUCT_METHOD(mCore, S32, getKey, _mScriptCoreGetKey, 1, S32, key);
mSCRIPT_DECLARE_STRUCT_D_METHOD(mCore, U32, getKeys, 0);

// Memory functions
mSCRIPT_DECLARE_STRUCT_D_METHOD(mCore, U32, busRead8, 1, U32, address);
mSCRIPT_DECLARE_STRUCT_D_METHOD(mCore, U32, busRead16, 1, U32, address);
mSCRIPT_DECLARE_STRUCT_D_METHOD(mCore, U32, busRead32, 1, U32, address);
mSCRIPT_DECLARE_STRUCT_METHOD(mCore, WSTR, readRange, _mScriptCoreReadRange, 2, U32, address, U32, length);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mCore, busWrite8, 2, U32, address, U8, value);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mCore, busWrite16, 2, U32, address, U16, value);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mCore, busWrite32, 2, U32, address, U32, value);

// Register functions
mSCRIPT_DECLARE_STRUCT_METHOD(mCore, WSTR, readRegister, _mScriptCoreReadRegister, 1, CHARP, regName);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mCore, writeRegister, _mScriptCoreWriteRegister, 2, CHARP, regName, S32, value);

// Savestate functions
mSCRIPT_DECLARE_STRUCT_METHOD_WITH_DEFAULTS(mCore, BOOL, saveStateSlot, mCoreSaveState, 2, S32, slot, S32, flags);
mSCRIPT_DECLARE_STRUCT_METHOD_WITH_DEFAULTS(mCore, WSTR, saveStateBuffer, _mScriptCoreSaveState, 1, S32, flags);
mSCRIPT_DECLARE_STRUCT_METHOD_WITH_DEFAULTS(mCore, BOOL, saveStateFile, _mScriptCoreSaveStateFile, 2, CHARP, path, S32, flags);
mSCRIPT_DECLARE_STRUCT_METHOD_WITH_DEFAULTS(mCore, BOOL, loadStateSlot, mCoreLoadState, 2, S32, slot, S32, flags);
mSCRIPT_DECLARE_STRUCT_METHOD_WITH_DEFAULTS(mCore, BOOL, loadStateBuffer, _mScriptCoreLoadState, 2, STR, buffer, S32, flags);
mSCRIPT_DECLARE_STRUCT_METHOD_WITH_DEFAULTS(mCore, BOOL, loadStateFile, _mScriptCoreLoadStateFile, 2, CHARP, path, S32, flags);

// Miscellaneous functions
mSCRIPT_DECLARE_STRUCT_VOID_METHOD_WITH_DEFAULTS(mCore, screenshot, _mScriptCoreTakeScreenshot, 1, CHARP, filename);
mSCRIPT_DECLARE_STRUCT_METHOD(mCore, W(mImage), screenshotToImage, _mScriptCoreTakeScreenshotToImage, 0);

mSCRIPT_DEFINE_STRUCT(mCore)
	mSCRIPT_DEFINE_CLASS_DOCSTRING(
		"An instance of an emulator core."
	)
	mSCRIPT_DEFINE_DOCSTRING("Load a ROM file into the current state of this core")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, loadFile)
	mSCRIPT_DEFINE_DOCSTRING("Load the save data associated with the currently loaded ROM file")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, autoloadSave)
	mSCRIPT_DEFINE_DOCSTRING("Load save data from the given path. If the `temporary` flag is set, the given save data will not be written back to disk")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, loadSaveFile)

	mSCRIPT_DEFINE_DOCSTRING("Get which platform is being emulated. See C.PLATFORM for possible values")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, platform)
	mSCRIPT_DEFINE_DOCSTRING("Get the number of the current frame")
	mSCRIPT_DEFINE_STRUCT_METHOD_NAMED(mCore, currentFrame, frameCounter)
	mSCRIPT_DEFINE_DOCSTRING("Get the number of cycles per frame")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, frameCycles)
	mSCRIPT_DEFINE_DOCSTRING("Get the number of cycles per second")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, frequency)
	mSCRIPT_DEFINE_DOCSTRING("Get the size of the loaded ROM")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, romSize)
	mSCRIPT_DEFINE_DOCSTRING("Get the checksum of the loaded ROM")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, checksum)

	mSCRIPT_DEFINE_DOCSTRING("Get internal title of the game from the ROM header")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, getGameTitle)
	mSCRIPT_DEFINE_DOCSTRING("Get internal product code for the game from the ROM header, if available")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, getGameCode)

	mSCRIPT_DEFINE_DOCSTRING("Reset the emulation. This does not invoke the **reset** callback")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, reset)
	mSCRIPT_DEFINE_DOCSTRING("Run until the next frame")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, runFrame)
	mSCRIPT_DEFINE_DOCSTRING("Run a single instruction")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, step)

	mSCRIPT_DEFINE_DOCSTRING("Set the currently active key list")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, setKeys)
	mSCRIPT_DEFINE_DOCSTRING("Add a single key to the currently active key list")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, addKey)
	mSCRIPT_DEFINE_DOCSTRING("Add a bitmask of keys to the currently active key list")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, addKeys)
	mSCRIPT_DEFINE_DOCSTRING("Remove a single key from the currently active key list")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, clearKey)
	mSCRIPT_DEFINE_DOCSTRING("Remove a bitmask of keys from the currently active key list")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, clearKeys)
	mSCRIPT_DEFINE_DOCSTRING("Get the active state of a given key")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, getKey)
	mSCRIPT_DEFINE_DOCSTRING("Get the currently active keys as a bitmask")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, getKeys)

	mSCRIPT_DEFINE_DOCSTRING("Read an 8-bit value from the given bus address")
	mSCRIPT_DEFINE_STRUCT_METHOD_NAMED(mCore, read8, busRead8)
	mSCRIPT_DEFINE_DOCSTRING("Read a 16-bit value from the given bus address")
	mSCRIPT_DEFINE_STRUCT_METHOD_NAMED(mCore, read16, busRead16)
	mSCRIPT_DEFINE_DOCSTRING("Read a 32-bit value from the given bus address")
	mSCRIPT_DEFINE_STRUCT_METHOD_NAMED(mCore, read32, busRead32)
	mSCRIPT_DEFINE_DOCSTRING("Read byte range from the given offset")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, readRange)
	mSCRIPT_DEFINE_DOCSTRING("Write an 8-bit value from the given bus address")
	mSCRIPT_DEFINE_STRUCT_METHOD_NAMED(mCore, write8, busWrite8)
	mSCRIPT_DEFINE_DOCSTRING("Write a 16-bit value from the given bus address")
	mSCRIPT_DEFINE_STRUCT_METHOD_NAMED(mCore, write16, busWrite16)
	mSCRIPT_DEFINE_DOCSTRING("Write a 32-bit value from the given bus address")
	mSCRIPT_DEFINE_STRUCT_METHOD_NAMED(mCore, write32, busWrite32)

	mSCRIPT_DEFINE_DOCSTRING("Read the value of the register with the given name")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, readRegister)
	mSCRIPT_DEFINE_DOCSTRING("Write the value of the register with the given name")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, writeRegister)

	mSCRIPT_DEFINE_DOCSTRING("Save state to the slot number. See C.SAVESTATE for possible values for `flags`")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, saveStateSlot)
	mSCRIPT_DEFINE_DOCSTRING("Save state and return as a buffer. See C.SAVESTATE for possible values for `flags`")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, saveStateBuffer)
	mSCRIPT_DEFINE_DOCSTRING("Save state to the given path. See C.SAVESTATE for possible values for `flags`")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, saveStateFile)
	mSCRIPT_DEFINE_DOCSTRING("Load state from the slot number. See C.SAVESTATE for possible values for `flags`")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, loadStateSlot)
	mSCRIPT_DEFINE_DOCSTRING("Load state from a buffer. See C.SAVESTATE for possible values for `flags`")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, loadStateBuffer)
	mSCRIPT_DEFINE_DOCSTRING("Load state from the given path. See C.SAVESTATE for possible values for `flags`")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, loadStateFile)

	mSCRIPT_DEFINE_DOCSTRING("Save a screenshot to a file")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, screenshot)
	mSCRIPT_DEFINE_DOCSTRING("Get a screenshot in an struct::mImage")
	mSCRIPT_DEFINE_STRUCT_METHOD(mCore, screenshotToImage)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT_BINDING_DEFAULTS(mCore, checksum)
	mSCRIPT_S32(mCHECKSUM_CRC32)
mSCRIPT_DEFINE_DEFAULTS_END;

mSCRIPT_DEFINE_STRUCT_BINDING_DEFAULTS(mCore, saveStateSlot)
	mSCRIPT_NO_DEFAULT,
	mSCRIPT_S32(SAVESTATE_ALL)
mSCRIPT_DEFINE_DEFAULTS_END;

mSCRIPT_DEFINE_STRUCT_BINDING_DEFAULTS(mCore, saveStateBuffer)
	mSCRIPT_S32(SAVESTATE_ALL)
mSCRIPT_DEFINE_DEFAULTS_END;

mSCRIPT_DEFINE_STRUCT_BINDING_DEFAULTS(mCore, saveStateFile)
	mSCRIPT_NO_DEFAULT,
	mSCRIPT_S32(SAVESTATE_ALL)
mSCRIPT_DEFINE_DEFAULTS_END;

mSCRIPT_DEFINE_STRUCT_BINDING_DEFAULTS(mCore, loadStateSlot)
	mSCRIPT_NO_DEFAULT,
	mSCRIPT_S32(SAVESTATE_ALL & ~SAVESTATE_SAVEDATA)
mSCRIPT_DEFINE_DEFAULTS_END;

mSCRIPT_DEFINE_STRUCT_BINDING_DEFAULTS(mCore, loadStateBuffer)
	mSCRIPT_NO_DEFAULT,
	mSCRIPT_S32(SAVESTATE_ALL & ~SAVESTATE_SAVEDATA)
mSCRIPT_DEFINE_DEFAULTS_END;

mSCRIPT_DEFINE_STRUCT_BINDING_DEFAULTS(mCore, loadStateFile)
	mSCRIPT_NO_DEFAULT,
	mSCRIPT_S32(SAVESTATE_ALL & ~SAVESTATE_SAVEDATA)
mSCRIPT_DEFINE_DEFAULTS_END;

mSCRIPT_DEFINE_STRUCT_BINDING_DEFAULTS(mCore, screenshot)
	mSCRIPT_CHARP(NULL)
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
		if (blocks[i].flags == mCORE_MEMORY_VIRTUAL) {
			continue;
		}
		struct mScriptMemoryDomain* memadapter = calloc(1, sizeof(*memadapter));
		memadapter->core = adapter->core;
		memcpy(&memadapter->block, &blocks[i], sizeof(memadapter->block));
		struct mScriptValue* value = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mScriptMemoryDomain));
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
		return &mScriptValueNull;
	}

	struct mScriptValue* ret = malloc(sizeof(*ret));
	memcpy(ret, &val, sizeof(*ret));
	ret->refs = 1;
	return ret;
}

static void _mScriptCoreAdapterReset(struct mScriptCoreAdapter* adapter) {
	adapter->core->reset(adapter->core);
	mScriptContextTriggerCallback(adapter->context, "reset", NULL);
}

mSCRIPT_DECLARE_STRUCT(mScriptCoreAdapter);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptCoreAdapter, W(mCore), _get, _mScriptCoreAdapterGet, 1, CHARP, name);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptCoreAdapter, _deinit, _mScriptCoreAdapterDeinit, 0);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptCoreAdapter, reset, _mScriptCoreAdapterReset, 0);

mSCRIPT_DEFINE_STRUCT(mScriptCoreAdapter)
	mSCRIPT_DEFINE_CLASS_DOCSTRING(
		"A wrapper around a struct::mCore object that exposes more functionality. "
		"It can be implicity cast to a Core object, and exposes the same methods. "
		"Please see the documentation on struct::mCore for details on those methods."
	)
	mSCRIPT_DEFINE_STRUCT_MEMBER_NAMED(mScriptCoreAdapter, PS(mCore), _core, core)
	mSCRIPT_DEFINE_DOCSTRING("A table containing a platform-specific set of struct::mScriptMemoryDomain objects")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptCoreAdapter, TABLE, memory)
	mSCRIPT_DEFINE_STRUCT_DEINIT(mScriptCoreAdapter)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_GET(mScriptCoreAdapter)
	mSCRIPT_DEFINE_DOCSTRING("Reset the emulation. As opposed to struct::mCore.reset, this version calls the **reset** callback")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptCoreAdapter, reset)
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

static struct mScriptTextBuffer* _mScriptConsoleCreateBuffer(struct mScriptConsole* lib, const char* name) {
	struct mScriptTextBuffer* buffer = lib->textBufferFactory(lib->textBufferContext);
	buffer->init(buffer, name);
	return buffer;
}

static void mScriptConsoleLog(struct mScriptConsole* console, struct mScriptString* msg) {
	if (console->logger) {
		mLogExplicit(console->logger, _mLOG_CAT_SCRIPT, mLOG_INFO, "%s", msg->buffer);
	} else {
		mLog(_mLOG_CAT_SCRIPT, mLOG_INFO, "%s", msg->buffer);
	}
}

static void mScriptConsoleWarn(struct mScriptConsole* console, struct mScriptString* msg) {
	if (console->logger) {
		mLogExplicit(console->logger, _mLOG_CAT_SCRIPT, mLOG_WARN, "%s", msg->buffer);
	} else {
		mLog(_mLOG_CAT_SCRIPT, mLOG_WARN, "%s", msg->buffer);
	}
}

static void mScriptConsoleError(struct mScriptConsole* console, struct mScriptString* msg) {
	if (console->logger) {
		mLogExplicit(console->logger, _mLOG_CAT_SCRIPT, mLOG_ERROR, "%s", msg->buffer);
	} else {
		mLog(_mLOG_CAT_SCRIPT, mLOG_WARN, "%s", msg->buffer);
	}
}

mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptConsole, log, mScriptConsoleLog, 1, STR, msg);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptConsole, warn, mScriptConsoleWarn, 1, STR, msg);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptConsole, error, mScriptConsoleError, 1, STR, msg);
mSCRIPT_DECLARE_STRUCT_METHOD_WITH_DEFAULTS(mScriptConsole, S(mScriptTextBuffer), createBuffer, _mScriptConsoleCreateBuffer, 1, CHARP, name);

mSCRIPT_DEFINE_STRUCT(mScriptConsole)
	mSCRIPT_DEFINE_CLASS_DOCSTRING(
		"A global singleton object `console` that can be used for presenting textual information to the user via a console."
	)
	mSCRIPT_DEFINE_DOCSTRING("Print a log to the console")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptConsole, log)
	mSCRIPT_DEFINE_DOCSTRING("Print a warning to the console")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptConsole, warn)
	mSCRIPT_DEFINE_DOCSTRING("Print an error to the console")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptConsole, error)
	mSCRIPT_DEFINE_DOCSTRING("Create a text buffer that can be used to display custom information")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptConsole, createBuffer)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT_BINDING_DEFAULTS(mScriptConsole, createBuffer)
	mSCRIPT_CHARP(NULL)
mSCRIPT_DEFINE_DEFAULTS_END;

static struct mScriptConsole* _ensureConsole(struct mScriptContext* context) {
	struct mScriptValue* value = mScriptContextGetGlobal(context, "console");
	if (value) {
		return value->value.opaque;
	}
	struct mScriptConsole* console = calloc(1, sizeof(*console));
	value = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mScriptConsole));
	value->value.opaque = console;
	value->flags = mSCRIPT_VALUE_FLAG_FREE_BUFFER;
	mScriptContextSetGlobal(context, "console", value);
	mScriptContextSetDocstring(context, "console", "Singleton instance of struct::mScriptConsole");
	return console;
}

void mScriptContextAttachLogger(struct mScriptContext* context, struct mLogger* logger) {
	struct mScriptConsole* console = _ensureConsole(context);
	console->logger = logger;
}

void mScriptContextDetachLogger(struct mScriptContext* context) {
	struct mScriptValue* value = mScriptContextGetGlobal(context, "console");
	if (!value) {
		return;
	}
	struct mScriptConsole* console = value->value.opaque;
	console->logger = mLogGetContext();
}

mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mScriptTextBuffer, deinit, 0);
mSCRIPT_DECLARE_STRUCT_CD_METHOD(mScriptTextBuffer, U32, getX, 0);
mSCRIPT_DECLARE_STRUCT_CD_METHOD(mScriptTextBuffer, U32, getY, 0);
mSCRIPT_DECLARE_STRUCT_CD_METHOD(mScriptTextBuffer, U32, cols, 0);
mSCRIPT_DECLARE_STRUCT_CD_METHOD(mScriptTextBuffer, U32, rows, 0);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mScriptTextBuffer, print, 1, CHARP, text);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mScriptTextBuffer, clear, 0);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mScriptTextBuffer, setSize, 2, U32, cols, U32, rows);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mScriptTextBuffer, moveCursor, 2, U32, x, U32, y);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mScriptTextBuffer, advance, 1, S32, adv);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mScriptTextBuffer, setName, 1, CHARP, name);

mSCRIPT_DEFINE_STRUCT(mScriptTextBuffer)
	mSCRIPT_DEFINE_CLASS_DOCSTRING(
		"An object that can be used to present texual data to the user. It is displayed monospaced, "
		"and text can be edited after sending by moving the cursor or clearing the buffer."
	)
	mSCRIPT_DEFINE_STRUCT_DEINIT_NAMED(mScriptTextBuffer, deinit)
	mSCRIPT_DEFINE_DOCSTRING("Get the current x position of the cursor")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptTextBuffer, getX)
	mSCRIPT_DEFINE_DOCSTRING("Get the current y position of the cursor")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptTextBuffer, getY)
	mSCRIPT_DEFINE_DOCSTRING("Get number of columns in the buffer")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptTextBuffer, cols)
	mSCRIPT_DEFINE_DOCSTRING("Get number of rows in the buffer")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptTextBuffer, rows)
	mSCRIPT_DEFINE_DOCSTRING("Print a string to the buffer")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptTextBuffer, print)
	mSCRIPT_DEFINE_DOCSTRING("Clear the buffer")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptTextBuffer, clear)
	mSCRIPT_DEFINE_DOCSTRING("Set the number of rows and columns")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptTextBuffer, setSize)
	mSCRIPT_DEFINE_DOCSTRING("Set the position of the cursor")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptTextBuffer, moveCursor)
	mSCRIPT_DEFINE_DOCSTRING("Advance the cursor a number of columns")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptTextBuffer, advance)
	mSCRIPT_DEFINE_DOCSTRING("Set the user-visible name of this buffer")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptTextBuffer, setName)
mSCRIPT_DEFINE_END;

void mScriptContextSetTextBufferFactory(struct mScriptContext* context, mScriptContextBufferFactory factory, void* cbContext) {
	struct mScriptConsole* console = _ensureConsole(context);
	console->textBufferFactory = factory;
	console->textBufferContext = cbContext;
}
